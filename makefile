check: wayclient.c wayclient.h protocols/xdg-shell.c
	@gcc -Wall -ggdb -fsyntax-only \
		wayclient.c protocols/xdg-shell.c \
		-lwayland-client -lm

protocols/xdg-shell.c: protocols/xdg-shell.xml
	wayland-scanner client-header protocols/xdg-shell.xml protocols/xdg-shell.h
	wayland-scanner private-code protocols/xdg-shell.xml protocols/xdg-shell.c
