#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <wayclient.h>

const char *keycode_to_str(uint32_t keycode);

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

	const char *key_str = keycode_to_str(keycode);

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

const char *keycode_to_str(uint32_t keycode) {
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
