#include "ScreencopyClient.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

#include <cerrno>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

namespace Screen::Screencopy {

static_assert(ShmFormat::ARGB8888 == WL_SHM_FORMAT_ARGB8888);
static_assert(ShmFormat::XRGB8888 == WL_SHM_FORMAT_XRGB8888);
static_assert(ShmFormat::XBGR8888 == WL_SHM_FORMAT_XBGR8888);
static_assert(ShmFormat::XRGB2101010 == WL_SHM_FORMAT_XRGB2101010);

namespace {

struct Connection;

struct Output {
    Connection *connection = nullptr;
    wl_output *output = nullptr;
    uint32_t version = 0;
    QString name;
    QPoint position;
    uint32_t transform = WL_OUTPUT_TRANSFORM_NORMAL;

    zwlr_screencopy_frame_v1 *wlrFrame = nullptr;
    ext_image_capture_source_v1 *source = nullptr;
    ext_image_copy_capture_session_v1 *session = nullptr;
    ext_image_copy_capture_frame_v1 *extFrame = nullptr;

    QList<quint32> shmFormats;
    std::optional<quint32> format;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t frameTransform = WL_OUTPUT_TRANSFORM_NORMAL;
    bool yInvert = false;

    wl_buffer *buffer = nullptr;
    void *data = MAP_FAILED;
    size_t size = 0;

    enum class State { Idle, Capturing, Ready, Failed } state = State::Idle;
    QString error;
    QImage image;
};

struct Connection {
    wl_display *display = nullptr;
    wl_event_queue *queue = nullptr;
    wl_display *wrapper = nullptr;
    wl_registry *registry = nullptr;
    wl_callback *sync = nullptr;   // the roundtrip in flight
    bool synced = false;
    bool bind = false;

    wl_shm *shm = nullptr;
    zwlr_screencopy_manager_v1 *wlr = nullptr;
    uint32_t wlrVersion = 0;
    ext_image_copy_capture_manager_v1 *ext = nullptr;
    ext_output_image_capture_source_manager_v1 *extSources = nullptr;
    Globals seen;
    bool seenExtCopy = false;
    bool seenExtSources = false;
    std::vector<std::unique_ptr<Output>> outputs;

    ~Connection();
};

wl_display *qtDisplay()
{
#if QT_CONFIG(wayland)
    if (auto *app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr)
        return app->display();
#endif
    return nullptr;
}

// Dispatches only our queue; Qt's event thread may read the same fd, which libwayland arbitrates.
bool dispatchUntil(Connection &c, const std::function<bool()> &done, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        if (wl_display_prepare_read_queue(c.display, c.queue) != 0) {
            if (wl_display_dispatch_queue_pending(c.display, c.queue) < 0)
                return false;
            continue;
        }
        wl_display_flush(c.display);

        const qint64 remaining = timeoutMs - timer.elapsed();
        if (remaining <= 0) {
            wl_display_cancel_read(c.display);
            return false;
        }
        pollfd pfd{wl_display_get_fd(c.display), POLLIN, 0};
        const int ready = poll(&pfd, 1, int(remaining));
        if (ready <= 0) {
            wl_display_cancel_read(c.display);
            if (ready < 0 && errno == EINTR)
                continue;
            return false;
        }
        if (wl_display_read_events(c.display) < 0)
            return false;
        if (wl_display_dispatch_queue_pending(c.display, c.queue) < 0)
            return false;
    }
    return true;
}

void dropSync(Connection &c)
{
    if (c.sync)
        wl_callback_destroy(c.sync);
    c.sync = nullptr;
}

void syncDone(void *data, wl_callback *, uint32_t)
{
    auto *c = static_cast<Connection *>(data);
    dropSync(*c);
    c->synced = true;
}

const wl_callback_listener syncListener{.done = syncDone};

bool roundtrip(Connection &c, int timeoutMs)
{
    c.synced = false;
    c.sync = wl_display_sync(c.wrapper);
    wl_callback_add_listener(c.sync, &syncListener, &c);
    if (dispatchUntil(c, [&c] { return c.synced; }, timeoutMs))
        return true;
    // A done arriving after the queue is gone would abort the app inside libwayland.
    dropSync(c);
    return false;
}

// --- wl_output ---

void outputGeometry(void *data, wl_output *, int32_t x, int32_t y, int32_t, int32_t, int32_t,
                    const char *, const char *, int32_t transform)
{
    auto *o = static_cast<Output *>(data);
    o->position = QPoint(x, y);
    o->transform = uint32_t(transform);
}

void outputMode(void *, wl_output *, uint32_t, int32_t, int32_t, int32_t) {}
void outputDone(void *, wl_output *) {}
void outputScale(void *, wl_output *, int32_t) {}

void outputName(void *data, wl_output *, const char *name)
{
    static_cast<Output *>(data)->name = QString::fromUtf8(name);
}

void outputDescription(void *, wl_output *, const char *) {}

const wl_output_listener outputListener{
    .geometry = outputGeometry,
    .mode = outputMode,
    .done = outputDone,
    .scale = outputScale,
    .name = outputName,
    .description = outputDescription,
};

// --- registry ---

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                    uint32_t version)
{
    auto *c = static_cast<Connection *>(data);
    if (std::strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        c->seen.wlr = true;
        if (c->bind) {
            c->wlrVersion = qMin<uint32_t>(version, 3);
            c->wlr = static_cast<zwlr_screencopy_manager_v1 *>(
                wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, c->wlrVersion));
        }
    } else if (std::strcmp(interface, ext_image_copy_capture_manager_v1_interface.name) == 0) {
        c->seenExtCopy = true;
        if (c->bind)
            c->ext = static_cast<ext_image_copy_capture_manager_v1 *>(
                wl_registry_bind(registry, name, &ext_image_copy_capture_manager_v1_interface, 1));
    } else if (std::strcmp(interface, ext_output_image_capture_source_manager_v1_interface.name) == 0) {
        c->seenExtSources = true;
        if (c->bind)
            c->extSources = static_cast<ext_output_image_capture_source_manager_v1 *>(
                wl_registry_bind(registry, name, &ext_output_image_capture_source_manager_v1_interface, 1));
    } else if (!c->bind) {
        return;
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        c->shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
        auto output = std::make_unique<Output>();
        output->connection = c;
        output->version = qMin<uint32_t>(version, 4);
        output->output = static_cast<wl_output *>(
            wl_registry_bind(registry, name, &wl_output_interface, output->version));
        wl_output_add_listener(output->output, &outputListener, output.get());
        c->outputs.push_back(std::move(output));
    }
    c->seen.ext = c->seenExtCopy && c->seenExtSources;
}

