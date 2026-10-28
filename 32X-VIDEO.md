# 32X output with Mega Drive H32

32X composition and display use all 320 pixels even when the Mega Drive VDP
is in H32 (256-pixel) mode. The MD layer is expanded from 256 to 320 pixels
before priority mixing; the 32X framebuffer is read at its own pixel width.
GDI, DirectDraw, screenshots and paused CD32X redraw use the same output width.
MD-only H32 output remains 256 pixels.

In particular, direct-color software with a 256-word line stride exposes the
first 64 pixels of the following row at the right edge. The former 256-pixel
crop hid this behavior. No game-specific workaround or disc changes are used.
This fixes the width/cropping mismatch; analog pixel-clock phase is not modeled.
See Sega's [32X display specifications](https://github.com/matiaszanolli/sega-vr-disasm/blob/master/docs/32x-hardware-manual.md#overview-of-display-specifications)
and [PicoDrive's H32/32X clock notes](https://github.com/notaz/picodrive/blob/master/pico/32x/draw.c).

Run `python tests/run_32x_render_tests.py` for synthetic format, priority,
H32/H40, output-width and paused-redraw coverage. Add
`--image C:\apps\CD32X_Template\build\plane-a-jp.iso` to exercise the supplied
game and save screenshots in `src/Gens/bin/32x-render-tests`. Use `--skip-build`
to reuse an existing Release build.

