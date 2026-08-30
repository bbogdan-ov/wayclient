#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <wayland-client.h>
#include "protocols/xdg-shell.h"

#define wayclient_log_info(msg) fprintf(stderr, "INFO: "msg"\n")
#define wayclient_log_infof(fmt, ...) fprintf(stderr, "INFO: "fmt"\n", __VA_ARGS__)
#define wayclient_log_error(msg) fprintf(stderr, "ERROR: "msg"\n")
#define wayclient_log_errorf(fmt, ...) fprintf(stderr, "ERROR: "fmt"\n", __VA_ARGS__)

// Number of components in a pixel. (ARGB)
#define WAYCLIENT_PIXEL_SIZE 4
#define WAYCLIENT_PIXEL_FORMAT WL_SHM_FORMAT_ARGB8888
static_assert(WAYCLIENT_PIXEL_SIZE == 4); // Just so i don't forget to update `WAYCLIENT_PIXEL_SIZE` if i change the pixel format.

#define WAYCLIENT_BUFFERS 2

#define WAYCLIENT_DEFAULT_WIDTH 256
#define WAYCLIENT_DEFAULT_HEIGHT 256

typedef struct {
	struct wl_buffer *wl_buffer;
	uint8_t          *data; // Pixel data of the wl_buffer.
	bool             attached;
} wayclient_buffer;

typedef struct {
	struct wl_display    *wl_display;
	struct wl_compositor *wl_compositor;
	struct wl_surface    *wl_surface;
	struct wl_shm        *wl_shm;
	struct wl_callback   *frame_callback;
	uint8_t              *pool_data;
	wayclient_buffer     buffers[WAYCLIENT_BUFFERS];

	struct xdg_wm_base   *xdg_wm_base;
	struct xdg_surface   *xdg_surface;
	struct xdg_toplevel  *xdg_toplevel;

	uint32_t             width, height;
	uint32_t             frame;
	bool                 should_close;
} wayclient_state;

static void TODO_resize(wayclient_state *state, uint32_t new_width, uint32_t new_height);
static void TODO_draw(wayclient_state *state, int buffer_index);
static int  TODO_next_buffer(wayclient_state *state);

static bool
TODO_draw_current_frame(wayclient_state *state) {
	int buffer_index = TODO_next_buffer(state);
	if (buffer_index < 0) {
		// Simply commit the currently attached buffer, so compositor thinks
		// that we drew something and calls "frame" callback of the surface.
		wl_surface_commit(state->wl_surface);
		return false;
	}

	TODO_draw(state, buffer_index);
	return true;
}

static bool
TODO_draw_next_frame(wayclient_state *state) {
	if (state->frame % 60 == 0) {
		wayclient_log_info("60th frame");
	}

	bool ok = TODO_draw_current_frame(state);
	if (!ok) {
		wayclient_log_infof("Frame dropped: %d", state->frame);
	}

	state->frame += 1;
	return ok;
}

// ------------------------------
// SHM.
// ------------------------------

