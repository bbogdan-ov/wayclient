#include "wayclient.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

// ------------------------------
// WL registry listener.
// ------------------------------

static void
wayclient__registry_handle_global(
	void *data,
	struct wl_registry *wl_registry,
	uint32_t name,
	const char *interface,
	uint32_t version
) {
	wayclient_state *state = data;

	#define BIND(field, iface) \
		if (strcmp(interface, iface.name) == 0) { \
			state->field = wl_registry_bind(wl_registry, name, &iface, version); \
			wayclient_logf("Bound %s v%d: %p", iface.name, version, state->field); \
			return; \
		}

	BIND(wl_compositor, wl_compositor_interface);
	BIND(wl_shm,        wl_shm_interface);
	BIND(wl_seat,       wl_seat_interface);
	BIND(xdg_wm_base,   xdg_wm_base_interface);
}

// TODO: do i have to handle this event?
static void
wayclient__registry_handle_global_remove(
	void *data,
	struct wl_registry *wl_registry,
	uint32_t name
) {}

struct wl_registry_listener wayclient__wl_registry_listener = {
	.global = wayclient__registry_handle_global,
	.global_remove = wayclient__registry_handle_global_remove,
};

// ------------------------------
// WL seat listener.
// ------------------------------

static void
wayclient__wl_seat_handle_capabilities(
	void *data,
	struct wl_seat *wl_seat,
	uint32_t capabilities
) {
	wayclient_state *state = data;

	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && state->wl_pointer == NULL) {
		state->wl_pointer = wl_seat_get_pointer(wl_seat);
		wayclient_logf("Bound wl_pointer: %p", state->wl_pointer);
	}
	if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && state->wl_keyboard == NULL) {
		state->wl_keyboard = wl_seat_get_keyboard(wl_seat);
		wayclient_logf("Bound wl_keyboard: %p", state->wl_keyboard);
	}
}

static void
wayclient__wl_seat_handle_name(
	void *data,
	struct wl_seat *wl_seat,
	const char *name
) {}

struct wl_seat_listener wayclient__wl_seat_listener = {
	.capabilities = wayclient__wl_seat_handle_capabilities,
	.name = wayclient__wl_seat_handle_name,
};

// ------------------------------
// WL pointer listener.
// ------------------------------

static void
wayclient__wl_pointer_handle_enter(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t serial,
	struct wl_surface *surface,
	wl_fixed_t surface_x,
	wl_fixed_t surface_y
) {
	wayclient_state *state = data;
	if (state->on_pointer_enter != NULL)
		state->on_pointer_enter(state, state->userdata);
}

static void
wayclient__wl_pointer_handle_leave(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t serial,
	struct wl_surface *surface
) {
	wayclient_state *state = data;
	if (state->on_pointer_leave != NULL)
		state->on_pointer_leave(state, state->userdata);
}

static void
wayclient__wl_pointer_handle_motion(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t time,
	wl_fixed_t surface_x,
	wl_fixed_t surface_y
) {
	wayclient_state *state = data;
	if (state->on_pointer_motion != NULL)
		state->on_pointer_motion(
			state,
			wl_fixed_to_double(surface_x),
			wl_fixed_to_double(surface_y),
			state->userdata
		);
}

static void
wayclient__wl_pointer_handle_button(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t serial,
	uint32_t time,
	uint32_t button,
	uint32_t button_state
) {
	wayclient_state *state = data;
	if (state->on_pointer_button != NULL)
		state->on_pointer_button(
			state,
			button,
			button_state,
			state->userdata
		);
}

static void
wayclient__wl_pointer_handle_axis(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t time,
	uint32_t axis,
	wl_fixed_t value
) {
	wayclient_state *state = data;
	if (state->on_pointer_scroll == NULL) return;

	double x = 0.0;
	double y = 0.0;

	if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
		x = wl_fixed_to_double(value);
	else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
		y = wl_fixed_to_double(value);

	state->on_pointer_scroll(state, x, y, state->userdata);
}

static void
wayclient__wl_pointer_handle_frame(
	void *data,
	struct wl_pointer *wl_pointer
) {}

