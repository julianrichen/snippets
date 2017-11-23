#define G_LOG_DOMAIN "download-async"

#include "download-async.h"

#include <glib.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

/**
 * SECTION:download-async
 * @title: Asynchronous File Download with libsoup
 * @short_description: Asynchronous file download using the libsoup package
 * @include: download-async.h
 * @see_also: #SoupSession, #SoupRequest, #GMainLoop, #GMainContext,
 *            #GCallback, #GCancellable, #GFile, #GInputStream
 *
 * Downloads files in parallel by running asynchronous file download with the
 * help of libsoup. Each download runs it's own #GMainLoop but refs the current
 * #GMainContext, if one exists using g_main_context_ref_thread_default(). If no
 * current #GMainContext exists then a new one is created. If you plan on using
 * downloads inside of an application you should run that application first
 * either with a #GMainLoop or using #GApplication so I can hook onto the
 * main context.
 **/

/**
 * download_resource_data_free:
 * @user_data: a #DownloadResourceData
 *
 * Frees a #DownloadResourceData struct.
 */
static void
download_resource_data_free (gpointer user_data)
{
    DownloadResourceData *data = user_data;

    if (data->loop)
        g_main_loop_quit (data->loop);

    if (data->uri)
        g_free (data->uri);

    if (data->path)
        g_free (data->path);

    if (data->cancellable)
    {
        g_cancellable_cancel (data->cancellable);
        g_object_unref (data->cancellable);
    }

    if (data->file)
        g_object_unref (data->file);

    if (data->output)
        g_object_unref (data->output);

    g_slice_free (DownloadResourceData, data);
}

/**
 * download_resource_from_uri_async_read_close_cb:
 * @object: a #GInputStream
 * @result: a #GAsyncResult
 * @user_data: a #DownloadResourceData
 *
 * Closes an open #GInputStream for @object.
 */
static void
download_resource_from_uri_async_read_close_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
    g_return_if_fail (G_IS_INPUT_STREAM (object));

    GInputStream *stream = G_INPUT_STREAM (object);
    DownloadResourceData *data = user_data;
    GError *error = NULL;

    if (data->p_handler)
    {
        data->p_handler (data->downloaded_bytes,
                         data->total_bytes,
                         data->p_user_data);
    }

    if (!g_input_stream_close_finish (stream, result, &error))
    {
        g_warning ("Downloader ( %s ): error closing GInputStream: %s", data->uri, error->message);
    }

    g_debug ("Downloader ( %s ): closed GInputStream, read \"%lu\" bytes", data->uri, data->downloaded_bytes);

    if (data->c_handler)
        data->c_handler (data->c_user_data);

    download_resource_data_free (data);
}

/**
 * download_resource_from_uri_async_read_cb:
 * @object: a #GInputStream
 * @result: a #GAsyncResult
 * @user_data: a #DownloadResourceData
 *
 * Reads a #GInputStream from @object and saves the bytes into @user_data->ouput
 * which is a #GOutputStream for @user_data->file. When the steam ends
 * g_input_stream_close_async() is called with a #GCallback to
 * download_resource_from_uri_async_read_close_cb().
 */
static void
download_resource_from_uri_async_read_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
    g_return_if_fail (G_IS_INPUT_STREAM (object));

    GInputStream *stream = G_INPUT_STREAM (object);
    DownloadResourceData *data = user_data;
    GError *error = NULL;
    gsize nread;

    if (data->downloaded_bytes == 0)
    {
        g_debug ("Downloader ( %s ): starting async read...", data->uri);
    }
    else
    {
        //g_debug ("Downloader ( %s ): downloaded bytes \"%lu\"", data->uri, data->downloaded_bytes);
    }

    nread = g_input_stream_read_finish (stream, result, &error);

    if (error)
    {
        g_warning ("Downloader ( %s ): stream read finished failed: %s",
                   data->uri,
                   error->message);

        g_error_free (error);
    }

    if (g_get_monotonic_time () - data->last_progress_time > 1 * G_USEC_PER_SEC)
    {
        if (data->p_handler)
        {
            data->p_handler (data->downloaded_bytes,
                             data->total_bytes,
                             data->p_user_data);
        }

        data->last_progress_time = g_get_monotonic_time ();
    }

    if (nread == -1 || nread == 0 || error)
    {
        // TODO: Add data->progress (data->downloaded_bytes)

        g_input_stream_close_async (stream,
                                    G_PRIORITY_DEFAULT,
                                    data->cancellable,
                                    download_resource_from_uri_async_read_close_cb,
                                    data);

        return;
    }

    if (data->output)
    {
        gboolean written;
        gsize n_written;

        written = g_output_stream_write_all (G_OUTPUT_STREAM (data->output),
                                             data->buffer,
                                             nread,
                                             &n_written,
                                             NULL,
                                             &error);

        if (error)
        {
            g_warning ("Downloader ( %s ): stream write failed: %s",
                       data->uri,
                       error->message);

            g_error_free (error);
        }

        data->downloaded_bytes += n_written;

        if (!written)
        {
            g_input_stream_close_async (stream,
                                        G_PRIORITY_DEFAULT,
                                        data->cancellable,
                                        download_resource_from_uri_async_read_close_cb,
                                        data);
        }
    }

    g_input_stream_read_async (stream,
                               data->buffer,
                               sizeof (data->buffer),
                               G_PRIORITY_DEFAULT,
                               data->cancellable,
                               download_resource_from_uri_async_read_cb,
                               data);
}

