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
static GstElement* app_sink;

// MUST match video size
static const int WIDTH = 560;
static const int HEIGHT = 320;
static const int BPP = 1;

/* Callback: The appsink has received a buffer */
static GstFlowReturn new_buffer (GstAppSink *app_sink, gpointer /*data*/) {
	// Get the frame from video stream
	GstBuffer* sample = gst_app_sink_pull_buffer(app_sink);
	if(!sample) {
		// Finished playing.
		return GST_FLOW_ERROR;
	}

	// Get the frame format.
	GstCaps* caps = gst_buffer_get_caps (sample);
	if (!caps) {
		g_print ("could not get snapshot format\n");
		exit (-1);
	}

#if 0
	// for debugging, print it to the standard output
	// The video format here MUST match the SDL texture format, because we
	// don't want to use gstreamer videoconvert to do the conversion. We want
	// to do it in hardware using OpenGL.
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
		GST_ROUND_UP_4(width * BPP));

	gst_buffer_unref(sample);

	return GST_FLOW_OK;
}
  

static void pad_added_handler (GstElement *src, GstPad *new_pad, void *) {
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

	/* Check the new pad's type */
	new_pad_caps = gst_pad_get_caps (new_pad);
	new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
	new_pad_type = gst_structure_get_name (new_pad_struct);
	
	// We are only interested in the video pad. Look for it...
	if (g_str_has_prefix (new_pad_type, "video/x-raw"))
	{
		/* Attempt the link */
		GstPad *sink_pad_video = gst_element_get_static_pad (app_sink, "sink");
    	ret = gst_pad_link (new_pad, sink_pad_video);
    	if (GST_PAD_LINK_FAILED (ret)) {
      		g_print ("  Type is '%s' but link failed.\n", new_pad_type);
    	} else {
      		g_print ("  Link succeeded (type '%s').\n", new_pad_type);
    	} 

  		/* Unreference the sink pad */
		gst_object_unref (sink_pad_video);
  	} else {
    	g_print ("  It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
    	gst_caps_unref (new_pad_caps);
  	}

}


static GstElement* makeSource()
{
#if 0
	// Using the "video test source"
	GstElement* source = gst_element_factory_make ("videotestsrc", "source");
	g_object_set (source, "pattern", 0, NULL);
#else
	// Using an actual video file
	GstElement* source = gst_element_factory_make ("uridecodebin", "source");
	// Warning: the URI must be absolute.
	g_object_set(source, "uri", "file:/home/pulkomandy/small.ogv", NULL);
  	g_signal_connect (source, "pad-added", G_CALLBACK (pad_added_handler), NULL);
#endif
	
	return source;
}


// Work function - this is where all the fun happens!
static bool texture()
{
	// Create the pipeline and add all the elements to it

	// PIPELINE ---------------------------------------------------------------
	GstElement* pipeline = gst_pipeline_new("test-pipeline");

	// SOURCE -----------------------------------------------------------------
	GstElement* source = makeSource();
	if (source == nullptr) {
		g_printerr("Unable to create source element.\n");
		return -6;
	}
	if (!gst_bin_add(GST_BIN(pipeline), source)) {
		g_printerr("Unable to add source element to the pipeline.\n");
		return -4;
	}

	// SINK -------------------------------------------------------------------
	app_sink = gst_element_factory_make ("appsink", "app_sink");

#if 0
	// Setup app_sink callbacks
	// SDL2 isn't thread safe, so, the callbacks must not call SDL methods!
	// DONT DO THIS. We will call newBuffer directly, not from a callback.
	GstAppSinkCallbacks callbacks {NULL, NULL, new_buffer, NULL, 0};
	gst_app_sink_set_callbacks(app_sink, &callbacks, NULL, NULL);
#endif
  
	if (app_sink == nullptr) {
		g_printerr("Unable to create sink element.\n");
		return -7;
	}
	if (!gst_bin_add(GST_BIN(pipeline), app_sink)) {
		g_printerr("Unable to add sink element to the pipeline.\n");
		return -5;
	}

	// Connect the source to the app_sink
#if 0
	// Using the "videotestsrc" - static connection
	if (!gst_element_link(source, (GstElement*)app_sink)) {
		g_printerr("Unable to link the source to the sink.\n");
		return -3;
		gst_object_unref (pipeline);
	}
#else
	// Using the "uridecodebin" - connctions must be done dynamically, when the
	// pads are created.
#endif

	/* Start playing */
	if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		gst_object_unref (pipeline);
		return -1;
	}

	// TODO texture size and format should match the video!
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STATIC, WIDTH, HEIGHT);
	if (tex == nullptr){
		std::cout << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
		return -9;
	}

	// TODO how do we wait for the playing to end?
	bool playing = true;
	puts("Everything initialized ok, starting replay...");
	while(playing)
	{
		SDL_RenderClear(ren);
		if (new_buffer((GstAppSink*)app_sink, 0) != GST_FLOW_OK)
			playing = false;
		else {
			SDL_RenderCopy(ren, tex, NULL, NULL);
			SDL_RenderPresent(ren);
		}
	}
	puts("Playing video finished. Cleaning up and leaving.");

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
	SDL_Window *win = SDL_CreateWindow("GStreamer-SDL2", 100, 100, WIDTH, HEIGHT,
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
