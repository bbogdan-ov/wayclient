#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <wayclient.h>

void draw(
	wayclient_state *state,
	uint8_t *pixel_data,
	size_t pixel_data_size,
	void *userdata
) {
	uint32_t *pixels = (uint32_t*)pixel_data;

	//           0xAARRGGBB
	#define RED  0xFFff0000
	#define BLUE 0xFF0000ff

	// Draw checkerboard pattern.
	for (int y = 0; y < state->height; y ++) {
		for (int x = 0; x < state->width; x ++) {
			uint32_t color = RED;
			int xx = x / 128;
			int yy = y / 128;
			if ((xx + yy) % 2 == 0) color = BLUE;
			pixels[x + y * state->width] = color;
		}
	}
}

void on_resize(wayclient_state *state, void *userdata) {
	printf("Resized %dx%d\n", state->width, state->height);
}

void on_pointer_enter(wayclient_state *state, void *userdata) {
	printf("Pointer enter\n");
}
void on_pointer_leave(wayclient_state *state, void *userdata) {
	printf("Pointer leave\n");
}
void on_pointer_motion(wayclient_state *state, double x, double y, void *userdata) {
	printf("Pointer %f, %f\n", x, y);
}
void on_pointer_scroll(wayclient_state *state, double x, double y, void *userdata) {
	printf("Pointer scroll %f, %f\n", x, y);
}
void on_pointer_button(
	wayclient_state *state,
	uint32_t button,
	enum wl_pointer_button_state button_state,
	void *userdata
) {
	const char *button_str;
	const char *state_str;

	switch (button) {
	case BTN_LEFT:    button_str = "left"; break;
	case BTN_MIDDLE:  button_str = "middle"; break;
	case BTN_RIGHT:   button_str = "right"; break;
	case BTN_SIDE:    button_str = "side"; break;
	case BTN_EXTRA:   button_str = "extra"; break;
	case BTN_FORWARD: button_str = "forward"; break;
	case BTN_BACK:    button_str = "back"; break;
	case BTN_TASK:    button_str = "task"; break;
	default:          button_str = "unknown"; break;
	}

	switch (button_state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:  state_str = "pressed"; break;
	case WL_POINTER_BUTTON_STATE_RELEASED: state_str = "released"; break;
	default:                               state_str = "unknown"; break;
	}

	printf("Pointer button: %s, state = %s\n", button_str, state_str);
}

void on_keyboard_enter(wayclient_state *state, void *userdata) {
	printf("keyboard enter\n");
}
void on_keyboard_leave(wayclient_state *state, void *userdata) {
	printf("Keyboard leave\n");
}
void on_keyboard_key(
	wayclient_state *state,
	uint32_t keycode,
	xkb_keysym_t keysym,
	enum wl_keyboard_key_state key_state,
	void *userdata
) {
	const char *state_str;
	switch (key_state) {
	case WL_KEYBOARD_KEY_STATE_PRESSED:  state_str = "pressed"; break;
	case WL_KEYBOARD_KEY_STATE_RELEASED: state_str = "released"; break;
	case WL_KEYBOARD_KEY_STATE_REPEATED: state_str = "repeated"; break; // Repeated event is not implement yet...
	default:                             state_str = "unknown"; break;
	}

	// I would describe the difference between `keycode` and `keysym` as:
	// - Keysym depends on the current layout and currently pressed modifiers
	//   (shift, capslock, etc) and used to determine the actual that is being
	//   pressed (to use for a text field in your app for example).
	// - Keycode is layout and state agnostic. Pressing 'Ц' with russian
	//   layout (on a classic QUERTY keyboard) will always result in KEY_W.

	char name_buf[64];
	int name_len = xkb_keysym_get_name(keysym, name_buf, sizeof(name_buf));

	char char_buf[4];
	// NOTE: using `keycode` here instead of `keysym` and add 8 because Wayland requires to do so.
	int char_len = xkb_state_key_get_utf8(state->xkb_state, keycode + 8, char_buf, sizeof(char_buf));
	if (char_buf[0] == 0) char_len = 0;

	const char *key_str = wayclient_keycode_to_str(keycode);

	printf(
		"Keyboard key: %s, keysym = %.*s, utf8_char = %.*s, state = %s\n",
		key_str,
		name_len, name_buf,
		char_len, char_buf,
		state_str
	);
}

int main() {
	wayclient_state state;
	wayclient_init(&state, 512, 512);

	wayclient_error err = wayclient_run(&state);
	assert(err == WAYCLIENT_OK);

	state.draw = draw;
	state.on_resize = on_resize;
	state.on_pointer_enter = on_pointer_enter;
	state.on_pointer_leave = on_pointer_leave;
	state.on_pointer_motion = on_pointer_motion;
	state.on_pointer_button = on_pointer_button;
	state.on_pointer_scroll = on_pointer_scroll;
	state.on_keyboard_enter = on_keyboard_enter;
	state.on_keyboard_leave = on_keyboard_leave;
	state.on_keyboard_key = on_keyboard_key;

	xdg_toplevel_set_title(state.xdg_toplevel, "Basic example");
	xdg_toplevel_set_app_id(state.xdg_toplevel, "basic_example");

	while (wl_display_dispatch(state.wl_display) && !state.should_close) {}

	wayclient_destroy(&state);

	return 0;
}
