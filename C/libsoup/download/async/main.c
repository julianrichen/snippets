#include "download-async.h"

#include <glib.h>

static void
progress (guint64   downloaded_bytes,
          guint64   total_bytes,
          gpointer  user_data)
{
    // This could be outputted into a GUI application or file, callback makes
    // it easy to work with progress
    g_print ("[ %06.2f%% ] %lu / %lu\n", (gfloat) (((gfloat) downloaded_bytes / (gfloat) total_bytes) * 100), downloaded_bytes, total_bytes);
}

static void
callback (gpointer user_data)
{
    g_print ("Operation finsihed");
}

static void
download (gchar *uri,
          gchar *path)
{
    GCancellable *cancellable = g_cancellable_new ();

    download_resource_from_uri_async_full (uri,
                                           path,
                                           TRUE,
                                           cancellable,
                                           progress,
                                           NULL,
                                           callback,
                                           NULL);
}

int
main (int    argc,
      char **argv)
{
    GMainLoop *loop = NULL;
    gchar *uri;

    loop = g_main_loop_new (NULL, FALSE);
    uri = g_strdup (argv[1]);

    // Example URL:
    //uri = "https://download.fedoraproject.org/pub/fedora/linux/releases/27/Workstation/x86_64/iso/Fedora-Workstation-Live-x86_64-27-1.6.iso";

    if (!uri)
    {
        g_print ("%s needs a uri passed as the first argument, kill with CTRL-C.\n", argv[0]);
        return 0;
    }

    download (uri, NULL);

	g_main_loop_run (loop);
    g_main_loop_unref (loop);

    return 0;
}

