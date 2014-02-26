/* Copyright 2014, Adrien Destugues <pulkomandy@pulkomandy.tk>
 * This file is distributed under the terms of the MIT License.
 */

#include <iostream>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <SDL2/SDL.h>


static SDL_Renderer* ren;


/* Callback: The appsink has received a buffer */
static void new_buffer (GstElement *sink, void *data) {
  GstBuffer *buffer;
  
  /* Retrieve the buffer */
  g_signal_emit_by_name (sink, "pull-buffer", &buffer);
  if (buffer) {
    /* The only thing we do in this example is print a * to indicate a received buffer */
    g_print ("*");
    gst_buffer_unref (buffer);
  }

}
  

// Work function - this is where all the fun happens!
static bool texture()
{
	// SOURCE -----------------------------------------------------------------
	//GstElement* source = gst_element_factory_make ("uridecodebin", "source");
	// TODO set the URI for our video file
	GstElement* source = gst_element_factory_make ("videotestsrc", "source");

	// SINK -------------------------------------------------------------------
	GstAppSink* app_sink = (GstAppSink*)gst_element_factory_make ("appsink",
		"app_sink");

  /* Configure appsink for appropriate video format */
	GstCaps *caps = gst_caps_new_simple ("video/x-raw",
     "format", G_TYPE_STRING, "RGB",
/*
     "framerate", GST_TYPE_FRACTION, 25, 1,
     "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
     "width", G_TYPE_INT, 320,
     "height", G_TYPE_INT, 240,
*/
     NULL);

//  gst_app_sink_set_caps(app_sink, caps);
	gst_caps_unref(caps);
  
	// PIPELINE ---------------------------------------------------------------
	// Connect the source to the app_sink
	GstElement* pipeline = gst_pipeline_new ("test-pipeline");
	gst_bin_add_many (GST_BIN (pipeline), source, app_sink, NULL);
	gst_element_link(source, (GstElement*)app_sink);

  g_object_set (source, "pattern", 0, NULL);

  /* Start playing */
  GstStateChangeReturn ret;
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return -1;
  }

	// TODO texture size and format should match the video!
	SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_STATIC, 640, 480);
	if (tex == nullptr){
		std::cout << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
		return false;
	}

	int frame = 0;
	do {
		// Get the next frame from video stream
		GstBuffer* sample = gst_app_sink_pull_buffer(app_sink);
		printf("Processing frame %d...", frame);

		/* get the snapshot buffer format now. We set the caps on the appsink so
		* that it can only be an rgb buffer. The only thing we have not specified
		* on the caps is the height, which is dependant on the pixel-aspect-ratio
		* of the source material */
		GstCaps* caps = gst_buffer_get_caps (sample);
		if (!caps) {
			g_print ("could not get snapshot format\n");
			exit (-1);
		}
		GstStructure* s = gst_caps_get_structure (caps, 0);

		/* we need to get the final caps on the buffer to get the size */
		gint width, height;
		int res = gst_structure_get_int (s, "width", &width)
				| gst_structure_get_int (s, "height", &height);
		if (!res) {
			g_print ("could not get snapshot dimension\n");
			exit (-1);
		}

		SDL_Rect r{0, 0, width, height};
		SDL_UpdateTexture(tex, &r, GST_BUFFER_DATA(sample),
			GST_ROUND_UP_4(width * 2));
		SDL_RenderClear(ren);
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);

		gst_buffer_unref(sample);
	} while(frame++ < 100);


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
	SDL_Window *win = SDL_CreateWindow("Hello World!", 100, 100, 640, 480,
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
