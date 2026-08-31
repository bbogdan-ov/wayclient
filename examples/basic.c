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

int main() {
	wayclient_state state;
	wayclient_init(&state, 512, 512);

	wayclient_error err = wayclient_run(&state);
	assert(err == WAYCLIENT_OK);

	state.draw = draw;
	state.on_resize = on_resize;

	xdg_toplevel_set_title(state.xdg_toplevel, "Basic example");
	xdg_toplevel_set_app_id(state.xdg_toplevel, "basic_example");

	while (wl_display_dispatch(state.wl_display) && !state.should_close) {}

	wayclient_destroy(&state);

	return 0;
}
