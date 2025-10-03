#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <cstring>

static guint64 frame_count = 0;

// need-data → pipeline veri istediğinde çağrılır
static void need_data_callback(GstAppSrc *appsrc, guint length, gpointer user_data)
{
    std::cout << "[AppSrc] Need-data tetiklendi (length=" << length << ")" << std::endl;

    const int width = 320;
    const int height = 240;
    const int channels = 3;
    const int buffer_size = width * height * channels;

    // 10 frame üretelim
    for (int i = 0; i < 10; i++)
    {
        GstBuffer *buffer = gst_buffer_new_allocate(NULL, buffer_size, NULL);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_WRITE);

        memset(map.data, (frame_count % 255), buffer_size);

        gst_buffer_unmap(buffer, &map);

        GST_BUFFER_PTS(buffer) = gst_util_uint64_scale(frame_count, GST_SECOND, 25);
        GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, 25);

        frame_count++;

        GstFlowReturn ret = gst_app_src_push_buffer(appsrc, buffer);
        if (ret != GST_FLOW_OK)
        {
            std::cout << "[AppSrc] push_buffer FAILED" << std::endl;
            break;
        }
    }
}

// seek-data → uygulama yeni offset’ten veri göndermeye başlamalı
static gboolean seek_data_callback(GstAppSrc *appsrc, guint64 offset, gpointer user_data)
{
    std::cout << "[AppSrc] Seek-data tetiklendi! Yeni offset (bytes): " << offset << std::endl;

    const int frame_size = 320 * 240 * 3; // 1 frame’in byte boyutu
    frame_count = offset / frame_size;

    std::cout << "[AppSrc] Frame counter reset -> " << frame_count << std::endl;
    return TRUE; // başarılı
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    // appsrc → videoconvert → autovideosink
    GstElement *pipeline = gst_parse_launch(
        "appsrc name=mysrc ! videoconvert ! autovideosink", NULL);

    // appsrc elementini al
    GstElement *src = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
    GstAppSrc *appsrc = GST_APP_SRC(src);

    // caps ayarla (RGB video)
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "RGB",
                                        "width", G_TYPE_INT, 320,
                                        "height", G_TYPE_INT, 240,
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        NULL);
    gst_app_src_set_caps(appsrc, caps);
    gst_caps_unref(caps);

    // stream-type ve format ayarla
    gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_SEEKABLE);
    g_object_set(appsrc, "format", GST_FORMAT_BYTES, NULL);

    // sinyalleri bağla
    g_signal_connect(appsrc, "need-data", G_CALLBACK(need_data_callback), NULL);
    g_signal_connect(appsrc, "seek-data", G_CALLBACK(seek_data_callback), NULL);

    // pipeline çalıştır
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // main loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // seek-data sinyalini manuel tetikle
    gboolean ret;
    const int frame_size = 320 * 240 * 3;
    guint64 newpos = frame_size * 50; // ~50. frame
    g_signal_emit_by_name(appsrc, "seek-data", (guint64)newpos, &ret);
    std::cout << "[Demo] seek-data result: " << ret << std::endl;

    g_main_loop_run(loop);

    // temizlik
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}
