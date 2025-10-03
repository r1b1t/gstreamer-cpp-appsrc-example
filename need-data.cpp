#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <string.h>

static void need_data_callback(GstAppSrc *appsrc, guint length, gpointer user_data)
{
    static guint64 frame_count = 0;

    // Basit bir video frame (ör: siyah ekran)
    const int width = 320;
    const int height = 240;
    const int channels = 3; // RGB
    const int buffer_size = width * height * channels;

    // (Belleği kimin yöneteceğini belirtir, size, ekstra parametreler)
    // GstBuffer* gst_buffer_new_allocate (GstAllocator *allocator, gsize size, GstAllocationParams *params);
    // bize bir buffer oluşturuyoruz
    GstBuffer *buffer = gst_buffer_new_allocate(NULL, buffer_size, NULL);

    // Buffer’ı bellekte erişilebilir hale getiriyoruz.
    GstMapInfo map;

    gst_buffer_map(buffer, &map, GST_MAP_WRITE);

    // Tamamen siyah doldur (örnek amaçlı)
    memset(map.data, (frame_count % 255), buffer_size);

    gst_buffer_unmap(buffer, &map);

    // Timestamp ata (görüntünün sürekliliği için önemli)
    GST_BUFFER_PTS(buffer) = frame_count * GST_MSECOND * 40; // ~25 fps
    GST_BUFFER_DURATION(buffer) = 40 * GST_MSECOND;

    // 🔹 Frame sayacını terminale yazdır
    std::cout << "Frame count: " << frame_count
              << "  (renk değeri = " << (frame_count % 255) << ")"
              << std::endl;

    frame_count++;

    // Buffer’ı push et
    GstFlowReturn ret = gst_app_src_push_buffer(appsrc, buffer);
    if (ret != GST_FLOW_OK)
    {
        std::cout << "Push buffer failed!" << std::endl;
    }
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    // Pipeline oluştur: appsrc ! videoconvert ! autovideosink
    GstElement *pipeline = gst_parse_launch(
        "appsrc name=mysource is-live=true block=true format=time ! videoconvert ! autovideosink",
        nullptr);

    // appsrc elementini al
    GstElement *appsrc_elem = gst_bin_get_by_name(GST_BIN(pipeline), "mysource");
    GstAppSrc *appsrc = GST_APP_SRC(appsrc_elem);

    // Caps ayarla (RGB video 320x240, 25 fps)
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "RGB",
                                        "width", G_TYPE_INT, 320,
                                        "height", G_TYPE_INT, 240,
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        NULL);
    gst_app_src_set_caps(appsrc, caps);
    gst_caps_unref(caps);

    // need-data sinyalini bağla
    g_signal_connect(appsrc, "need-data", G_CALLBACK(need_data_callback), NULL);

    // Pipeline başlat
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Main loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    // Temizlik
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}
