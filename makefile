check: src/wayclient.c src/wayclient.h src/xdg_shell.c
	@gcc -Wall -ggdb -fsyntax-only \
		src/*.c \
		-lwayland-client -lm

src/xdg_shell.c: protocols/xdg_shell.xml
	wayland-scanner client-header protocols/xdg_shell.xml src/xdg_shell.h
	wayland-scanner private-code protocols/xdg_shell.xml src/xdg_shell.c
