#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/cmake-build-debug"
BINARY="$BUILD_DIR/snim"
DESKTOP_TEMPLATE="$SCRIPT_DIR/deploy/dev.snim.Snim.desktop"
DESKTOP_DEST="$HOME/.local/share/applications/dev.snim.Snim.desktop"

# Build
cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR"
cmake --build "$BUILD_DIR"

# Install .desktop file with correct Exec= path for KWin ScreenShot2 permissions
mkdir -p "$(dirname "$DESKTOP_DEST")"
sed "s|^Exec=.*|Exec=$BINARY|" "$DESKTOP_TEMPLATE" > "$DESKTOP_DEST"

echo ""
echo "Build complete: $BINARY"
echo "Desktop file installed: $DESKTOP_DEST"
echo "KWin ScreenShot2 permissions are now active."
