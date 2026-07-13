#!/usr/bin/env bash
# Signs Flatpak bundles and publishes the Snim Flatpak repository.
#
#   flatpak-repo.sh bundle <build-repo> <arch> <out.flatpak> <repo|test-repo>
#   flatpak-repo.sh publish <rclone-dir> <repo|test-repo> <bundle>...
#
# FLATPAK_GPG_KEY_ID names a secret key already in GNUPGHOME. <rclone-dir> is what
# $DL_BASE_URL/flatpak serves (r2:<bucket>/flatpak), or a local directory for a rehearsal.
set -euo pipefail

APP_ID=dev.snim.Snim
BRANCH=master
DL_BASE_URL="${DL_BASE_URL:-https://dl.snim.dev}"
RUNTIME_REPO=https://dl.flathub.org/repo/flathub.flatpakrepo
IMMUTABLE="Cache-Control: public, max-age=31536000, immutable"
FRESH="Cache-Control: no-cache"

die() { echo "flatpak-repo: $*" >&2; exit 1; }

: "${FLATPAK_GPG_KEY_ID:?set FLATPAK_GPG_KEY_ID}"
: "${GNUPGHOME:?set GNUPGHOME to the keyring holding the signing key}"
gpg --batch --list-secret-keys "$FLATPAK_GPG_KEY_ID" > /dev/null \
  || die "no secret key $FLATPAK_GPG_KEY_ID in $GNUPGHOME"
SIGN=(--gpg-sign="$FLATPAK_GPG_KEY_ID" --gpg-homedir="$GNUPGHOME")

export_public_key() {
  gpg --batch --export "$FLATPAK_GPG_KEY_ID" > "$1"
  [ -s "$1" ] || die "could not export the public key"
}

cmd_bundle() {
  [ $# -eq 4 ] || die "usage: bundle <build-repo> <arch> <out.flatpak> <repo|test-repo>"
  local repo=$1 arch=$2 out=$3 name=$4 pubkey
  case "$name" in
    repo|test-repo) ;;
    *) die "the repository is either repo or test-repo, not $name" ;;
  esac
  pubkey=$(mktemp)
  export_public_key "$pubkey"
  # A bundle carrying a key installs only if its commit is signed by it.
  flatpak build-sign "${SIGN[@]}" --arch="$arch" "$repo" "$APP_ID" "$BRANCH"
  # The key also makes the bundle's origin remote verify updates from the repository.
  flatpak build-bundle --gpg-keys="$pubkey" \
    --arch="$arch" \
    --repo-url="$DL_BASE_URL/flatpak/$name" \
    --runtime-repo="$RUNTIME_REPO" \
    "$repo" "$out" "$APP_ID" "$BRANCH"
  rm -f "$pubkey"
}

cmd_publish() {
  [ $# -ge 3 ] || die "usage: publish <rclone-dir> <repo|test-repo> <bundle>..."
  local base=$1 name=$2 stem title
  shift 2
  case "$name" in
    repo) stem=snim; title=Snim ;;
    test-repo) stem=snim-test; title="Snim (test)" ;;
    *) die "the repository is either repo or test-repo, not $name" ;;
  esac
  local dest="$base/$name" url="$DL_BASE_URL/flatpak/$name/" work repo
  work=$(mktemp -d)
  repo="$work/repo"

  # R2 reports success for missing objects, so list instead; a failed listing aborts.
  local listing
  listing=$(rclone lsf --files-only --max-depth 1 "$dest/") || die "could not list $dest"
  if grep -qx config <<<"$listing"; then
    echo "Pulling $dest"
    rclone copy "$dest" "$repo" --fast-list --exclude '/tmp/**'
    # Object storage keeps no empty directories, and ostree expects these.
    mkdir -p "$repo"/{objects,refs/heads,refs/mirrors,refs/remotes,state,tmp,extensions}
  else
    echo "No repository at $dest yet, starting one"
    ostree init --mode=archive-z2 --repo="$repo"
  fi

  local bundle
  for bundle in "$@"; do
    flatpak build-import-bundle "${SIGN[@]}" "$repo" "$bundle"
  done
  flatpak build-update-repo "${SIGN[@]}" \
    --title="$title" --homepage=https://snim.dev \
    --generate-static-deltas --prune --prune-depth=3 \
    "$repo"

  # Clients read summary, then refs, then objects: upload in the reverse order.
  rclone copy "$repo/objects" "$dest/objects" --fast-list --ignore-existing \
    --exclude '*.commitmeta' --header-upload "$IMMUTABLE"
  if [ -d "$repo/deltas" ]; then
    rclone copy "$repo/deltas" "$dest/deltas" --fast-list --ignore-existing \
      --header-upload "$IMMUTABLE"
  fi
  # Commit signatures, delta indexes and indexed summaries, all of which can change.
  rclone copy "$repo" "$dest" --fast-list --header-upload "$FRESH" \
    --filter '- /tmp/**' --filter '- /.lock' --filter '- /summary*' --filter '- /refs/**' \
    --filter '- /config' --filter '+ /objects/**.commitmeta' --filter '- /objects/**' \
    --filter '- /deltas/**'
  rclone copy "$repo" "$dest" --fast-list --header-upload "$FRESH" \
    --filter '+ /refs/**' --filter '+ /config' --filter '- **'
  rclone copy "$repo" "$dest" --fast-list --header-upload "$FRESH" \
    --filter '+ /summary*' --filter '- **'
  # Everything new is up by now, so this only deletes what the prune dropped.
  rclone sync "$repo" "$dest" --fast-list --size-only --header-upload "$FRESH" \
    --filter '- /tmp/**' --filter '- /.lock'

  local key
  key=$(gpg --batch --export "$FLATPAK_GPG_KEY_ID" | base64 -w0)
  [ -n "$key" ] || die "could not export the public key"
  cat > "$work/$stem.flatpakrepo" <<END
[Flatpak Repo]
Title=$title
Url=$url
Homepage=https://snim.dev
Comment=Screenshots, recordings, OCR and uploads
GPGKey=$key
END
  cat > "$work/$stem.flatpakref" <<END
[Flatpak Ref]
Name=$APP_ID
Branch=$BRANCH
Title=$title
Url=$url
SuggestRemoteName=$stem
Homepage=https://snim.dev
IsRuntime=false
RuntimeRepo=$RUNTIME_REPO
GPGKey=$key
END
  rclone copyto "$work/$stem.flatpakrepo" "$base/$stem.flatpakrepo" \
    --header-upload "$FRESH" --header-upload "Content-Type: application/vnd.flatpak.repo"
  rclone copyto "$work/$stem.flatpakref" "$base/$stem.flatpakref" \
    --header-upload "$FRESH" --header-upload "Content-Type: application/vnd.flatpak.ref"
  rm -rf "$work"
  echo "Published $url"
}

case "${1:-}" in
  bundle) shift; cmd_bundle "$@" ;;
  publish) shift; cmd_publish "$@" ;;
  *) die "usage: $0 bundle|publish ..." ;;
esac
