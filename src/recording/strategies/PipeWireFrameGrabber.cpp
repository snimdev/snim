#include "screen/PipeWireFrames.h"

#include "media/gst/GstSupport.h"

#include <QElapsedTimer>
#include <QImage>
#include <QString>

namespace {

using Media::Gst::GstPtr;

// The bus error, if the pipeline has posted one; empty otherwise.
QString pendingError(GstElement *pipeline)
{
    GstPtr<GstBus> bus(gst_element_get_bus(pipeline));
    GstMessage *message = gst_bus_pop_filtered(bus.get(), GST_MESSAGE_ERROR);
    if (!message)
        return {};
    GError *error = nullptr;
    gst_message_parse_error(message, &error, nullptr);
    const QString text = Media::Gst::errorText(error);
    g_clear_error(&error);
    gst_message_unref(message);
    return text.isEmpty() ? QStringLiteral("PipeWire stream error") : text;
}

QImage imageFromSample(GstSample *sample)
{
    const GstStructure *structure = gst_caps_get_structure(gst_sample_get_caps(sample), 0);
    int width = 0;
    int height = 0;
    if (!gst_structure_get_int(structure, "width", &width)
        || !gst_structure_get_int(structure, "height", &height) || width <= 0 || height <= 0)
        return {};

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (!buffer || !gst_buffer_map(buffer, &map, GST_MAP_READ))
        return {};

    // Packed BGRx without a video meta, so the stride is whatever each row occupies.
    QImage image;
    const gsize stride = map.size / gsize(height);
    if (stride >= gsize(width) * 4) {
        image = QImage(map.data, width, height, qsizetype(stride), QImage::Format_RGB32).copy();
    }
    gst_buffer_unmap(buffer, &map);
    return image;
}

} // namespace

extern "C" bool snimGrabPipeWireFrames(int pipewireFd, const quint32 *nodeIds, int count,
                                       int timeoutMs, QImage *frames, QString *error)
{
    const auto failWith = [error](const QString &text) {
        if (error)
            *error = text;
        return false;
    };

    if (count <= 0)
        return failWith(QStringLiteral("No PipeWire stream to grab"));
    if (!Media::Gst::ensureInitialized() || !Media::Gst::hasFactory("pipewiresrc")
        || !Media::Gst::hasFactory("appsink") || !Media::Gst::hasFactory("videoconvert"))
        return failWith(QStringLiteral("GStreamer pipewiresrc, videoconvert or appsink is missing"));

    // One branch per stream in a single pipeline, so the monitors start in parallel.
    QString description;
    for (int i = 0; i < count; ++i) {
        description += QStringLiteral(
            "pipewiresrc name=src%1 ! videoconvert ! video/x-raw,format=BGRx "
            "! appsink name=sink%1 sync=false max-buffers=1 drop=true ").arg(i);
    }

    GError *parseError = nullptr;
    GstPtr<GstElement> pipeline(gst_parse_launch(description.toUtf8().constData(), &parseError));
    const QString parseText = Media::Gst::errorText(parseError);
    g_clear_error(&parseError);
    if (!pipeline)
        return failWith(QStringLiteral("The frame pipeline could not be built: %1").arg(parseText));

    for (int i = 0; i < count; ++i) {
        GstPtr<GstElement> src(gst_bin_get_by_name(
            GST_BIN(pipeline.get()), QStringLiteral("src%1").arg(i).toUtf8().constData()));
        if (!src)
            return failWith(QStringLiteral("The frame pipeline could not be built"));
        // pipewiresrc dups the fd, so the caller still owns and closes its own.
        g_object_set(src.get(), "fd", pipewireFd,
                     "path", QByteArray::number(nodeIds[i]).constData(), nullptr);
    }

    if (gst_element_set_state(pipeline.get(), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        const QString text = pendingError(pipeline.get());
        gst_element_set_state(pipeline.get(), GST_STATE_NULL);
        return failWith(text.isEmpty() ? QStringLiteral("The frame pipeline could not start") : text);
    }

    QElapsedTimer clock;
    clock.start();
    QString failure;
    for (int i = 0; i < count && failure.isEmpty(); ++i) {
        GstPtr<GstElement> sink(gst_bin_get_by_name(
            GST_BIN(pipeline.get()), QStringLiteral("sink%1").arg(i).toUtf8().constData()));
        if (!sink) {
            failure = QStringLiteral("The frame pipeline could not be built");
            break;
        }
        GstSample *sample = nullptr;
        // Short slices so a bus error fails fast instead of waiting out the deadline.
        while (!sample) {
            const qint64 left = timeoutMs - clock.elapsed();
            if (left <= 0) {
                failure = QStringLiteral("No frame arrived from the PipeWire stream in time");
                break;
            }
            g_signal_emit_by_name(sink.get(), "try-pull-sample",
                                  GstClockTime(qMin<qint64>(left, 50)) * GST_MSECOND, &sample);
            if (!sample) {
                failure = pendingError(pipeline.get());
                if (!failure.isEmpty())
                    break;
            }
        }
        if (sample) {
            frames[i] = imageFromSample(sample);
            gst_sample_unref(sample);
            if (frames[i].isNull())
                failure = QStringLiteral("The PipeWire frame could not be read");
        }
    }

    // Blocks until the streaming threads are joined, so the streams are gone on return.
    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
    if (!failure.isEmpty())
        return failWith(failure);
    return true;
}
