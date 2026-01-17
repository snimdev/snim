#!/usr/bin/env bash
#
# Regenerates every rasterized app icon from resources/icons/app-icon.svg:
#
#   resources/icons/hicolor/<N>x<N>/apps/dev.snim.Snim.png   (Linux, installed by CMake)
#   resources/AppIcon.icns                                   (macOS bundle icon)
#
# Both outputs are committed, so this only needs rerunning when the SVG changes.
#
# Usage: scripts/gen-icons.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SVG="$ROOT/resources/icons/app-icon.svg"
HICOLOR="$ROOT/resources/icons/hicolor"
ICNS="$ROOT/resources/AppIcon.icns"

SIZES="16 32 48 64 128 256 512"

if [ ! -f "$SVG" ]; then
    echo "error: $SVG not found" >&2
    exit 1
fi

if ! command -v rsvg-convert >/dev/null 2>&1; then
    echo "error: rsvg-convert not found (install librsvg2-tools / librsvg)" >&2
    exit 1
fi

render() {
    # render <size> <output-png>
    rsvg-convert --width "$1" --height "$2" --output "$3" "$SVG"
}

echo "==> Rendering hicolor PNGs"
for n in $SIZES; do
    out="$HICOLOR/${n}x${n}/apps/dev.snim.Snim.png"
    mkdir -p "$(dirname "$out")"
    render "$n" "$n" "$out"
    echo "    $out"
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# 1024 exists only inside the .icns (512@2x); Linux has no hicolor directory for it.
echo "==> Rendering icns sizes"
for n in 16 32 64 128 256 512 1024; do
    render "$n" "$n" "$TMP/icon_$n.png"
done

echo "==> Building $ICNS"
if command -v png2icns >/dev/null 2>&1; then
    png2icns "$ICNS" "$TMP/icon_16.png" "$TMP/icon_32.png" "$TMP/icon_128.png" \
             "$TMP/icon_256.png" "$TMP/icon_512.png" "$TMP/icon_1024.png"
    echo "    built with png2icns"
elif magick -list format 2>/dev/null | grep -qi '^ *ICNS.*[rw]'; then
    magick "$TMP/icon_1024.png" -define icon:auto-resize=1024,512,256,128,64,32,16 "$ICNS"
    echo "    built with ImageMagick"
else
    # No png2icns and no ImageMagick ICNS coder: write the container directly. The
    # format is a header plus 8-byte-tagged chunks, and macOS reads PNG payloads
    # for every type used here.
    python3 - "$ICNS" "$TMP" <<'PY'
import struct
import sys

out_path, tmp = sys.argv[1], sys.argv[2]

# icns type -> pixel size of the PNG stored under it.
entries = [
    ("icp4", 16), ("icp5", 32), ("icp6", 64),
    ("ic07", 128), ("ic08", 256), ("ic09", 512), ("ic10", 1024),
    ("ic11", 32), ("ic12", 64), ("ic13", 256), ("ic14", 512),
]

chunks = []
for tag, size in entries:
    with open("%s/icon_%d.png" % (tmp, size), "rb") as fh:
        data = fh.read()
    chunks.append(tag.encode("ascii") + struct.pack(">I", len(data) + 8) + data)

body = b"".join(chunks)
with open(out_path, "wb") as fh:
    fh.write(b"icns" + struct.pack(">I", len(body) + 8) + body)
PY
    echo "    built with the python fallback (png2icns not installed)"
fi

echo "==> Done"
