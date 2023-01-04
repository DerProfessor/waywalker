#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

struct wayw_server {
	struct wl_display *wl_display;

	struct wlr_backend *backend;

	struct wl_listener new_output;
	struct wlr_allocator *allocator;
	struct wlr_renderer *renderer;
	struct wlr_scene *scene;
	struct wlr_output_layout *output_layout;

	struct wl_list outputs; // wayw_output::link
};

struct wayw_output {
	struct wlr_output *wlr_output;
	struct wayw_server *server;
	struct timespec last_frame;
	float color[4];
	int dec;

	struct wl_listener destroy;
	struct wl_listener frame;

	struct wl_list link;
};

/*
static void output_frame_notify(struct wl_listener *listener, void *data) {
	struct wayw_output *output = wl_container_of(listener, output, frame);
	struct wlr_output *wlr_output = data;
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
			wlr_output->backend);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	// Calculate a color, just for pretty demo purposes
	long ms = (now.tv_sec - output->last_frame.tv_sec) * 1000 +
		(now.tv_nsec - output->last_frame.tv_nsec) / 1000000;
	int inc = (output->dec + 1) % 3;
	output->color[inc] += ms / 2000.0f;
	output->color[output->dec] -= ms / 2000.0f;
	if (output->color[output->dec] < 0.0f) {
		output->color[inc] = 1.0f;
		output->color[output->dec] = 0.0f;
		output->dec = inc;
	}
	// End pretty color calculation

	wlr_output_make_current(wlr_output, NULL);
	//TODO:FIX THIS
	wlr_renderer_begin(renderer, wlr_output);

	wlr_renderer_clear(renderer, &output->color);

	wlr_output_swap_buffers(wlr_output, NULL, NULL);
	wlr_renderer_end(renderer);

	output->last_frame = now;
}
*/

static void output_frame(struct wl_listener *listener, void *data)
{
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	struct wayw_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;
	
	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
		scene, output->wlr_output);
	
	/* Render the scene if needed and commit the output */
	wlr_scene_output_commit(scene_output);

	struct timespec now; //TODO: Geht das statt einem &?
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);

}

/*
static void output_destroy_notify(struct wl_listener *listener, void *data) {
	struct wayw_output *output = wl_container_of(listener, output, destroy);
	wl_list_remove(&output->link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->frame.link);
	free(output);
}
*/

static void server_new_output(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display
	 * or monitor) becomes available. */
	struct wayw_server *server = wl_container_of(
			listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/* use the output created by the backend to use our allocator
	 * and our renderer. must be done once, before commiting the output. */
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	/* some backends don't have modes but drm requires it. wayland and x11
	 * are example backends for this. output modes specify the following
	 * tuple: (width, height, refresh rate). more sophisticated compositors
	 * would let the user choose this, here it is set automatically. */
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
	}

	/* allocates and configures our state for the output. */
	struct wayw_output *output = calloc(1, sizeof(struct wayw_output));
	output->wlr_output = wlr_output;
	output->server = server;
	
	/* sets up a listener for the frame notify event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);

	/* adds this to the output layout. the add_auto function arranges outputs
	 * from left-to-right in the order they appear. a more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * the output layout utility automatically adds a wl_output global to the
	 * display, which wayland clients can see to find out information about the
	 * output (such as dpi, scale factor, manufacturer, etc).
	 */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}

int main(int argc, char **argv) {
	wlr_log_init(WLR_DEBUG, NULL);
	char *startup_cmd = NULL;

	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}
	
	struct wayw_server server;
	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	server.wl_display = wl_display_create();

	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
	server.backend = wlr_backend_autocreate(server.wl_display);

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	server.renderer = wlr_renderer_autocreate(server.backend);
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	server.output_layout = wlr_output_layout_create();

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	/* Create a scene graph. This is a wlroots abstraction that handles all
	 * rendering and damage tracking. All the compositor author needs to do
	 * is add things that should be rendered to the scene graph at the proper
	 * positions and then call wlr_scene_output_commit() to render a frame if
	 * necessary.
	 */
	server.scene = wlr_scene_create();
	wlr_scene_attach_output_layout(server.scene, server.output_layout);

	//TODO: Add xdg_shell, cursor, keyboards etc. from tinywl


	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

	/* Start the backend. This will enumerate outputs and inputs, become
	 * become the DRM master, etc*/
	if (!wlr_backend_start(server.backend)) {
		fprintf(stderr, "Failed to start backend\n");
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}
	printf("Running compositor on wayland display '%s'\n", socket);

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run
	 * and run startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void*)NULL);
		}
	}

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY %s",
		socket);
	wl_display_run(server.wl_display);
	
	/* Once wl_display_run returns, we shut down the server. */
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
