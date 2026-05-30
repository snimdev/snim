#!/usr/bin/env bash
# Signs Flatpak bundles and publishes the Snim Flatpak repository.
#
#   flatpak-repo.sh bundle <build-repo> <arch> <out.flatpak>
#
# FLATPAK_GPG_KEY_ID names a secret key already in GNUPGHOME.
set -euo pipefail

APP_ID=dev.snim.Snim
BRANCH=master
DL_BASE_URL="${DL_BASE_URL:-https://dl.snim.dev}"
RUNTIME_REPO=https://dl.flathub.org/repo/flathub.flatpakrepo

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
  [ $# -eq 3 ] || die "usage: bundle <build-repo> <arch> <out.flatpak>"
  local repo=$1 arch=$2 out=$3 pubkey
  pubkey=$(mktemp)
  export_public_key "$pubkey"
  # A bundle carrying a key installs only if its commit is signed by it.
  flatpak build-sign "${SIGN[@]}" --arch="$arch" "$repo" "$APP_ID" "$BRANCH"
  # The key also makes the bundle's origin remote verify updates from the repository.
  flatpak build-bundle --gpg-keys="$pubkey" \
    --arch="$arch" \
    --repo-url="$DL_BASE_URL/flatpak/repo" \
    --runtime-repo="$RUNTIME_REPO" \
    "$repo" "$out" "$APP_ID" "$BRANCH"
  rm -f "$pubkey"
}

case "${1:-}" in
  bundle) shift; cmd_bundle "$@" ;;
  *) die "usage: $0 bundle ..." ;;
esac