static void
wayclient__wl_pointer_handle_axis_source(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t axis_source
) {
	// TODO: should probably store "scroll source" in the state.
}

static void
wayclient__wl_pointer_handle_axis_stop(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t time,
	uint32_t axis
) {}

static void
wayclient__wl_pointer_handle_axis_discrete(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t axis,
	int32_t discrete
) {}

static void
wayclient__wl_pointer_handle_axis_value120(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t axis,
	int32_t value120
) {}

static void
wayclient__wl_pointer_handle_axis_relative_direction(
	void *data,
	struct wl_pointer *wl_pointer,
	uint32_t axis,
	uint32_t direction
) {}

struct wl_pointer_listener wayclient__wl_pointer_listener = {
	.enter = wayclient__wl_pointer_handle_enter,
	.leave = wayclient__wl_pointer_handle_leave,
	.motion = wayclient__wl_pointer_handle_motion,
	.button = wayclient__wl_pointer_handle_button,
	.axis = wayclient__wl_pointer_handle_axis,
	.frame = wayclient__wl_pointer_handle_frame,
	.axis_source = wayclient__wl_pointer_handle_axis_source,
	.axis_stop = wayclient__wl_pointer_handle_axis_stop,
	.axis_discrete = wayclient__wl_pointer_handle_axis_discrete,
	.axis_value120 = wayclient__wl_pointer_handle_axis_value120,
	.axis_relative_direction = wayclient__wl_pointer_handle_axis_relative_direction,
};

// ------------------------------
// WL keyboard listener.
// ------------------------------

static void
wayclient__wl_keyboard_handle_keymap(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t format,
	int32_t fd,
	uint32_t size
) {
	wayclient_state *state = data;

	assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1); // TODO: handle unsupported format.

	wayclient_logf("Keymap: format = %d, fd = %d, size = %d", format, fd, size);

	char *keymap_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	assert(keymap_str != MAP_FAILED); // TODO: handle error.

	struct xkb_keymap *keymap = xkb_keymap_new_from_buffer(
		state->xkb_context,
		keymap_str,
		size,
		XKB_KEYMAP_FORMAT_TEXT_V1,
		XKB_KEYMAP_COMPILE_NO_FLAGS
	);
	state->xkb_state = xkb_state_new(keymap);
	xkb_keymap_unref(keymap);

	munmap(keymap_str, size);
	close(fd);
}

static void
wayclient__wl_keyboard_handle_enter(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t serial,
	struct wl_surface *surface,
	struct wl_array *keys
) {
	wayclient_state *state = data;
	if (state->on_keyboard_enter != NULL)
		state->on_keyboard_enter(state, state->userdata);
}

static void
wayclient__wl_keyboard_handle_leave(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t serial,
	struct wl_surface *surface
) {
	wayclient_state *state = data;
	if (state->on_keyboard_leave != NULL)
		state->on_keyboard_leave(state, state->userdata);
}

static void
wayclient__wl_keyboard_handle_key(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t serial,
	uint32_t time,
	uint32_t key,
	uint32_t key_state
) {
	wayclient_state *state = data;
	if (state->on_keyboard_key == NULL) return;

	// "...clients must add 8 to the key event keycode" for xkb keymap format.
	xkb_keysym_t keysym = xkb_state_key_get_one_sym(state->xkb_state, key + 8);

	state->on_keyboard_key(state, key, keysym, key_state, state->userdata);
}

static void
wayclient__wl_keyboard_handle_modifiers(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t serial,
	uint32_t mods_depressed,
	uint32_t mods_latched,
	uint32_t mods_locked,
	uint32_t group
) {
	wayclient_state *state = data;

	xkb_state_update_mask(
		state->xkb_state,
		mods_depressed,
		mods_latched,
		mods_locked,
		0,
		0,
		group
	);
}

static void
wayclient__wl_keyboard_handle_repeat_info(
	void *data,
	struct wl_keyboard *wl_keyboard,
	int32_t rate,
	int32_t delay
) {
	// wayclient_state *state = data;

	// TODO!!: implement key press repeation.
	// For now repeated keypress (after you hold on a key and wait a little
	// delay) doesn't get registered.
}

