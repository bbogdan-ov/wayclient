# wayclient

> [!WARNING]
> I'm not a pro Wayland client developer and i might make some mistakes, i've
> made this template for people who are strugling with learning Wayland client
> development as i did.
>
> Feel free to send pull requests!

Minimal C library/template to open a Wayland window for software rendering stuff.

There is still a lot of things to do, but you already can draw something and handle keyboard and mouse inputs!

Have fun!

## Implemented protocols

- [`wayland.xml`](https://wayland.app/protocols/wayland)
- [`xdg-shell.xml`](https://wayland.app/protocols/xdg-shell)
- [`cursor-shape-v1.xml`](https://wayland.app/protocols/cursor-shape-v1), [`zwp_tablet`](https://wayland.app/protocols/tablet-v2) removed from dependencies

## Building

**Dependencies**:

- libwayland-client
- libxkbcommon

**Building a static library:**

```sh
make
# Files you need:
#   ./build/libwayclient.a
#   ./src/wayclient.h
#   ./src/xdg_shell.h
#   ./src/cursor_shape_v1.h
```

**Building examples:**

```sh
make examples
./build/<example_name>
```

## Resources

- [The Wayland book](https://wayland-book.com/)
- [Wayland protocol](https://wayland.app/protocols/)
- [Example by Will Thomas](https://github.com/willth7/wayland-client-example)
- [Example by Simon Ser](https://gitlab.freedesktop.org/emersion/hello-wayland)
- [Rofi Wayland by lbonn](https://github.com/lbonn/rofi)

## License

DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
