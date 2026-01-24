#!/usr/bin/env bash
#
# Prints GitHub release notes for a tag on stdout. The release workflow pipes this
# into `gh release create --notes-file`.
#
# Commit subjects follow `Area: Imperative description`, so they group into one
# `## Area` section each; anything without a prefix lands under General. The first
# release has no previous tag to diff against, so it gets a curated feature list
# instead of the entire history.
#
# Usage: scripts/changelog.sh <tag>
#   SNIM_CHANGELOG_PREV=<tag>  overrides the detected previous tag (for testing).

set -euo pipefail

TAG="${1:?usage: changelog.sh <tag>}"
REPO_URL="https://github.com/snimdev/snim"

PREV="${SNIM_CHANGELOG_PREV-}"
if [ -z "$PREV" ]; then
    # --match keeps the rolling `alpha` release tag out of the diff base.
    PREV="$(git describe --tags --abbrev=0 --match 'v*' "$TAG^" 2>/dev/null || true)"
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

**Linux**: the AppImage is self-updating. Point
[AppImageUpdate](https://github.com/AppImageCommunity/AppImageUpdate) at it and it
pulls the next release from the embedded zsync info, no reinstall needed.

Screen recording on Linux goes through the desktop portal, so it needs PipeWire and
`xdg-desktop-portal` (plus your compositor's backend) installed and running.
EOF

    # A hyphen in the tag means a prerelease (v1.0.0-alpha.1), never a plain release.
    case "$TAG" in
        *-*)
            printf '\nThis is a prerelease, published for early testing: expect rough edges.\n'
            printf 'AppImage builds from the alpha channel update themselves through '
            printf 'AppImageUpdate, pulling each new alpha as it lands; installing a stable '
            printf 'release later switches the app to the stable channel.\n'
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
