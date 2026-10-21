#!/usr/bin/env bash
#
# Regenerate the board images in Images/ from the KiCad layouts.
#
# The images are generated, not hand-drawn:
#   * the Lamoka1/2 views (iso, top, bottom) and the DEV-Brain views are
#     ray-traced with `kicad-cli pcb render` on a transparent background with
#     the floor shadow on, then trimmed of the transparent border so the board
#     fills the frame. Both boards render with `--use-board-stackup-colors`:
#     the Lamoka1/2 stackup is green mask with a white legend, matching the
#     PCBWay order, and the DEV-Brain stackup is black mask with a white
#     legend;
#   * the renders use `--quality high`, not `--quality user`. The "user" preset
#     blows the solder mask out to near-white under the default lighting; the
#     "high" preset keeps the mask the colour it really is, and its camera
#     framing fits the whole board *including* the tall parts (the Wago
#     connectors, the electrolytic cans and the TO-220 devices) inside the
#     frame, so nothing is cropped;
#   * Images/lamoka1-2-routing-copper.png is a 2D plot of the F.Cu and B.Cu
#     layers in KiCad's default colours (front red, back blue), rasterised with
#     ImageMagick and reduced to a 256-colour palette.
#
# Usage:
#
#     make images                       # or: tools/make_images.sh
#     KICAD_CLI=/path/to/kicad-cli tools/make_images.sh
#
# The DEV-Brain views need that board's 3D models, which are not committed (see
# its Readme): the script renders them when Lamoka1-DEV-Brain/Kicad
# files/components is present and skips those two images when it is not.
#
# Nothing outside Images/ is written. See CONTRIBUTING.md, "Board images".
set -euo pipefail

cd "$(dirname "$0")/.."

KICAD_CLI=${KICAD_CLI:-kicad-cli}
MAGICK=${MAGICK:-magick}

MAIN_KICAD="HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb"
DEV_DIR="Lamoka1-DEV-Brain/Kicad files"
DEV_KICAD="$DEV_DIR/Lamoka1-DEV-board.kicad_pcb"

command -v "$KICAD_CLI" >/dev/null 2>&1 || {
    echo "make images: $KICAD_CLI not found — install KiCad 10." >&2
    exit 1
}
command -v "$MAGICK" >/dev/null 2>&1 || {
    echo "make images: $MAGICK not found — install ImageMagick (it trims the renders and rasterises the routing plot)." >&2
    exit 1
}

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

wrote() {
    [ -s "$1" ] || {
        echo "make images: $1 was not written" >&2
        exit 1
    }
    echo "  $1"
}

echo "Lamoka1/2 ($MAIN_KICAD)"

# Isometric hero: the top view tilted -60 degrees about X and -45 about Z,
# orthographic (no --perspective), on a 2200x1600 canvas.
"$KICAD_CLI" pcb render --side top --rotate -60,0,-45 \
    --quality high --preset follow_pcb_editor --floor --background transparent --use-board-stackup-colors \
    --width 2200 --height 1600 -o "$TMP/iso.png" "$MAIN_KICAD"
"$MAGICK" "$TMP/iso.png" -trim +repage "Images/lamoka1-2-render-iso.png"
wrote "Images/lamoka1-2-render-iso.png"

# Straight-on top and bottom views on a square canvas.
"$KICAD_CLI" pcb render --side top \
    --quality high --preset follow_pcb_editor --floor --background transparent --use-board-stackup-colors \
    --width 1900 --height 1900 -o "$TMP/top.png" "$MAIN_KICAD"
"$MAGICK" "$TMP/top.png" -trim +repage "Images/lamoka1-2-render-top.png"
wrote "Images/lamoka1-2-render-top.png"

"$KICAD_CLI" pcb render --side bottom \
    --quality high --preset follow_pcb_editor --floor --background transparent --use-board-stackup-colors \
    --width 1900 --height 1900 -o "$TMP/bottom.png" "$MAIN_KICAD"
"$MAGICK" "$TMP/bottom.png" -trim +repage "Images/lamoka1-2-render-bottom.png"
wrote "Images/lamoka1-2-render-bottom.png"

# Routing view: one SVG with F.Cu and B.Cu, board area only, no drawing sheet.
# Rasterising at 1600 px before the 1200 px palette reduction keeps the pads
# and via barrels crisp.
"$KICAD_CLI" pcb export svg --mode-single --layers F.Cu,B.Cu,Edge.Cuts,F.SilkS,B.SilkS \
    --page-size-mode 2 --exclude-drawing-sheet --theme "KiCad Default" \
    -o "$TMP/routing.svg" "$MAIN_KICAD"
"$MAGICK" -density 120 "$TMP/routing.svg" -background white -alpha remove -alpha off \
    -fill black -opaque "#F2EDA1" -resize 2000x2000 "$TMP/routing.png"
"$MAGICK" "$TMP/routing.png" -resize 1400x1400 -colors 256 -strip \
    "PNG8:Images/lamoka1-2-routing-copper.png"
wrote "Images/lamoka1-2-routing-copper.png"

# Silkscreen plot: 2D top view of the legend (including the Flamingo & Cactus
# logo) so the logo is visible even where the 3D render drops silkscreen.
"$KICAD_CLI" pcb export svg --mode-single --layers F.SilkS,B.SilkS,Edge.Cuts \
    --page-size-mode 2 --exclude-drawing-sheet --theme "KiCad Default" \
    -o "$TMP/silk.svg" "$MAIN_KICAD"
"$MAGICK" -density 300 "$TMP/silk.svg" -background white -alpha remove -alpha off \
    -fill black -opaque "#F2EDA1" -resize 2000x2000 "$TMP/silk.png"
"$MAGICK" "$TMP/silk.png" -resize 1400x1400 -strip \
    "Images/lamoka1-2-silkscreen.png"
wrote "Images/lamoka1-2-silkscreen.png"

if [ -d "$DEV_DIR/components" ]; then
    echo "Lamoka1-DEV-Brain ($DEV_KICAD)"
    # A long, narrow board: the camera zooms out to fit it, and the render
    # takes the board's own stackup colours (black mask).
    "$KICAD_CLI" pcb render --side top --rotate -30,0,30 --perspective \
        --quality high --preset follow_pcb_editor --floor --use-board-stackup-colors \
        --width 2400 --height 1800 --zoom 0.85 -o "$TMP/dev-iso.png" "$DEV_KICAD"
    "$MAGICK" "$TMP/dev-iso.png" -trim +repage "Images/lamoka1-dev-brain-render-iso.png"
    wrote "Images/lamoka1-dev-brain-render-iso.png"

    "$KICAD_CLI" pcb render --side top \
        --quality high --preset follow_pcb_editor --floor --use-board-stackup-colors \
        --width 1880 --height 1880 --zoom 0.9 -o "$TMP/dev-top.png" "$DEV_KICAD"
    "$MAGICK" "$TMP/dev-top.png" -trim +repage "Images/lamoka1-dev-brain-render-top.png"
    wrote "Images/lamoka1-dev-brain-render-top.png"
else
    echo "Lamoka1-DEV-Brain: skipped — no 3D models at '$DEV_DIR/components'" >&2
    echo "  (link the local model folder there to regenerate its two renders)" >&2
fi