// Copied from https://wayland-book.com/surfaces/shared-memory.html
static void
randname(char *buf, int n) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < n; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int
create_shm_file(void) {
	int retries = 100;
	do {
		char name[] = "/wayclient_shm-XXXXXX";
		randname(name + sizeof(name) - 7, 6);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}

int
allocate_shm_file(size_t size) {
	int fd = create_shm_file();
	if (fd < 0) return -1;

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

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

	wayclient_log_infof(
		"Interface: %s, name = %d, version = %d",
		interface,
		name,
		version
	);

	#define BIND(field, iface) \
		if (strcmp(interface, iface.name) == 0) { \
			state->field = wl_registry_bind(wl_registry, name, &iface, version); \
			wayclient_log_infof("    Bound: %p", state->field); \
			return; \
		}

	BIND(wl_compositor, wl_compositor_interface);
	BIND(wl_shm,        wl_shm_interface);
	BIND(xdg_wm_base,   xdg_wm_base_interface);
}

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
// XDG WM base listener.
// ------------------------------

static void
wayclient__xdg_wm_base_handle_ping(
	void *data,
	struct xdg_wm_base *xdg_wm_base,
	uint32_t serial
) {
	xdg_wm_base_pong(xdg_wm_base, serial);
	wayclient_log_info("Event: xdg_wm_base.ping");
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
	state->frame_callback = wl_callback;

	TODO_draw_next_frame(state);
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
	wayclient_log_info("Event: xdg_surface.configure");

	wayclient_state *state = data;

	xdg_surface_ack_configure(xdg_surface, serial);

	if (state->pool_data == NULL) {
		TODO_resize(state, WAYCLIENT_DEFAULT_WIDTH, WAYCLIENT_DEFAULT_HEIGHT);
	}

	TODO_draw_current_frame(state);
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
	wayclient_log_info("Event: xdg_toplevel.configure");

	wayclient_state *state = data;
	TODO_resize(state, width, height);
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

	for (int i = 0; i < WAYCLIENT_BUFFERS; i ++) {
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
// Drawing.
// ------------------------------

static void
wayclient__destroy_and_unmap_buffers(wayclient_state *state) {
	// NOTE: it is important to destroy buffers BEFORE unmapping pool data they
	// depend on, because buffers may need to access this pool data before
	// being destroyed.
	for (int i = 0; i < WAYCLIENT_BUFFERS; i ++) {
		wayclient_buffer *buffer = &state->buffers[i];
		if (buffer->wl_buffer == NULL) continue;

		wl_buffer_destroy(buffer->wl_buffer);
		buffer->wl_buffer = NULL;
	}

	// Unmap previous pool data.
	if (state->pool_data != NULL) {
		munmap(state->pool_data, state->width * state->height * WAYCLIENT_PIXEL_SIZE * WAYCLIENT_BUFFERS);
		state->pool_data = NULL;
	}
}

static void
TODO_resize(wayclient_state *state, uint32_t new_width, uint32_t new_height) {
	if (new_width == 0 && new_height == 0) return;
	if (new_width == state->width && new_height == state->height) return;

	wayclient__destroy_and_unmap_buffers(state);

	int stride = new_width * WAYCLIENT_PIXEL_SIZE;
	int size = new_height * stride;
	int pool_size = size * WAYCLIENT_BUFFERS;

	int fd = allocate_shm_file(pool_size);
	assert(fd >= 0); // TODO: handle error.

	state->pool_data = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	struct wl_shm_pool *wl_pool = wl_shm_create_pool(state->wl_shm, fd, pool_size);
	assert(wl_pool != NULL); // TODO: handle error.

	for (int i = 0; i < WAYCLIENT_BUFFERS; i ++) {
		wayclient_buffer *buffer = &state->buffers[i];
		*buffer = (wayclient_buffer){0};

		int offset = size * i;
		buffer->wl_buffer = wl_shm_pool_create_buffer(
			wl_pool,
			offset,
			new_width,
			new_height,
			stride,
			WAYCLIENT_PIXEL_FORMAT
		);
		assert(buffer->wl_buffer != NULL); // TODO: handle error.

		buffer->data = state->pool_data + offset;

		wl_buffer_add_listener(buffer->wl_buffer, &wayclient__wl_buffer_listener, state);
	}

	wl_shm_pool_destroy(wl_pool);
	close(fd);

	wayclient_log_infof("Resized: %dx%d", new_width, new_height);

	state->width = new_width;
	state->height = new_height;
}

static int
TODO_next_buffer(wayclient_state *state) {
	for (int i = 0; i < WAYCLIENT_BUFFERS; i ++) {
		if (!state->buffers[i].attached) {
			return i;
		}
	}

	return -1;
}

static void
TODO_draw(wayclient_state *state, int buffer_index) {
	assert(buffer_index >= 0);

	wayclient_buffer *buffer = &state->buffers[buffer_index];

	uint32_t *pixels = (uint32_t*)buffer->data;
	memset(pixels, 0xff000000, state->width * state->height * WAYCLIENT_PIXEL_SIZE);

	for (int y = 0; y < state->height; y ++) {
		for (int x = 0; x < state->width; x ++) {
			uint32_t color = 0xff111111;
			int xx = (x + state->frame) / 128;
			int yy = (y + state->frame) / 128;
			if ((xx + yy) % 2 == 0) color = 0xffeeeeee;
			pixels[x + y * state->width] = color;
		}
	}

	buffer->attached = true;
	wl_surface_attach(state->wl_surface, buffer->wl_buffer, 0, 0);
	wl_surface_damage_buffer(state->wl_surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->wl_surface);
}

// ------------------------------
// Main.
// ------------------------------

int
main() {
	wayclient_state state = {0};

	state.wl_display = wl_display_connect(NULL);
	assert(state.wl_display != NULL); // TODO: handle error.

	struct wl_registry *wl_registry = wl_display_get_registry(state.wl_display);
	assert(wl_registry != NULL); // TODO: handle error.

	wl_registry_add_listener(wl_registry, &wayclient__wl_registry_listener, &state);
	wl_display_roundtrip(state.wl_display);

	// Expect these regsitry objects to be presented by the compositor:
	assert(state.wl_compositor != NULL); // TODO: handle error.
	assert(state.wl_shm != NULL); // TODO: handle error.
	assert(state.xdg_wm_base != NULL); // TODO: handle error.

	xdg_wm_base_add_listener(state.xdg_wm_base, &wayclient__xdg_wm_base_listener, &state);

	state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
	assert(state.wl_surface != NULL); // TODO: handle error.

	state.frame_callback = wl_surface_frame(state.wl_surface);
	wl_callback_add_listener(state.frame_callback, &wayclient__frame_callback_listener, &state);

	state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);
	assert(state.xdg_surface != NULL); // TODO: handle error.

	xdg_surface_add_listener(state.xdg_surface, &wayclient__xdg_surface_listener, &state);

	state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
	assert(state.xdg_toplevel != NULL); // TODO: handle error.

	xdg_toplevel_add_listener(state.xdg_toplevel, &wayclient__xdg_toplevel_listener, &state);
	xdg_toplevel_set_title(state.xdg_toplevel, "Wayclient example");

	wl_surface_commit(state.wl_surface);

	// Event loop.
	while (wl_display_dispatch(state.wl_display) != -1) {
		if (state.should_close) break;
	}

	wayclient_log_info("Bye");

	wayclient__destroy_and_unmap_buffers(&state);

	wl_callback_destroy(state.frame_callback);
	xdg_toplevel_destroy(state.xdg_toplevel);
	xdg_surface_destroy(state.xdg_surface);
	wl_surface_destroy(state.wl_surface);
	xdg_wm_base_destroy(state.xdg_wm_base);
	wl_shm_destroy(state.wl_shm);
	wl_compositor_destroy(state.wl_compositor);
	wl_registry_destroy(wl_registry);
	wl_display_disconnect(state.wl_display);

	return 0;
}
