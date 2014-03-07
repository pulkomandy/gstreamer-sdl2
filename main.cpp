/* Copyright 2014, Adrien Destugues <pulkomandy@pulkomandy.tk>
 * This file is distributed under the terms of the MIT License.
 */

#include <iostream>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <SDL2/SDL.h>


// These will be initialized from the main code, then used from the callback.
// They can e moved to a struct and passed as the callback argument, if you
// don't like global variables.
static SDL_Renderer* ren;
static SDL_Texture* tex;
static GstBuffer* sample;


/* Callback: The appsink has received a buffer */
static GstFlowReturn new_buffer (GstAppSink *app_sink, gpointer /*data*/) {
	// Get the frame from video stream
	sample = gst_app_sink_pull_buffer(app_sink);

	// Get the frame format.
	GstCaps* caps = gst_buffer_get_caps (sample);
	if (!caps) {
		g_print ("could not get snapshot format\n");
		exit (-1);
	}

#if 0
	// for debugging, print it to the standard output
	printf("caps are %s\n", gst_caps_to_string(caps));
#endif

	// Extract the width and height of the frame
	gint width, height;
	GstStructure* s = gst_caps_get_structure (caps, 0);
	int res = gst_structure_get_int (s, "width", &width)
		| gst_structure_get_int (s, "height", &height);
	if (!res) {
		g_print ("could not get snapshot dimension\n");
		exit (-1);
	}

	// Update the texture with the received data.
	SDL_Rect r{0, 0, width, height};
	SDL_UpdateTexture(tex, &r, GST_BUFFER_DATA(sample),
		GST_ROUND_UP_4(width * 2));

	gst_buffer_unref(sample);

	return GST_FLOW_OK;
}
  

static GstElement* makeSource()
{
	GstElement* source = gst_element_factory_make ("videotestsrc", "source");
	g_object_set (source, "pattern", 0, NULL);

	//GstElement* source = gst_element_factory_make ("uridecodebin", "source");
	// TODO set the URI for our video file
}


// Work function - this is where all the fun happens!
static bool texture()
{
	// SOURCE -----------------------------------------------------------------
	GstElement* source = makeSource();

	// SINK -------------------------------------------------------------------
	GstAppSink* app_sink = (GstAppSink*)gst_element_factory_make ("appsink",
		"app_sink");

	// Setup app_sink callbacks
	// SDL2 isn't thread safe, so, the callbacks must not call SDL methods!
#if 0
	GstAppSinkCallbacks callbacks {NULL, NULL, new_buffer, NULL, 0};
	gst_app_sink_set_callbacks(app_sink, &callbacks, NULL, NULL);
#endif
  
	// PIPELINE ---------------------------------------------------------------
	// Connect the source to the app_sink
	GstElement* pipeline = gst_pipeline_new ("test-pipeline");
	gst_bin_add_many (GST_BIN (pipeline), source, app_sink, NULL);
	gst_element_link(source, (GstElement*)app_sink);

	/* Start playing */
	GstStateChangeReturn ret;
	ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		gst_object_unref (pipeline);
		return -1;
	}

	// TODO texture size and format should match the video!
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_YUY2,
		SDL_TEXTUREACCESS_STATIC, 320, 240);
	if (tex == nullptr){
		std::cout << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
		return false;
	}

	// TODO how do we wait for the playing to end?
	for(;;)
	{
		SDL_RenderClear(ren);
		new_buffer(app_sink, 0);
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	SDL_DestroyTexture(tex);

	// When we get here, the program is done.
	gst_object_unref(pipeline);

	return true;
}


// Entry point
int main(int argc, char **argv){
	int ret = 0;

	// Initialize GStreamer
	gst_init (&argc, &argv);

	// Initialize SDL2
	if (SDL_Init(SDL_INIT_VIDEO) != 0){
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	// Create window and renderer
	SDL_Window *win = SDL_CreateWindow("Hello World!", 100, 100, 320, 240,
		SDL_WINDOW_SHOWN);
	if (win == nullptr){
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

	ren = SDL_CreateRenderer(win, -1, 
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr){
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_DestroyWindow(win);
		SDL_Quit();
		return 1;
	}

	// Do work
	if (!texture())
		ret = 1;

	// Cleanup and teardown
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return ret;
}