/**
 * download_resource_from_uri_async_cb:
 * @object: a #SoupRequest
 * @result: a #GAsyncResult
 * @user_data: a #DownloadResourceData
 *
 * After a successful #SoupRequest async callback a #GInputStream is returned.
 * If the #GInputStream is error free then a #GOutputStream is created for the
 * #GFile at user_data->file. Assuming a #GOutputStream is created without
 * errors then g_input_stream_read_async() is called with a #GCallback to
 * download_resource_from_uri_async_read_cb() which reads the #GInputStream of
 * the #SoupRequest.
 */
static void
download_resource_from_uri_async_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
    g_return_if_fail (SOUP_IS_REQUEST (object));

    SoupRequest *request = SOUP_REQUEST (object);
    GInputStream *stream = NULL;
    DownloadResourceData *data = user_data;
    GError *error = NULL;

    g_debug ("Downloader ( %s ): starting async fetch...", data->uri);

    stream = soup_request_send_finish (request, result, &error);

    data->total_bytes = soup_request_get_content_length (request);

    if (error)
    {
        g_warning ("Downloader ( %s ): failed to start resource: %s",
                   data->uri,
                   error->message);

        g_error_free (error);
        download_resource_data_free (data);

        return;
    }

    data->output = G_OUTPUT_STREAM (g_file_replace (data->file,
                                                    NULL,
                                                    FALSE,
                                                    G_FILE_CREATE_NONE,
                                                    data->cancellable,
                                                    &error));

    if (error)
    {
        g_warning ("Downloader ( %s ): failed to create output stream and file \"%s\": %s",
                   data->uri,
                   data->path,
                   error->message);

        g_error_free (error);
        download_resource_data_free (data);

        return;
    }

    g_input_stream_read_async (stream,
                               data->buffer,
                               sizeof (data->buffer),
                               G_PRIORITY_DEFAULT,
                               data->cancellable,
                               download_resource_from_uri_async_read_cb,
                               data);
}

/**
 * download_resource_from_uri_async_full:
 * @uri: a uri to a resource to download
 * @path: a path to save the @uri resource to
 * @overwrite: %TRUE to overwrite @path
 * @cancellable: a #GCancellable, used to stop the asynchronous resource fetch
 *               and resource save
 * @p_handler: the #GCallback for the progress callback.
 * @p_user_data: data to pass to @p_handler callback
 * @c_handler: the #GCallback for the finial callback
 * @c_user_data: data to pass to @c_handler callback
 *
 * Requests a resource from @uri saving it into @path, overriding the contents
 * of @path if @overwrite is set to %TRUE. If @path is %NULL then the @uri will
 * be downloaded into %G_USER_DIRECTORY_DOWNLOAD (a.k.a ~/Downloads). If path
 * ends in a slash (/) then the path is treated as a directory. In both cases
 * the file will be named the basename of @uri. Not supplying @path is
 * discouraged.
 *
 * In addition a #GFile is created for @path and set into a new
 * #DownloadResourceData struct as (struct name)->file. Checks are also made if
 * the @path file exists and if it should overwrite based on @overwrite.
 *
 * When initial data is set soup_request_send_async() is called with a
 * #GCallback to download_resource_from_uri_async_cb() which starts the
 * #SoupRequest.
 *
 * Note: download_resource_from_uri_async_full() runs it's own #GMainLoop
 * but refs the current #GMainContext, if on exists using
 * g_main_context_ref_thread_default(). If no current #GMainContext exists then
 * a new one is created. If you plan on using downloads inside of an
 * application you should run that application first either with a #GMainLoop
 * or using #GApplication so I can hook onto the main context.
 */