struct wl_keyboard_listener wayclient__wl_keyboard_listener = {
	.keymap = wayclient__wl_keyboard_handle_keymap,
	.enter = wayclient__wl_keyboard_handle_enter,
	.leave = wayclient__wl_keyboard_handle_leave,
	.key = wayclient__wl_keyboard_handle_key,
	.modifiers = wayclient__wl_keyboard_handle_modifiers,
	.repeat_info = wayclient__wl_keyboard_handle_repeat_info,
};

// ------------------------------
// XDG WM base listener.
// ------------------------------

static void
wayclient__xdg_wm_base_handle_ping(
	void *data,
	struct xdg_wm_base *xdg_wm_base,
	uint32_t serial
) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

struct xdg_wm_base_listener wayclient__xdg_wm_base_listener = {
	.ping = wayclient__xdg_wm_base_handle_ping,
};

// ------------------------------
// WL callback listener.
// ------------------------------

struct wl_callback_listener wayclient__frame_callback_listener;

static void
wayclient__frame_callback_handle_done(
	void *data,
	struct wl_callback *wl_callback,
	uint32_t callback_data
) {
	wayclient_state *state = data;

	wl_callback_destroy(wl_callback);
	wl_callback = wl_surface_frame(state->wl_surface);
	wl_callback_add_listener(wl_callback, &wayclient__frame_callback_listener, state);
	state->wl_frame_callback = wl_callback;

	wayclient__draw(state);
}

struct wl_callback_listener wayclient__frame_callback_listener = {
	.done = wayclient__frame_callback_handle_done,
};

// ------------------------------
// XDG surface listener.
// ------------------------------

static void
wayclient__xdg_surface_handle_configure(
	void *data,
	struct xdg_surface *xdg_surface,
	uint32_t serial
) {
	wayclient_state *state = data;

	xdg_surface_ack_configure(xdg_surface, serial);

	if (state->pool_data == NULL) {
		wayclient__update_buffers_size(state);
	}

	wayclient__draw(state);
}

struct xdg_surface_listener wayclient__xdg_surface_listener = {
	.configure = wayclient__xdg_surface_handle_configure,
};

// ------------------------------
// XDG toplevel listener.
// ------------------------------

static void
wayclient__xdg_toplevel_handle_configure(
	void *data,
	struct xdg_toplevel *xdg_toplevel,
	int32_t width,
	int32_t height,
	struct wl_array *states
) {
	wayclient_state *state = data;
	if (!state->resizable) return;
	if (width != state->width || height != state->height) {
		state->width = width;
		state->height = height;
		wayclient__update_buffers_size(state);
	}
}

static void
wayclient__xdg_toplevel_handle_close(
	void *data,
	struct xdg_toplevel *xdg_toplevel
) {
	wayclient_state *state = data;
	state->should_close = true;
}

static void
wayclient__xdg_toplevel_handle_configure_bounds(
	void *data,
	struct xdg_toplevel *xdg_toplevel,
	int32_t width,
	int32_t height
) {}

static void
wayclient__xdg_toplevel_handle_wm_capabilities(
	void *data,
	struct xdg_toplevel *xdg_toplevel,
	struct wl_array *capabilities
) {}

struct xdg_toplevel_listener wayclient__xdg_toplevel_listener = {
	.configure = wayclient__xdg_toplevel_handle_configure,
	.close = wayclient__xdg_toplevel_handle_close,
	.configure_bounds = wayclient__xdg_toplevel_handle_configure_bounds,
	.wm_capabilities = wayclient__xdg_toplevel_handle_wm_capabilities,
};

// ------------------------------
// WL buffer listener.
// ------------------------------

static void
wayclient__wl_buffer_handle_release(void *data, struct wl_buffer *wl_buffer) {
	wayclient_state *state = data;

	for (int i = 0; i < WAYCLIENT_BUFFER_COUNT; i ++) {
		if (state->buffers[i].wl_buffer == wl_buffer) {
			state->buffers[i].attached = false;
			break;
		}
	}
}

struct wl_buffer_listener wayclient__wl_buffer_listener = {
	.release = wayclient__wl_buffer_handle_release,
};

// ------------------------------
// Public functions.
// ------------------------------

