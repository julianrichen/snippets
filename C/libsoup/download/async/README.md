# Asynchronous File Download with libsoup
Code example for downloading files in parallel by running asynchronous libsoup
requests. Each download runs it's own GMainLoop but refs the current
GMainContext, if one exists using g_main_context_ref_thread_default(). If no
current GMainContext exists then a new one is created. If you plan on using
downloads inside of an application you should run that application first either
with a GMainLoop or using GApplication so I can hook onto the main context.

Documentation is available in the code via gtk-doc.

# Features
* Asynchronous resource fetch
* Asynchronous resource download (with byte buffer)
* Asynchronous resource write
* Fully GCancellable
* Progress function callback
* Final function callback
* Functions are documented with gtk-doc

# Libraries
## Core

    glib-2.0
    gio-2.0
    libsoup-2.4

## Building

    [your preferred compiler]
    meson

# Build
This code comes with a simple example & build script, however, it won't show the
asynchronous aspect of the downloader since the example asks for one URL as an
argument. Incorporate it into your project to see it in action.

	mkdir -p build
	meson . build
	cd build
	ninja
	./download-async URL
