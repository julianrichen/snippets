# Asynchronous File Download with libsoup
Downloads files in parallel by running asynchronous file download with the help
of libsoup. Each download runs it's own GMainLoop but refs the current
GMainContext, if one exists using g_main_context_ref_thread_default(). If no
current GMainContext exists then a new one is created. If you plan on using
downloads inside of an application you should run that application first either
with a GMainLoop or using GApplication so I can hook onto the main context.

Documentation is available in the code via gtk-doc.

# Build
This code comes with a simple example & build script, however, it won't show the
asynchronous aspect of the downloader. Incorporate it into your project to see
it in action.
	mkdir -p build
	meson . build
	cd build
	ninja
	./download-async URL