void
download_resource_from_uri_async_full (gchar                        *uri,
                                       gchar                        *path,
                                       gboolean                      overwrite,
                                       GCancellable                 *cancellable,
                                       DownloadResourceDataProgress  p_handler,
                                       gpointer                      p_user_data,
                                       DownloadResourceDataCallback  c_handler,
                                       gpointer                      c_user_data)
{
    g_return_if_fail (uri && g_strv_length (&uri) > 0);
    g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

    GMainLoop *loop = NULL;
    GMainContext *context = NULL;
    DownloadResourceData *data;
    SoupSession *session;
    SoupRequest *request;
    gchar *internal_uri = NULL;
    gchar *internal_path = NULL;
    GError *error = NULL;
    GFile *file;

    context = g_main_context_ref_thread_default ();
    loop = g_main_loop_new (context, TRUE);

    internal_uri = g_strdup (uri);
    internal_path = g_strdup (path);

    g_debug ("Downloader ( %s ): starting...", internal_uri);

    if (path == NULL)
    {
        internal_path =
            g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD),
                              g_path_get_basename (internal_uri),
                              NULL);
    }

    if (g_str_has_suffix (internal_path, "/"))
    {
        if (!g_file_test (internal_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
        {
            g_mkdir_with_parents (internal_path, 0700);
        }

        internal_path =
            g_build_filename (internal_path,
                              g_path_get_basename (internal_uri),
                              NULL);
    }

    g_debug ("Downloader ( %s ): saving to \"%s\"", internal_uri, internal_path);

    file = g_file_new_for_path (internal_path);

    if (!overwrite && g_file_query_exists (file, cancellable))
    {
        g_debug ("Downloader ( %s ): overwite = FALSE and file exists ( %s ), download cancelled", internal_uri, internal_path);
        return;
    }

    session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT, "download_async",
                                             SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
                                             SOUP_SESSION_USE_THREAD_CONTEXT, TRUE,
                                             SOUP_SESSION_TIMEOUT, 60,
                                             SOUP_SESSION_IDLE_TIMEOUT, 60,
                                             NULL);

    request = soup_session_request (session, internal_uri, &error);

    if (error)
    {
        g_warning ("Downloader ( %s ): failed to prase uri", internal_uri);
        g_cancellable_cancel (cancellable);
        g_object_unref (cancellable);
        g_error_free (error);
        return;
    }

    g_debug ("Downloader ( %s ): soup session and request started", internal_uri);

    data = g_slice_new0 (DownloadResourceData);
    data->loop = loop;

    data->uri = internal_uri;
    data->path = internal_path;
    data->overwrite = overwrite;

    data->cancellable = cancellable;
    data->p_handler = p_handler;
    data->p_user_data = p_user_data;
    data->c_handler = c_handler;
    data->c_user_data = c_user_data;

    data->file = file;
    data->total_bytes = 0;
    data->downloaded_bytes = 0;
    data->last_progress_time = g_get_monotonic_time ();

    soup_request_send_async (SOUP_REQUEST(request),
                             cancellable,
                             download_resource_from_uri_async_cb,
                             data);

    g_main_loop_run (loop);
    g_main_loop_unref (loop);
    g_main_context_unref (context);
}

/**
 * download_resource_from_uri_async_with_callback:
 * @uri: a uri to a resource to download
 * @path: a path to save the @uri resource to
 * @overwrite: %TRUE to overwrite @path
 * @cancellable: a #GCancellable, used to stop the asynchronous resource fetch
 *               and resource save
 * @p_handler: the #GCallback for the progress callback.
 * @c_user_data: data to pass to @p_handler callback
 *
 * Requests a resource from @uri saving it into @path, overriding the contents
 * of @path if @overwrite is set to %TRUE. Because download_async() is
 * asynchronous @uri & @path will be freed when the request is finished. Make a
 * copy of @uri and @path when calling, for example with g_strdup().
 *
 * Use download_resource_from_uri_async_full() if you want to call a function
 * when the asynchronous download is finished, like emitting a signal with
 * g_signal_emit().
 *
 * See download_resource_from_uri_async_full() for more about technical
 * description of the download process.
 */
void
download_resource_from_uri_async_with_callback (gchar                        *uri,
                                                gchar                        *path,
                                                gboolean                      overwrite,
                                                GCancellable                 *cancellable,
                                                DownloadResourceDataCallback  c_handler,
                                                gpointer                      c_user_data)
{
    download_resource_from_uri_async_full (uri,
                                           path,
                                           overwrite,
                                           cancellable,
                                           NULL,
                                           NULL,
                                           c_handler,
                                           c_user_data);
}

/**
 * download_resource_from_uri_async:
 * @uri: a uri to a resource to download, do not free
 * @path: a path to save the @uri resource to, do not free
 * @overwrite: %TRUE to overwrite @path
 * @cancellable: a #GCancellable, used to stop the asynchronous resource fetch
 *               and resource save
 *
 * Requests a resource from @uri saving it into @path, overriding the contents
 * of @path if @overwrite is set to %TRUE. Because download_async() is
 * asynchronous @uri & @path will be freed when the request is finished. Make a
 * copy of @uri and @path when calling, for example with g_strdup().
 *
 * Use download_resource_from_uri_async_full() if you want to call a function
 * when the asynchronous download is finished, like emitting a signal with
 * g_signal_emit().
 *
 * See download_resource_from_uri_async_full() for more about technical
 * description of the download process.
 */
void
download_resource_from_uri_async (gchar        *uri,
                                  gchar        *path,
                                  gboolean      overwrite,
                                  GCancellable *cancellable)
{
    download_resource_from_uri_async_full (uri,
                                           path,
                                           overwrite,
                                           cancellable,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
}
