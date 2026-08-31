#include <stdio.h>
#include <assert.h>

#include <wayclient.h>

int main() {
	wayclient_state state;
	assert(wayclient_init(&state, 256, 256) == WAYCLIENT_OK);

	wayclient_set_title(&state, "basic");

	while (wl_display_dispatch(state.wl_display) != -1) {
		if (state.should_close) break;
	}

	wayclient_destroy(&state);

	return 0;
}