void registryGlobalRemove(void *, wl_registry *, uint32_t) {}

const wl_registry_listener registryListener{
    .global = registryGlobal,
    .global_remove = registryGlobalRemove,
};

bool openConnection(Connection &c, bool bind, int timeoutMs)
{
    c.display = qtDisplay();
    if (!c.display)
        return false;
    c.bind = bind;
    c.queue = wl_display_create_queue(c.display);
    c.wrapper = static_cast<wl_display *>(wl_proxy_create_wrapper(c.display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(c.wrapper), c.queue);
    c.registry = wl_display_get_registry(c.wrapper);
    wl_registry_add_listener(c.registry, &registryListener, &c);
    // The second roundtrip collects the wl_output events of the outputs just bound.
    return roundtrip(c, timeoutMs) && (!bind || roundtrip(c, timeoutMs));
}

Connection::~Connection()
{
    dropSync(*this);
    for (auto &o : outputs) {
        if (o->wlrFrame)
            zwlr_screencopy_frame_v1_destroy(o->wlrFrame);
        if (o->extFrame)
            ext_image_copy_capture_frame_v1_destroy(o->extFrame);
        if (o->session)
            ext_image_copy_capture_session_v1_destroy(o->session);
        if (o->source)
            ext_image_capture_source_v1_destroy(o->source);
        if (o->buffer)
            wl_buffer_destroy(o->buffer);
        if (o->data != MAP_FAILED)
            munmap(o->data, o->size);
        if (o->output) {
            if (o->version >= WL_OUTPUT_RELEASE_SINCE_VERSION)
                wl_output_release(o->output);
            else
                wl_output_destroy(o->output);
        }
    }
    if (wlr)
        zwlr_screencopy_manager_v1_destroy(wlr);
    if (ext)
        ext_image_copy_capture_manager_v1_destroy(ext);
    if (extSources)
        ext_output_image_capture_source_manager_v1_destroy(extSources);
    if (shm)
        wl_shm_destroy(shm);
    if (registry)
        wl_registry_destroy(registry);
    if (wrapper)
        wl_proxy_wrapper_destroy(wrapper);
    if (display)
        wl_display_flush(display);
    if (queue)
        wl_event_queue_destroy(queue);
}

// --- frames ---

void fail(Output *o, const QString &error)
{
    if (o->state == Output::State::Ready || o->state == Output::State::Failed)
        return;
    o->state = Output::State::Failed;
    o->error = error;
}

bool createBuffer(Output *o)
{
    if (!o->format) {
        fail(o, QStringLiteral("no supported shm format"));
        return false;
    }
    if (o->stride == 0)
        o->stride = uint32_t(strideFor(*o->format, int(o->width)));
    o->size = size_t(o->stride) * o->height;
    if (o->size == 0) {
        fail(o, QStringLiteral("empty buffer"));
        return false;
    }

    const int fd = memfd_create("snim-screencopy", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, off_t(o->size)) < 0) {
        if (fd >= 0)
            close(fd);
        fail(o, QStringLiteral("shm allocation failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }
    o->data = mmap(nullptr, o->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (o->data == MAP_FAILED) {
        close(fd);
        fail(o, QStringLiteral("mmap failed"));
        return false;
    }
    wl_shm_pool *pool = wl_shm_create_pool(o->connection->shm, fd, int32_t(o->size));
    o->buffer = wl_shm_pool_create_buffer(pool, 0, int32_t(o->width), int32_t(o->height),
                                          int32_t(o->stride), *o->format);
    wl_shm_pool_destroy(pool);
    close(fd);
    return true;
}

// Copies the pixels out before teardown unmaps them, upright per the transform in force.
void finish(Output *o, uint32_t transform)
{
    const QImage raw(static_cast<const uchar *>(o->data), int(o->width), int(o->height),
                     qsizetype(o->stride), imageFormatFor(*o->format));
    o->image = uprightFrame(raw.copy(), transform, o->yInvert);
    o->state = Output::State::Ready;
}

// wlr-screencopy

void wlrCopy(Output *o)
{
    if (o->buffer || o->state != Output::State::Capturing)
        return;
    if (createBuffer(o))
        zwlr_screencopy_frame_v1_copy(o->wlrFrame, o->buffer);
}

void wlrBuffer(void *data, zwlr_screencopy_frame_v1 *, uint32_t format, uint32_t width,
               uint32_t height, uint32_t stride)
{
    auto *o = static_cast<Output *>(data);
    if (!o->format && bytesPerPixel(format) > 0) {
        o->format = format;
        o->width = width;
        o->height = height;
        o->stride = stride;
    }
    // Before version 3 there is no buffer_done: the one buffer event is all there is.
    if (o->connection->wlrVersion < 3)
        wlrCopy(o);
}

void wlrFlags(void *data, zwlr_screencopy_frame_v1 *, uint32_t flags)
{
    static_cast<Output *>(data)->yInvert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
}

void wlrReady(void *data, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t)
{
    auto *o = static_cast<Output *>(data);
    finish(o, o->transform);
}

void wlrFailed(void *data, zwlr_screencopy_frame_v1 *)
{
    fail(static_cast<Output *>(data), QStringLiteral("the compositor refused the copy"));
}

void wlrDamage(void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t, uint32_t) {}
void wlrDmabuf(void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t) {}

void wlrBufferDone(void *data, zwlr_screencopy_frame_v1 *)
{
    wlrCopy(static_cast<Output *>(data));
}

const zwlr_screencopy_frame_v1_listener wlrFrameListener{
    .buffer = wlrBuffer,
    .flags = wlrFlags,
    .ready = wlrReady,
    .failed = wlrFailed,
    .damage = wlrDamage,
    .linux_dmabuf = wlrDmabuf,
    .buffer_done = wlrBufferDone,
};

// ext-image-copy-capture

void extFrameTransform(void *data, ext_image_copy_capture_frame_v1 *, uint32_t transform)
{
    static_cast<Output *>(data)->frameTransform = transform;
}

void extFrameDamage(void *, ext_image_copy_capture_frame_v1 *, int32_t, int32_t, int32_t, int32_t) {}
void extFramePresentation(void *, ext_image_copy_capture_frame_v1 *, uint32_t, uint32_t, uint32_t) {}

void extFrameReady(void *data, ext_image_copy_capture_frame_v1 *)
{
    auto *o = static_cast<Output *>(data);
    finish(o, o->frameTransform);
}

void extFrameFailed(void *data, ext_image_copy_capture_frame_v1 *, uint32_t reason)
{
    fail(static_cast<Output *>(data), QStringLiteral("the compositor refused the copy (reason %1)").arg(reason));
}

const ext_image_copy_capture_frame_v1_listener extFrameListener{
    .transform = extFrameTransform,
    .damage = extFrameDamage,
    .presentation_time = extFramePresentation,
    .ready = extFrameReady,
    .failed = extFrameFailed,
};

void extBufferSize(void *data, ext_image_copy_capture_session_v1 *, uint32_t width, uint32_t height)
{
    auto *o = static_cast<Output *>(data);
    o->width = width;
    o->height = height;
}

void extShmFormat(void *data, ext_image_copy_capture_session_v1 *, uint32_t format)
{
    static_cast<Output *>(data)->shmFormats.append(format);
}

void extDmabufDevice(void *, ext_image_copy_capture_session_v1 *, wl_array *) {}
void extDmabufFormat(void *, ext_image_copy_capture_session_v1 *, uint32_t, wl_array *) {}

// The constraints are complete: allocate and capture once, later updates are ignored.
void extSessionDone(void *data, ext_image_copy_capture_session_v1 *)
{
    auto *o = static_cast<Output *>(data);
    if (o->extFrame || o->state != Output::State::Capturing)
        return;
    o->format = pickShmFormat(o->shmFormats);
    if (!createBuffer(o))
        return;
    o->extFrame = ext_image_copy_capture_session_v1_create_frame(o->session);
    ext_image_copy_capture_frame_v1_add_listener(o->extFrame, &extFrameListener, o);
    ext_image_copy_capture_frame_v1_attach_buffer(o->extFrame, o->buffer);
    ext_image_copy_capture_frame_v1_damage_buffer(o->extFrame, 0, 0, int32_t(o->width), int32_t(o->height));
    ext_image_copy_capture_frame_v1_capture(o->extFrame);
}

void extSessionStopped(void *data, ext_image_copy_capture_session_v1 *)
{
    fail(static_cast<Output *>(data), QStringLiteral("the capture session stopped"));
}

const ext_image_copy_capture_session_v1_listener extSessionListener{
    .buffer_size = extBufferSize,
    .shm_format = extShmFormat,
    .dmabuf_device = extDmabufDevice,
    .dmabuf_format = extDmabufFormat,
    .done = extSessionDone,
    .stopped = extSessionStopped,
};

void startCapture(Connection &c, Output *o, Protocol protocol)
{
    o->state = Output::State::Capturing;
    if (protocol == Protocol::WlrScreencopy) {
        o->wlrFrame = zwlr_screencopy_manager_v1_capture_output(c.wlr, 0, o->output);
        zwlr_screencopy_frame_v1_add_listener(o->wlrFrame, &wlrFrameListener, o);
    } else {
        o->source = ext_output_image_capture_source_manager_v1_create_source(c.extSources, o->output);
        o->session = ext_image_copy_capture_manager_v1_create_session(c.ext, o->source, 0);
        ext_image_copy_capture_session_v1_add_listener(o->session, &extSessionListener, o);
    }
}

} // namespace

Globals advertisedGlobals()
{
    static std::optional<Globals> cached;
    if (!cached) {
        Connection c;
        cached = openConnection(c, false, 1000) ? c.seen : Globals{};
    }
    return *cached;
}

QList<OutputFrame> captureOutputs(Protocol protocol, QString *error, int timeoutMs)
{
    auto failWith = [error](const QString &message) {
        if (error)
            *error = message;
        return QList<OutputFrame>();
    };

    if (protocol == Protocol::None)
        return failWith(QStringLiteral("no screencopy protocol"));

    Connection c;
    if (!openConnection(c, true, timeoutMs))
        return failWith(QStringLiteral("no Wayland connection"));
    const bool bound = protocol == Protocol::WlrScreencopy ? c.wlr != nullptr
                                                           : c.ext && c.extSources;
    if (!bound || !c.shm)
        return failWith(QStringLiteral("the compositor does not offer the protocol"));
    if (c.outputs.empty())
        return failWith(QStringLiteral("no outputs"));

    for (auto &o : c.outputs)
        startCapture(c, o.get(), protocol);

    const bool settled = dispatchUntil(c, [&c] {
        for (const auto &o : c.outputs) {
            if (o->state == Output::State::Capturing)
                return false;
        }
        return true;
    }, timeoutMs);
    if (!settled)
        return failWith(QStringLiteral("timed out waiting for the compositor"));

    QList<OutputFrame> frames;
    for (const auto &o : c.outputs) {
        if (o->state != Output::State::Ready)
            return failWith(QStringLiteral("output %1: %2").arg(o->name, o->error));
        frames.append({o->name, o->position, o->image});
    }
    return frames;
}

} // namespace Screen::Screencopy
