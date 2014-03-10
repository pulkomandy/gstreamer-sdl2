gstreamer-sdl2
==============

GStreamer rendering to an SDL2 texture

This code is very ugly, lacks some error checking, and has some (known and unknown) bugs. Use with caution.

We use the videotestsrc source to generate a video test pattern, and then use
an app\_sink to extract frames and put them on the display using an SDL2
SDL\_Texture. Unlike existing GStreamer sinks, which hijack the window and use
XV to render to it directly, this can be easily mixed with other things going
on in an SDL2 application. However, this may be slower.