void
wayclient_init(wayclient_state *state, uint32_t width, uint32_t height) {
	memset(state, 0, sizeof(wayclient_state));
	state->width = width;
	state->height = height;
	state->resizable = true;
}

wayclient_error
wayclient_run(wayclient_state *state) {
	state->wl_display = wl_display_connect(NULL);
	if (state->wl_display == NULL) {
		return WAYCLIENT_ERR_CONNECT;
	}

	state->wl_registry = wl_display_get_registry(state->wl_display);
	if (state->wl_registry == NULL) {
		return WAYCLIENT_ERR_GET_OBJECTS;
	}

	wl_registry_add_listener(state->wl_registry, &wayclient__wl_registry_listener, state);
	wl_display_roundtrip(state->wl_display);

	// Expect these regsitry objects to be presented by the compositor:
	if (state->wl_compositor == NULL) return WAYCLIENT_ERR_GET_OBJECTS;
	if (state->wl_shm == NULL)        return WAYCLIENT_ERR_GET_OBJECTS;
	if (state->xdg_wm_base == NULL)   return WAYCLIENT_ERR_GET_OBJECTS;

	if (state->wl_seat != NULL) {
		wl_seat_add_listener(state->wl_seat, &wayclient__wl_seat_listener, state);
		wl_display_roundtrip(state->wl_display); // Wait untill we get all devices (pointer, keyboard, etc).

		if (state->wl_pointer != NULL)
			wl_pointer_add_listener(state->wl_pointer, &wayclient__wl_pointer_listener, state);

		if (state->wl_keyboard != NULL) {
			state->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
			wl_keyboard_add_listener(state->wl_keyboard, &wayclient__wl_keyboard_listener, state);
		}
	}

	xdg_wm_base_add_listener(state->xdg_wm_base, &wayclient__xdg_wm_base_listener, state);

	state->wl_surface = wl_compositor_create_surface(state->wl_compositor);
	if (state->wl_surface == NULL) return WAYCLIENT_ERR_CREATE_SURFACE;

	state->xdg_surface = xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->wl_surface);
	if (state->xdg_surface == NULL) return WAYCLIENT_ERR_CREATE_SURFACE;

	state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
	if (state->xdg_toplevel == NULL) return WAYCLIENT_ERR_CREATE_TOPLEVEL;

	state->wl_frame_callback = wl_surface_frame(state->wl_surface);
	wl_callback_add_listener(state->wl_frame_callback, &wayclient__frame_callback_listener, state);

	xdg_surface_add_listener(state->xdg_surface, &wayclient__xdg_surface_listener, state);
	xdg_toplevel_add_listener(state->xdg_toplevel, &wayclient__xdg_toplevel_listener, state);

	wl_surface_commit(state->wl_surface);

	return WAYCLIENT_OK;
}

void
wayclient_destroy(wayclient_state *state) {
	state->prev_width = state->width;
	state->prev_height = state->height;
	wayclient__destroy_and_unmap_buffers(state);

	if (state->wl_frame_callback != NULL) wl_callback_destroy(state->wl_frame_callback);

	if (state->xdg_toplevel != NULL) xdg_toplevel_destroy(state->xdg_toplevel);
	if (state->xdg_surface != NULL)  xdg_surface_destroy(state->xdg_surface);
	if (state->xdg_wm_base != NULL)  xdg_wm_base_destroy(state->xdg_wm_base);

	if (state->wl_pointer != NULL)  wl_pointer_release(state->wl_pointer);
	if (state->wl_keyboard != NULL) wl_keyboard_release(state->wl_keyboard);
	if (state->wl_seat != NULL)     wl_seat_destroy(state->wl_seat);

	wl_surface_destroy(state->wl_surface);
	wl_shm_destroy(state->wl_shm);
	wl_compositor_destroy(state->wl_compositor);
	wl_registry_destroy(state->wl_registry);
	wl_display_disconnect(state->wl_display);
}

// ------------------------------
// Utils.
// ------------------------------

