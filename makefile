CFLAGS = -Wall -lwayland-client -lxkbcommon

EXAMPLE_CC = gcc $(CFLAGS) -ggdb -Isrc src/*.c

.PHONY: protocols examples clean

# Compile static library.
build/libwayclient.a: src/*.c src/*.h
	mkdir -p build
	gcc $(CFLAGS) -O3 -c src/*.c
	ar rcs build/libwayclient.a *.o

# Generate protocols code.
protocols:
	wayland-scanner client-header protocols/xdg_shell.xml src/xdg_shell.h
	wayland-scanner private-code  protocols/xdg_shell.xml src/xdg_shell.c
	wayland-scanner client-header protocols/cursor_shape_v1_modified.xml src/cursor_shape_v1.h
	wayland-scanner private-code  protocols/cursor_shape_v1_modified.xml src/cursor_shape_v1.c

# Compile examples.
examples:
	mkdir -p build
	$(EXAMPLE_CC) examples/basic.c -o build/basic

# Clean the mess up after a compilation.
clean:
	rm *.o
	rm -r build
