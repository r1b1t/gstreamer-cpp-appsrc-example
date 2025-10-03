#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <cstring>

static guint64 frame_count = 0;

// need-data → pipeline yeni veri istiyor
static void need_data_callback(GstAppSrc *appsrc, guint length, gpointer /*user_data*/)
{
    std::cout << "[AppSrc] need-data: " << length << " byte istendi (ipucu, -1 olabilir). "
              << "Burst push yapıyorum..." << std::endl;

    const int width = 320;
    const int height = 240;
    const int channels = 3; // RGB
    const gsize buffer_size = width * height * channels;

    // Kuyruğu doldurup enough-data'yı tetiklemek için bir anda çok buffer gönderelim
    for (int i = 0; i < 100; ++i)
    {
        GstBuffer *buffer = gst_buffer_new_allocate(nullptr, buffer_size, nullptr);
        if (!buffer)
            break;

        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_WRITE))
        {
            // Basit gri ton animasyonu
            std::memset(map.data, static_cast<int>(frame_count % 255), buffer_size);
            gst_buffer_unmap(buffer, &map);
        }

        // 25 fps → her kare 40 ms (TIME formatında ns cinsinden)
        GST_BUFFER_PTS(buffer) = gst_util_uint64_scale(frame_count, GST_SECOND, 25);
        GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, 25);

        ++frame_count;

        GstFlowReturn ret = gst_app_src_push_buffer(appsrc, buffer);
        if (ret != GST_FLOW_OK)
        {
            std::cerr << "push_buffer failed (flow=" << ret << "), döngüden çıkıyorum.\n";
            break;
        }
    }
}

// enough-data → kuyruk doldu, üretimi durdur
static void enough_data_callback(GstAppSrc * /*appsrc*/, gpointer /*user_data*/)
{
    std::cout << "[AppSrc] enough-data: Kuyruk doldu, şimdilik push etmeyi bırak.\n";
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    // Basit görüntüleme hattı
    GstElement *pipeline = gst_parse_launch(
        "appsrc name=mysrc ! videoconvert ! autovideosink", nullptr);

    // appsrc'yi al
    GstElement *src_elem = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
    GstAppSrc *appsrc = GST_APP_SRC(src_elem);

    // Caps: RGB, 320x240 @ 25fps
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "RGB",
                                        "width", G_TYPE_INT, 320,
                                        "height", G_TYPE_INT, 240,
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        nullptr);
    gst_app_src_set_caps(appsrc, caps);
    gst_caps_unref(caps);

    // AppSrc özellikleri
    g_object_set(appsrc,
                 // Zamanı TIME formatında çalıştır → PTS/DURATION ile uyumlu
                 "format", GST_FORMAT_TIME,
                 // Kuyruk dolunca push'un bloklamasını istemiyorsan FALSE, bloklasın dersen TRUE
                 "block", FALSE,
                 // Kuyruk sınırlarını küçük tutarak enough-data'yı kolay tetikle
                 "max-buffers", 2,               // en fazla 2 buffer
                 "max-bytes", 2 * 320 * 240 * 3, // ~2 frame kadar
                 nullptr);

    // Sinyalleri bağla
    g_signal_connect(appsrc, "need-data", G_CALLBACK(need_data_callback), nullptr);
    g_signal_connect(appsrc, "enough-data", G_CALLBACK(enough_data_callback), nullptr);

    // Oynat
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Basit main loop
    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    // Temizlik
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}