const char *
wayclient_keycode_to_str(uint32_t keycode) {
	// I'm pretty sure the C compiler should optimize this into a lookup table.
	switch (keycode) {
		case KEY_RESERVED:         return "Reserved";
		case KEY_ESC:              return "Esc";
		case KEY_1:                return "1";
		case KEY_2:                return "2";
		case KEY_3:                return "3";
		case KEY_4:                return "4";
		case KEY_5:                return "5";
		case KEY_6:                return "6";
		case KEY_7:                return "7";
		case KEY_8:                return "8";
		case KEY_9:                return "9";
		case KEY_0:                return "0";
		case KEY_MINUS:            return "Minus";
		case KEY_EQUAL:            return "Equal";
		case KEY_BACKSPACE:        return "Backspace";
		case KEY_TAB:              return "Tab";
		case KEY_Q:                return "Q";
		case KEY_W:                return "W";
		case KEY_E:                return "E";
		case KEY_R:                return "R";
		case KEY_T:                return "T";
		case KEY_Y:                return "Y";
		case KEY_U:                return "U";
		case KEY_I:                return "I";
		case KEY_O:                return "O";
		case KEY_P:                return "P";
		case KEY_LEFTBRACE:        return "Leftbrace";
		case KEY_RIGHTBRACE:       return "Rightbrace";
		case KEY_ENTER:            return "Enter";
		case KEY_LEFTCTRL:         return "Leftctrl";
		case KEY_A:                return "A";
		case KEY_S:                return "S";
		case KEY_D:                return "D";
		case KEY_F:                return "F";
		case KEY_G:                return "G";
		case KEY_H:                return "H";
		case KEY_J:                return "J";
		case KEY_K:                return "K";
		case KEY_L:                return "L";
		case KEY_SEMICOLON:        return "Semicolon";
		case KEY_APOSTROPHE:       return "Apostrophe";
		case KEY_GRAVE:            return "Grave";
		case KEY_LEFTSHIFT:        return "Leftshift";
		case KEY_BACKSLASH:        return "Backslash";
		case KEY_Z:                return "Z";
		case KEY_X:                return "X";
		case KEY_C:                return "C";
		case KEY_V:                return "V";
		case KEY_B:                return "B";
		case KEY_N:                return "N";
		case KEY_M:                return "M";
		case KEY_COMMA:            return "Comma";
		case KEY_DOT:              return "Dot";
		case KEY_SLASH:            return "Slash";
		case KEY_RIGHTSHIFT:       return "Rightshift";
		case KEY_KPASTERISK:       return "Kpasterisk";
		case KEY_LEFTALT:          return "Leftalt";
		case KEY_SPACE:            return "Space";
		case KEY_CAPSLOCK:         return "Capslock";
		case KEY_F1:               return "F1";
		case KEY_F2:               return "F2";
		case KEY_F3:               return "F3";
		case KEY_F4:               return "F4";
		case KEY_F5:               return "F5";
		case KEY_F6:               return "F6";
		case KEY_F7:               return "F7";
		case KEY_F8:               return "F8";
		case KEY_F9:               return "F9";
		case KEY_F10:              return "F10";
		case KEY_NUMLOCK:          return "Numlock";
		case KEY_SCROLLLOCK:       return "Scrolllock";
		case KEY_KP7:              return "Kp7";
		case KEY_KP8:              return "Kp8";
		case KEY_KP9:              return "Kp9";
		case KEY_KPMINUS:          return "Kpminus";
		case KEY_KP4:              return "Kp4";
		case KEY_KP5:              return "Kp5";
		case KEY_KP6:              return "Kp6";
		case KEY_KPPLUS:           return "Kpplus";
		case KEY_KP1:              return "Kp1";
		case KEY_KP2:              return "Kp2";
		case KEY_KP3:              return "Kp3";
		case KEY_KP0:              return "Kp0";
		case KEY_KPDOT:            return "Kpdot";
		case KEY_ZENKAKUHANKAKU:   return "Zenkakuhankaku";
		case KEY_102ND:            return "102nd";
		case KEY_F11:              return "F11";
		case KEY_F12:              return "F12";
		case KEY_RO:               return "Ro";
		case KEY_KATAKANA:         return "Katakana";
		case KEY_HIRAGANA:         return "Hiragana";
		case KEY_HENKAN:           return "Henkan";
		case KEY_KATAKANAHIRAGANA: return "Katakanahiragana";
		case KEY_MUHENKAN:         return "Muhenkan";
		case KEY_KPJPCOMMA:        return "Kpjpcomma";
		case KEY_KPENTER:          return "Kpenter";
		case KEY_RIGHTCTRL:        return "Rightctrl";
		case KEY_KPSLASH:          return "Kpslash";
		case KEY_SYSRQ:            return "Sysrq";
		case KEY_RIGHTALT:         return "Rightalt";
		case KEY_LINEFEED:         return "Linefeed";
		case KEY_HOME:             return "Home";
		case KEY_UP:               return "Up";
		case KEY_PAGEUP:           return "Pageup";
		case KEY_LEFT:             return "Left";
		case KEY_RIGHT:            return "Right";
		case KEY_END:              return "End";
		case KEY_DOWN:             return "Down";
		case KEY_PAGEDOWN:         return "Pagedown";
		case KEY_INSERT:           return "Insert";
		case KEY_DELETE:           return "Delete";
		case KEY_MACRO:            return "Macro";
		case KEY_MUTE:             return "Mute";
		case KEY_VOLUMEDOWN:       return "Volumedown";
		case KEY_VOLUMEUP:         return "Volumeup";
		case KEY_POWER:            return "Power";
		case KEY_KPEQUAL:          return "Kpequal";
		case KEY_KPPLUSMINUS:      return "Kpplusminus";
		case KEY_PAUSE:            return "Pause";
		case KEY_SCALE:            return "Scale";
		case KEY_KPCOMMA:          return "Kpcomma";
		case KEY_HANGEUL:          return "Hangeul";
		case KEY_HANJA:            return "Hanja";
		case KEY_YEN:              return "Yen";
		case KEY_LEFTMETA:         return "Leftmeta";
		case KEY_RIGHTMETA:        return "Rightmeta";
		case KEY_COMPOSE:          return "Compose";
		case KEY_STOP:             return "Stop";
		case KEY_AGAIN:            return "Again";
		case KEY_PROPS:            return "Props";
		case KEY_UNDO:             return "Undo";
		case KEY_FRONT:            return "Front";
		case KEY_COPY:             return "Copy";
		case KEY_OPEN:             return "Open";
		case KEY_PASTE:            return "Paste";
		case KEY_FIND:             return "Find";
		case KEY_CUT:              return "Cut";
		case KEY_HELP:             return "Help";
		case KEY_MENU:             return "Menu";
		case KEY_CALC:             return "Calc";
		case KEY_SETUP:            return "Setup";
		case KEY_SLEEP:            return "Sleep";
		case KEY_WAKEUP:           return "Wakeup";
		case KEY_FILE:             return "File";
		case KEY_SENDFILE:         return "Sendfile";
		case KEY_DELETEFILE:       return "Deletefile";
		case KEY_XFER:             return "Xfer";
		case KEY_PROG1:            return "Prog1";
		case KEY_PROG2:            return "Prog2";
		case KEY_WWW:              return "Www";
		case KEY_MSDOS:            return "Msdos";
		case KEY_COFFEE:           return "Coffee";
		case KEY_ROTATE_DISPLAY:   return "Rotate display";
		case KEY_CYCLEWINDOWS:     return "Cyclewindows";
		case KEY_MAIL:             return "Mail";
		case KEY_BOOKMARKS:        return "Bookmarks";
		case KEY_COMPUTER:         return "Computer";
		case KEY_BACK:             return "Back";
		case KEY_FORWARD:          return "Forward";
		case KEY_CLOSECD:          return "Closecd";
		case KEY_EJECTCD:          return "Ejectcd";
		case KEY_EJECTCLOSECD:     return "Ejectclosecd";
		case KEY_NEXTSONG:         return "Nextsong";
		case KEY_PLAYPAUSE:        return "Playpause";
		case KEY_PREVIOUSSONG:     return "Previoussong";
		case KEY_STOPCD:           return "Stopcd";
		case KEY_RECORD:           return "Record";
		case KEY_REWIND:           return "Rewind";
		case KEY_PHONE:            return "Phone";
		case KEY_ISO:              return "Iso";
		case KEY_CONFIG:           return "Config";
		case KEY_HOMEPAGE:         return "Homepage";
		case KEY_REFRESH:          return "Refresh";
		case KEY_EXIT:             return "Exit";
		case KEY_MOVE:             return "Move";
		case KEY_EDIT:             return "Edit";
		case KEY_SCROLLUP:         return "Scrollup";
		case KEY_SCROLLDOWN:       return "Scrolldown";
		case KEY_KPLEFTPAREN:      return "Kpleftparen";
		case KEY_KPRIGHTPAREN:     return "Kprightparen";
		case KEY_NEW:              return "New";
		case KEY_REDO:             return "Redo";
		case KEY_F13:              return "F13";
		case KEY_F14:              return "F14";
		case KEY_F15:              return "F15";
		case KEY_F16:              return "F16";
		case KEY_F17:              return "F17";
		case KEY_F18:              return "F18";
		case KEY_F19:              return "F19";
		case KEY_F20:              return "F20";
		case KEY_F21:              return "F21";
		case KEY_F22:              return "F22";
		case KEY_F23:              return "F23";
		case KEY_F24:              return "F24";
		case KEY_PLAYCD:           return "Playcd";
		case KEY_PAUSECD:          return "Pausecd";
		case KEY_PROG3:            return "Prog3";
		case KEY_PROG4:            return "Prog4";
		case KEY_ALL_APPLICATIONS: return "All applications";
		case KEY_SUSPEND:          return "Suspend";
		case KEY_CLOSE:            return "Close";
		case KEY_PLAY:             return "Play";
		case KEY_FASTFORWARD:      return "Fastforward";
		case KEY_BASSBOOST:        return "Bassboost";
		case KEY_PRINT:            return "Print";
		case KEY_HP:               return "Hp";
		case KEY_CAMERA:           return "Camera";
		case KEY_SOUND:            return "Sound";
		case KEY_QUESTION:         return "Question";
		case KEY_EMAIL:            return "Email";
		case KEY_CHAT:             return "Chat";
		case KEY_SEARCH:           return "Search";
		case KEY_CONNECT:          return "Connect";
		case KEY_FINANCE:          return "Finance";
		case KEY_SPORT:            return "Sport";
		case KEY_SHOP:             return "Shop";
		case KEY_ALTERASE:         return "Alterase";
		case KEY_CANCEL:           return "Cancel";
		case KEY_BRIGHTNESSDOWN:   return "Brightnessdown";
		case KEY_BRIGHTNESSUP:     return "Brightnessup";
		case KEY_MEDIA:            return "Media";
		case KEY_SWITCHVIDEOMODE:  return "Switchvideomode";
		case KEY_KBDILLUMTOGGLE:   return "Kbdillumtoggle";
		case KEY_KBDILLUMDOWN:     return "Kbdillumdown";
		case KEY_KBDILLUMUP:       return "Kbdillumup";
		case KEY_SEND:             return "Send";
		case KEY_REPLY:            return "Reply";
		case KEY_FORWARDMAIL:      return "Forwardmail";
		case KEY_SAVE:             return "Save";
		case KEY_DOCUMENTS:        return "Documents";
		case KEY_BATTERY:          return "Battery";
		case KEY_BLUETOOTH:        return "Bluetooth";
		case KEY_WLAN:             return "Wlan";
		case KEY_UWB:              return "Uwb";
		case KEY_UNKNOWN:          return "Unknown";
		case KEY_VIDEO_NEXT:       return "Video next";
		case KEY_VIDEO_PREV:       return "Video prev";
		case KEY_BRIGHTNESS_CYCLE: return "Brightness cycle";
		case KEY_BRIGHTNESS_AUTO:  return "Brightness auto";
		case KEY_DISPLAY_OFF:      return "Display off";
		case KEY_WWAN:             return "Wwan";
		case KEY_RFKILL:           return "Rfkill";
		case KEY_MICMUTE:          return "Micmute";
		default:                   return "Unknown";
	}
}

