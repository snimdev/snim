#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/cmake-build-debug"
BINARY="$BUILD_DIR/snim"
DESKTOP_TEMPLATE="$SCRIPT_DIR/deploy/dev.snim.Snim.desktop"
DESKTOP_DEST="$HOME/.local/share/applications/dev.snim.Snim.desktop"
ICON_TEMPLATE="$SCRIPT_DIR/resources/icons/app-icon.svg"
ICON_THEME_DIR="$HOME/.local/share/icons/hicolor"
ICON_DEST="$ICON_THEME_DIR/scalable/apps/dev.snim.Snim.svg"

# Build
cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR"
cmake --build "$BUILD_DIR"

# Install .desktop file with correct Exec= path for KWin ScreenShot2 permissions
mkdir -p "$(dirname "$DESKTOP_DEST")"
sed "s|^Exec=.*|Exec=$BINARY|" "$DESKTOP_TEMPLATE" > "$DESKTOP_DEST"

# Install the app icon so the desktop entry's Icon= name resolves
mkdir -p "$(dirname "$ICON_DEST")"
cp "$ICON_TEMPLATE" "$ICON_DEST"

# Refresh the caches, where those tools exist
command -v update-desktop-database >/dev/null && update-desktop-database "$(dirname "$DESKTOP_DEST")" || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t -f "$ICON_THEME_DIR" || true

echo ""
echo "Build complete: $BINARY"
echo "Desktop file installed: $DESKTOP_DEST"
echo "Icon installed: $ICON_DEST"
echo "KWin ScreenShot2 permissions are now active."
