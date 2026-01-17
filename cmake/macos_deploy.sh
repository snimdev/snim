#!/usr/bin/env bash
#
# macOS deployment helper for Snim.
#
# 1. Runs macdeployqt to copy the Qt frameworks and other linked dylibs
#    (tesseract, leptonica, archive, ...) into the .app bundle.
# 2. Normalizes the main executable's rpaths: removes build-time Homebrew
#    rpaths (/opt/homebrew, /usr/local) and ensures @executable_path/../Frameworks
#    is present. Without this, plugins that load Qt via @rpath (e.g. the cocoa
#    platform plugin) resolve through the leftover Homebrew rpath and load a
#    SECOND copy of Qt, causing "Class ... implemented in both ..." warnings
#    and potential crashes.
# 3. Copies Homebrew's English Tesseract language pack into Contents/Resources,
#    since a user's Mac has no tessdata of its own.
# 4. Ad-hoc code-signs the whole bundle so it launches and keeps a stable
#    Designated Requirement.
#
# Usage: macos_deploy.sh <path-to-.app> <path-to-macdeployqt>

set -euo pipefail

APP="${1:?usage: macos_deploy.sh <app> <macdeployqt>}"
MACDEPLOYQT="${2:?usage: macos_deploy.sh <app> <macdeployqt>}"

EXE_NAME="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP/Contents/Info.plist" 2>/dev/null || true)"
if [ -z "$EXE_NAME" ]; then
    # Fall back to the single binary in Contents/MacOS if the plist key is absent.
    EXE_NAME="$(ls "$APP/Contents/MacOS" 2>/dev/null | head -n1)"
fi
EXE="$APP/Contents/MacOS/$EXE_NAME"
if [ ! -f "$EXE" ]; then
    echo "error: executable not found in $APP/Contents/MacOS (is the bundle fully built?)" >&2
    exit 1
fi

echo "==> macdeployqt $APP"
"$MACDEPLOYQT" "$APP" -always-overwrite

echo "==> Normalizing rpaths on $EXE"
# Collect existing rpaths, then drop any that point into a Homebrew/local prefix.
otool -l "$EXE" \
    | awk '$1=="cmd" && $2=="LC_RPATH"{f=1} f && $1=="path"{print $2; f=0}' \
    | while read -r rp; do
        case "$rp" in
            /opt/homebrew/*|/usr/local/*)
                echo "    delete rpath: $rp"
                install_name_tool -delete_rpath "$rp" "$EXE" 2>/dev/null || true
                ;;
        esac
    done

# Check LC_RPATH entries only (a plain grep would falsely match the
# @executable_path/../Frameworks/... paths inside LC_LOAD_DYLIB commands).
existing_rpaths="$(otool -l "$EXE" | awk '$1=="cmd" && $2=="LC_RPATH"{f=1} f && $1=="path"{print $2; f=0}')"
if ! grep -Fxq "@executable_path/../Frameworks" <<<"$existing_rpaths"; then
    echo "    add rpath: @executable_path/../Frameworks"
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$EXE"
fi

echo "==> Bundling tessdata"
# OCR needs a language pack next to the binary; a user's Mac has no Homebrew tessdata.
# Must happen before signing, or the added file invalidates the signature.
BREW_PREFIX="$(brew --prefix 2>/dev/null || true)"
TRAINEDDATA="${BREW_PREFIX:+$BREW_PREFIX/share/tessdata/eng.traineddata}"
if [ -n "$TRAINEDDATA" ] && [ -f "$TRAINEDDATA" ]; then
    mkdir -p "$APP/Contents/Resources/tessdata"
    cp "$TRAINEDDATA" "$APP/Contents/Resources/tessdata/"
    echo "    copied $TRAINEDDATA"
else
    echo "    skipped: no Homebrew eng.traineddata found (OCR will need a system install)"
fi

echo "==> ad-hoc codesigning $APP"
codesign --force --deep --sign - "$APP"

echo "==> Verifying signature"
codesign --verify --deep --strict "$APP"

echo "==> Done: $APP"
