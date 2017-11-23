#include <glib.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

typedef void (* DownloadResourceDataProgress) (guint64   downloaded_bytes,
                                               guint64   total_bytes,
                                               gpointer  user_data);

typedef void (* DownloadResourceDataCallback) (gpointer user_data);


typedef struct _DownloadResourceData {
    GMainLoop *loop;

    gchar *uri;
    gchar *path;
    gboolean overwrite;

    GCancellable *cancellable;
    DownloadResourceDataProgress p_handler;
    gpointer p_user_data;
    DownloadResourceDataCallback c_handler;
    gpointer c_user_data;

    GFile *file;
    gchar buffer[16384]; // 16 * 1024
    GOutputStream *output;

    guint64 total_bytes;
    guint64 downloaded_bytes;
    guint64 last_progress_time;
} DownloadResourceData;

void
download_resource_from_uri_async_full (gchar                        *uri,
                                       gchar                        *path,
                                       gboolean                      overwrite,
                                       GCancellable                 *cancellable,
                                       DownloadResourceDataProgress  p_handler,
                                       gpointer                      p_user_data,
                                       DownloadResourceDataCallback  c_handler,
                                       gpointer                      c_user_data);

void
download_resource_from_uri_async_with_callback (gchar                        *uri,
                                                gchar                        *path,
                                                gboolean                      overwrite,
                                                GCancellable                 *cancellable,
                                                DownloadResourceDataCallback  c_handler,
                                                gpointer                      c_user_data);

void
download_resource_from_uri_async (gchar        *uri,
                                  gchar        *path,
                                  gboolean      overwrite,
                                  GCancellable *cancellable);

G_END_DECLS