// ------------------------------
// Internal functions.
// ------------------------------

bool
wayclient__draw(wayclient_state *state) {
	int buffer_index = wayclient__first_released_buffer_index(state);
	if (buffer_index < 0) {
		// Simply commit the currently attached buffer, so compositor thinks
		// that we drew something and calls "frame" callback of the surface.
		wl_surface_commit(state->wl_surface);
		return false;
	}

	wayclient_buffer *buffer = &state->buffers[buffer_index];

	if (state->draw != NULL) state->draw(
		state,
		buffer->data,
		state->width * state->height * WAYCLIENT_PIXEL_SIZE,
		state->userdata
	);

	buffer->attached = true;
	wl_surface_attach(state->wl_surface, buffer->wl_buffer, 0, 0);
	wl_surface_damage_buffer(state->wl_surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->wl_surface);

	return true;
}

void
wayclient__update_buffers_size(wayclient_state *state) {
	if (state->width < 1) state->width = 1;
	if (state->height < 1) state->height = 1;
	if (state->prev_width == state->width && state->prev_height == state->height) return;

	wayclient__destroy_and_unmap_buffers(state);

	int stride = state->width * WAYCLIENT_PIXEL_SIZE;
	int size = state->height * stride;
	int pool_size = size * WAYCLIENT_BUFFER_COUNT;

	int fd = wayclient__create_shm_file(pool_size);
	assert(fd >= 0); // TODO: handle error.

	state->pool_data = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	assert(state->pool_data != MAP_FAILED); // TODO: handle error.

	struct wl_shm_pool *wl_pool = wl_shm_create_pool(state->wl_shm, fd, pool_size);
	assert(wl_pool != NULL); // TODO: handle error.

	for (int i = 0; i < WAYCLIENT_BUFFER_COUNT; i ++) {
		wayclient_buffer *buffer = &state->buffers[i];
		*buffer = (wayclient_buffer){0};

		int offset = size * i;
		buffer->wl_buffer = wl_shm_pool_create_buffer(
			wl_pool,
			offset,
			state->width,
			state->height,
			stride,
			WAYCLIENT_PIXEL_FORMAT
		);
		assert(buffer->wl_buffer != NULL); // TODO: handle error.

		buffer->data = state->pool_data + offset;

		wl_buffer_add_listener(buffer->wl_buffer, &wayclient__wl_buffer_listener, state);
	}

	wl_shm_pool_destroy(wl_pool);
	close(fd);

	if (state->on_resize != NULL) state->on_resize(state, state->userdata);

	state->prev_width = state->width;
	state->prev_height = state->height;
}

