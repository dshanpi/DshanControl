# Licensing and redistribution notice

DshanControl's original application code is provided under GPL-3.0. Files in
this board-support snapshot may be derived from or intended to be combined
with Linux, Buildroot, SOEM, AIC8800 driver sources, the Allwinner Tina SDK and
other independently licensed projects. Kernel, wireless and U-Boot source
files in `overlay/bsp/` and `overlay/brandy/` remain under their GPLv2/SPDX
terms. SPDX identifiers embedded in individual files take precedence.

This snapshot deliberately excludes vendor firmware, boot binaries, compiled
device-tree tools and images, and the Tina SDK itself. Their absence is a
licensing boundary, not an assertion that the board can be rebuilt without
obtaining those prerequisites separately.
