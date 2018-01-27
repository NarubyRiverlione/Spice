
Debian
====================
This directory contains files used to package spiced/spice-qt
for Debian-based Linux systems. If you compile spiced/spice-qt yourself, there are some useful files here.

## spice: URI support ##


spice-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install spice-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your spice-qt binary to `/usr/bin`
and the `../../share/pixmaps/spice128.png` to `/usr/share/pixmaps`

spice-qt.protocol (KDE)