uint32_t
wayclient__first_released_buffer_index(wayclient_state *state) {
	for (uint32_t i = 0; i < WAYCLIENT_BUFFER_COUNT; i ++) {
		if (!state->buffers[i].attached) {
			return i;
		}
	}

	return -1;
}

void
wayclient__destroy_and_unmap_buffers(wayclient_state *state) {
	for (int i = 0; i < WAYCLIENT_BUFFER_COUNT; i ++) {
		wayclient_buffer *buffer = &state->buffers[i];
		if (buffer->wl_buffer == NULL) continue;

		wl_buffer_destroy(buffer->wl_buffer);
		buffer->wl_buffer = NULL;
	}

	if (state->pool_data != NULL) {
		size_t pool_size = state->prev_width * state->prev_height * WAYCLIENT_PIXEL_SIZE * WAYCLIENT_BUFFER_COUNT;
		munmap(state->pool_data, pool_size);
		state->pool_data = NULL;
	}
}

// `wayclient__randname` and `wayclient__create_shm_file` are copied from
// https://wayland-book.com/surfaces/shared-memory.html

void
wayclient__randname(char *buf, int n) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < n; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

int
wayclient__create_shm_file(size_t size) {
	int retries = 100;
	do {
		--retries;

		char name[] = "/wayclient_shm-XXXXXX";
		wayclient__randname(name + sizeof(name) - 7, 6);

		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd < 0) continue;

		shm_unlink(name);

		if (ftruncate(fd, size) < 0) {
			// Do not try again and just return, because something went
			// horribly wrong.
			close(fd);
			return -1;
		}

		return fd;
	} while (retries > 0 && errno == EEXIST);
	return -1;
}
