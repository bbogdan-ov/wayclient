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
			wayclient_logf("Bound %s: %p", iface.name, state->field); \
			return; \
		}

	BIND(wl_compositor, wl_compositor_interface);
	BIND(wl_shm,        wl_shm_interface);
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

wayclient_error
wayclient_init(wayclient_state *state, uint32_t width, uint32_t height) {
	memset(state, 0, sizeof(wayclient_state));

	state->width = width;
	state->height = height;

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

	if (state->wl_frame_callback != NULL)
		wl_callback_destroy(state->wl_frame_callback);

	xdg_toplevel_destroy(state->xdg_toplevel);
	xdg_surface_destroy(state->xdg_surface);
	wl_surface_destroy(state->wl_surface);
	xdg_wm_base_destroy(state->xdg_wm_base);
	wl_shm_destroy(state->wl_shm);
	wl_compositor_destroy(state->wl_compositor);
	wl_registry_destroy(state->wl_registry);
	wl_display_disconnect(state->wl_display);
}

void
wayclient_set_title(wayclient_state *state, const char *title) {
	xdg_toplevel_set_title(state->xdg_toplevel, title);
}

void
wayclient_set_app_id(wayclient_state *state, const char *app_id) {
	xdg_toplevel_set_app_id(state->xdg_toplevel, app_id);
}

void
wayclient_set_size(wayclient_state *state, uint32_t width, uint32_t height) {
	assert(false); // TODO!!:
}

void
wayclient_set_max_size(wayclient_state *state, uint32_t max_width, uint32_t max_height) {
	xdg_toplevel_set_max_size(state->xdg_toplevel, max_width, max_height);
}

void
wayclient_set_min_size(wayclient_state *state, uint32_t min_width, uint32_t min_height) {
	xdg_toplevel_set_min_size(state->xdg_toplevel, min_width, min_height);
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
