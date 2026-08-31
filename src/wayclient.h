#ifndef WAYCLIENT_H
#define WAYCLIENT_H

#include <stdint.h>

#include <wayland-client.h>
#include "xdg_shell.h"

#define wayclient_log(msg)       fprintf(stderr, "WAYCLIENT: "msg"\n")
#define wayclient_logf(fmt, ...) fprintf(stderr, "WAYCLIENT: "fmt"\n", __VA_ARGS__)

// Format of a pixel.
#define WAYCLIENT_PIXEL_FORMAT WL_SHM_FORMAT_ARGB8888
// Number of components in a pixel. (ARGB)
#define WAYCLIENT_PIXEL_SIZE 4

// Double-buffered by default.
#define WAYCLIENT_BUFFER_COUNT 2

// ------------------------------
// Types.
// ------------------------------

typedef enum {
	WAYCLIENT_OK = 0,
	WAYCLIENT_ERR_CONNECT,
	WAYCLIENT_ERR_GET_OBJECTS,
	WAYCLIENT_ERR_CREATE_SURFACE,
	WAYCLIENT_ERR_CREATE_TOPLEVEL,
} wayclient_error;

typedef struct {
	struct wl_buffer *wl_buffer;
	uint8_t          *data; // Pixel data of the wl_buffer.
	bool             attached; // Whether the buffer is being used.
} wayclient_buffer;

typedef struct {
	struct wl_display    *wl_display;
	struct wl_registry   *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_surface    *wl_surface;
	struct wl_shm        *wl_shm;
	uint8_t              *pool_data;
	wayclient_buffer     buffers[WAYCLIENT_BUFFER_COUNT];

	struct wl_callback   *wl_frame_callback;

	struct xdg_wm_base   *xdg_wm_base;
	struct xdg_surface   *xdg_surface;
	struct xdg_toplevel  *xdg_toplevel;

	uint32_t             width, height;
	uint32_t             prev_width, prev_height;
	bool                 should_close;
} wayclient_state;

// ------------------------------
// Public functions.
// ------------------------------

// Initialize a state for a top-level XDG window.
wayclient_error
wayclient_init(wayclient_state *state, uint32_t width, uint32_t height);

// Clean up the memory and disconnect from the Wayland display.
void
wayclient_destroy(wayclient_state *state);

void
wayclient_set_title(wayclient_state *state, const char *title);

void
wayclient_set_app_id(wayclient_state *state, const char *app_id);

void
wayclient_set_size(wayclient_state *state, uint32_t width, uint32_t height);

void
wayclient_set_max_size(wayclient_state *state, uint32_t max_width, uint32_t max_height);

void
wayclient_set_min_size(wayclient_state *state, uint32_t min_width, uint32_t min_height);

// ------------------------------
// Internal functions.
// ------------------------------

// Call user draw callback, damage and commit the surface to the compositor.
// Returns whether the draw call was cancelled due to all buffers are being
// used by the compositor.
bool
wayclient__draw(wayclient_state *state);

// Returns the index of the first buffer that is not used by the compositor.
// Returns -1 if all buffers are being used.
uint32_t
wayclient__first_released_buffer_index(wayclient_state *state);

// Update buffers and SHM pool accordingly to the current window size.
void
wayclient__update_buffers_size(wayclient_state *state);

void
wayclient__destroy_and_unmap_buffers(wayclient_state *state);

void
wayclient__randname(char *buf, int n);

int
wayclient__create_shm_file(size_t size);

#endif
