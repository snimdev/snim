#!/usr/bin/env bash
#
# Prints GitHub release notes for a tag on stdout. The release workflow pipes this
# into `gh release create --notes-file`.
#
# Commit subjects follow `Area: Imperative description`, so they group into one
# `## Area` section each; anything without a prefix lands under General. A prerelease
# diffs against the tag before it, a stable release against the last stable one. The
# first release has no such tag, so it gets a curated feature list instead of the
# entire history.
#
# Usage: scripts/changelog.sh <tag>
#   SNIM_CHANGELOG_PREV=<tag>  overrides the detected previous tag (for testing).

set -euo pipefail

TAG="${1:?usage: changelog.sh <tag>}"
REPO_URL="https://github.com/snimdev/snim"

PREV="${SNIM_CHANGELOG_PREV-}"
if [ -z "$PREV" ]; then
    case "$TAG" in
        *-*) PREV="$(git describe --tags --abbrev=0 --match 'v*' "$TAG^" 2>/dev/null || true)" ;;
        *) PREV="$(git describe --tags --abbrev=0 --match 'v*' --exclude 'v*-*' "$TAG^" 2>/dev/null || true)" ;;
    esac
fi

emit_footer() {
    cat <<'EOF'

## Installing

**macOS**: the app is not notarized yet, so Gatekeeper blocks the first launch.
Right-click `Snim.app` and choose **Open**, or clear the quarantine flag:

```bash
xattr -cr /Applications/Snim.app
```

The DMG is Apple Silicon only (arm64); there is no Intel build.

**Windows** (Windows 10 2004 or newer, x64): the recommended `-setup.exe`
installs for your user alone, without admin rights. The portable `.zip` runs from any
folder and keeps its settings in the `snim.ini` beside `snim.exe`. Or install from
PowerShell:

```powershell
irm https://snim.dev/install.ps1 | iex
```

The builds are not code-signed yet, so SmartScreen warns on the first launch: click
**More info**, then **Run anyway**.

EOF

    sed "s/@VERSION@/${TAG#v}/g" <<'EOF'

**Linux**: on Ubuntu or Debian, install the `.deb`:

```bash
sudo apt install ./snim_@VERSION@_amd64.deb
```

On Fedora, the `.rpm`:

```bash
sudo dnf install ./snim-@VERSION@.x86_64.rpm
```

On ARM, take the `arm64` `.deb` or the `aarch64` `.rpm` instead. Any distribution can
install the Flatpak from our own repository:

```bash
flatpak install https://dl.snim.dev/flatpak/snim.flatpakref
```

Elsewhere, extract the portable `Snim-x86_64.tar.gz` or `Snim-aarch64.tar.gz` anywhere
and run `usr/bin/snim`. The packages and the tarball run on glibc 2.35 or newer on
x86_64 (Ubuntu 22.04, Debian 12) and glibc 2.38 or newer on aarch64 (Ubuntu 24.04,
Debian 13).

Screen recording on Linux goes through the desktop portal, so it needs PipeWire and
`xdg-desktop-portal` (plus your compositor's backend) installed and running.

Recording uses your distribution's own GStreamer. The packages pull in its plugins; with
the tarball, install `gstreamer1.0-plugins-good`, `gstreamer1.0-plugins-bad` and
`gstreamer1.0-pipewire` (Debian, Ubuntu) or `gstreamer1-plugins-good`,
`gstreamer1-plugins-bad-free`, `gstreamer1-plugin-openh264` and `pipewire-gstreamer` (Fedora).
EOF

    # A hyphen in the tag means a prerelease (v1.0.0-beta.1), never a plain release.
    case "$TAG" in
        *-*)
            printf '\nThis is a prerelease, published for early testing: expect rough edges.\n'
            printf '\nThe PowerShell one-liner installs the latest stable release; for this prerelease, run:\n\n'
            printf '```powershell\n& ([scriptblock]::Create((irm https://snim.dev/install.ps1))) -Alpha\n```\n'
            ;;
    esac
}

if [ -z "$PREV" ]; then
    cat <<EOF
Snim $TAG, the first release.

## Features

- **Annotation editor** with arrows, rectangles, ellipses, freehand, text, highlight,
  blur and step numbers, plus beautify backdrops (padding, rounded corners, shadow)
  and saved presets.
- **Screen recording** with a trim editor, GIF export and an optional webcam bubble.
- **OCR text snip** that copies recognized text straight to the clipboard.
- **Uploads** to S3-compatible storage, SFTP and FTP, with named destination profiles.
- **Global hotkeys** for every capture action, bound to the same actions as the tray menu.
- **KDE fast-path screenshots** via KWin's ScreenShot2 interface, so captures skip the
  portal dialog entirely.
EOF
    emit_footer
    exit 0
fi

RANGE="$PREV..$TAG"

SUBJECTS="$(mktemp)"
trap 'rm -f "$SUBJECTS"' EXIT
git log --no-merges --format='%s' "$RANGE" > "$SUBJECTS"

# `Area: ` prefixes only; a colon inside a sentence must not become a heading.
PREFIX_RE='^[A-Z][A-Za-z0-9+/ -]*: '

printf 'Snim %s.\n' "$TAG"

AREAS="$(grep -E "$PREFIX_RE" "$SUBJECTS" | sed 's/: .*//' | sort -u || true)"

while IFS= read -r area; do
    [ -n "$area" ] || continue
    printf '\n## %s\n\n' "$area"
    sed -n "s/^${area}: //p" "$SUBJECTS" | sed 's/^/- /'
done <<EOF
$AREAS
EOF

GENERAL="$(grep -Ev "$PREFIX_RE" "$SUBJECTS" || true)"
if [ -n "$GENERAL" ]; then
    printf '\n## General\n\n'
    printf '%s\n' "$GENERAL" | sed 's/^/- /'
fi

if [ ! -s "$SUBJECTS" ]; then
    printf '\nNo commits since %s.\n' "$PREV"
fi

printf '\n**Full changelog**: %s/compare/%s...%s\n' "$REPO_URL" "$PREV" "$TAG"

emit_footer
