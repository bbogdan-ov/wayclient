CFLAGS = -Wall

EXAMPLE_CC = gcc $(CFLAGS) -ggdb -Isrc -lwayland-client src/*.c

.PHONY: check examples

check: src/wayclient.c src/wayclient.h src/xdg_shell.c
	@gcc $(CFLAGS) -fsyntax-only \
		src/*.c \
		-lwayland-client

src/xdg_shell.c: protocols/xdg_shell.xml
	wayland-scanner client-header protocols/xdg_shell.xml src/xdg_shell.h
	wayland-scanner private-code protocols/xdg_shell.xml src/xdg_shell.c

examples:
	$(EXAMPLE_CC) examples/basic.c -o examples/basic
