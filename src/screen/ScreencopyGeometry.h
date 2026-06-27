#ifndef SCREEN_SCREENCOPYGEOMETRY_H
#define SCREEN_SCREENCOPYGEOMETRY_H

#include <QImage>
#include <QList>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QTransform>
#include <QtMath>
#include <optional>

namespace Screen::Screencopy {

/**
 * The pure half of the native Wayland screencopy path: protocol choice, wl_shm pixel
 * formats, output transforms and stitching the per-output frames into one
 * virtual-desktop pixmap. Header-only and free of Wayland headers, so the unit tests
 * build on every platform and the client code stays a thin protocol shim.
 */

enum class Protocol {
    None,
    ExtImageCopyCapture,    // ext-image-copy-capture-v1 + ext-output-image-capture-source
    WlrScreencopy,          // zwlr_screencopy_manager_v1
};

// The standard protocol first; forced only wins when it is actually advertised.
inline Protocol pickProtocol(bool hasExt, bool hasWlr, Protocol forced = Protocol::None)
{
    if (forced == Protocol::ExtImageCopyCapture && hasExt)
        return forced;
    if (forced == Protocol::WlrScreencopy && hasWlr)
        return forced;
    if (forced != Protocol::None)
        return Protocol::None;
    if (hasExt)
        return Protocol::ExtImageCopyCapture;
    if (hasWlr)
        return Protocol::WlrScreencopy;
    return Protocol::None;
}

constexpr quint32 fourcc(char a, char b, char c, char d)
{
    return quint32(quint8(a)) | quint32(quint8(b)) << 8 | quint32(quint8(c)) << 16
           | quint32(quint8(d)) << 24;
}

// wl_shm.format values: the two legacy codes, DRM fourcc for the rest.
namespace ShmFormat {
constexpr quint32 ARGB8888 = 0;
constexpr quint32 XRGB8888 = 1;
constexpr quint32 XBGR8888 = fourcc('X', 'B', '2', '4');
constexpr quint32 ABGR8888 = fourcc('A', 'B', '2', '4');
constexpr quint32 RGB888 = fourcc('R', 'G', '2', '4');
constexpr quint32 BGR888 = fourcc('B', 'G', '2', '4');
constexpr quint32 RGB565 = fourcc('R', 'G', '1', '6');
constexpr quint32 XRGB2101010 = fourcc('X', 'R', '3', '0');
constexpr quint32 ARGB2101010 = fourcc('A', 'R', '3', '0');
constexpr quint32 XBGR2101010 = fourcc('X', 'B', '3', '0');
constexpr quint32 ABGR2101010 = fourcc('A', 'B', '3', '0');
} // namespace ShmFormat

// Screen content is opaque, so alpha formats read as their X twins (no garbage alpha).
inline QImage::Format imageFormatFor(quint32 shmFormat)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    switch (shmFormat) {
    case ShmFormat::XRGB8888:
    case ShmFormat::ARGB8888:
        return QImage::Format_RGB32;
    case ShmFormat::XBGR8888:
    case ShmFormat::ABGR8888:
        return QImage::Format_RGBX8888;
    case ShmFormat::RGB888:
        return QImage::Format_BGR888;
    case ShmFormat::BGR888:
        return QImage::Format_RGB888;
    case ShmFormat::RGB565:
        return QImage::Format_RGB16;
    case ShmFormat::XRGB2101010:
    case ShmFormat::ARGB2101010:
        return QImage::Format_RGB30;
    case ShmFormat::XBGR2101010:
    case ShmFormat::ABGR2101010:
        return QImage::Format_BGR30;
    default:
        return QImage::Format_Invalid;
    }
#else
    // DRM formats are little-endian; a big-endian host would need byte swapping.
    Q_UNUSED(shmFormat);
    return QImage::Format_Invalid;
#endif
}

inline int bytesPerPixel(quint32 shmFormat)
{
    switch (imageFormatFor(shmFormat)) {
    case QImage::Format_Invalid:
        return 0;
    case QImage::Format_RGB888:
    case QImage::Format_BGR888:
        return 3;
    case QImage::Format_RGB16:
        return 2;
    default:
        return 4;
    }
}

// 8-bit formats first: cheapest to convert and what every wlroots renderer offers.
inline std::optional<quint32> pickShmFormat(const QList<quint32> &offered)
{
    static constexpr quint32 preference[] = {
        ShmFormat::XRGB8888, ShmFormat::ARGB8888, ShmFormat::XBGR8888, ShmFormat::ABGR8888,
        ShmFormat::XRGB2101010, ShmFormat::ARGB2101010, ShmFormat::XBGR2101010,
        ShmFormat::ABGR2101010, ShmFormat::BGR888, ShmFormat::RGB888, ShmFormat::RGB565,
    };
    for (quint32 format : preference) {
        if (offered.contains(format))
            return format;
    }
    return std::nullopt;
}

// Stride of a client-allocated buffer, rows padded to 4 bytes; 0 when unsupported.
inline int strideFor(quint32 shmFormat, int width)
{
    const int bpp = bytesPerPixel(shmFormat);
    return bpp > 0 && width > 0 ? (width * bpp + 3) & ~3 : 0;
}

inline QImage flippedImage(const QImage &image, Qt::Orientations orientations)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return image.flipped(orientations);
#else
    return image.mirrored(orientations.testFlag(Qt::Horizontal), orientations.testFlag(Qt::Vertical));
#endif
}

/**
 * The frame as the user saw it. @p transform is the wl_output.transform the compositor
 * applied to the buffer (rotation counter-clockwise in bits 0-1, flip in bit 2), so it
 * is undone the way grim does: y-invert first, then rotate clockwise, then mirror.
 */
inline QImage uprightFrame(const QImage &raw, quint32 transform, bool yInvert)
{
    if (raw.isNull())
        return raw;

    QImage image = yInvert ? flippedImage(raw, Qt::Vertical) : raw;
    const int quarterTurns = int(transform & 3);
    if (quarterTurns != 0)
        image = image.transformed(QTransform().rotate(90.0 * quarterTurns));
    if (transform & 4)
        image = flippedImage(image, Qt::Horizontal);
    return image;
}

// One captured output, already upright. position is wl_output.geometry's x/y.
struct OutputFrame {
    QString name;
    QPoint position;
    QImage image;
};

// A Qt screen as the selector sees it: QScreen::name() and its logical geometry.
struct ScreenSlot {
    QString name;
    QRect geometry;
};

// For each frame, the screen it shows: by output name, then by layout position, else -1.
inline QList<int> matchFramesToScreens(const QList<OutputFrame> &frames, const QList<ScreenSlot> &screens)
{
    QList<int> match(frames.size(), -1);
    QList<bool> taken(screens.size(), false);

    for (qsizetype f = 0; f < frames.size(); ++f) {
        if (frames[f].name.isEmpty())
            continue;
        for (qsizetype s = 0; s < screens.size(); ++s) {
            if (!taken[s] && screens[s].name == frames[f].name) {
                match[f] = int(s);
                taken[s] = true;
                break;
            }
        }
    }
    for (qsizetype f = 0; f < frames.size(); ++f) {
        if (match[f] != -1)
            continue;
        for (qsizetype s = 0; s < screens.size(); ++s) {
            if (!taken[s] && screens[s].geometry.topLeft() == frames[f].position) {
                match[f] = int(s);
                taken[s] = true;
                break;
            }
        }
    }
    return match;
}

// A frame's own DPR. The smaller axis wins: compositors truncate fractional logical sizes.
inline qreal frameDevicePixelRatio(const QImage &image, const QRect &geometry)
{
    if (image.isNull() || geometry.width() <= 0 || geometry.height() <= 0)
        return 0.0;
    return qMin(image.width() / qreal(geometry.width()), image.height() / qreal(geometry.height()));
}

/**
 * The virtual-desktop frame the selectors expect: every output at its screen's logical
 * place, at the densest output's DPR, uncovered areas black. Outputs at that DPR are
 * copied pixel for pixel, sparser ones scaled up. Null when no frame matched a screen.
 */
inline QImage stitchFrames(const QList<OutputFrame> &frames, const QList<ScreenSlot> &screens,
                           const QRect &virtualGeometry)
{
    const QList<int> match = matchFramesToScreens(frames, screens);
    qreal dpr = 0.0;
    for (qsizetype f = 0; f < frames.size(); ++f) {
        if (match[f] >= 0)
            dpr = qMax(dpr, frameDevicePixelRatio(frames[f].image, screens[match[f]].geometry));
    }
    if (dpr <= 0.0 || virtualGeometry.isEmpty())
        return {};

    struct Placement {
        QRectF target;
        const QImage *image;
    };
    QList<Placement> placements;
    QRectF extent(0, 0, virtualGeometry.width() * dpr, virtualGeometry.height() * dpr);
    for (qsizetype f = 0; f < frames.size(); ++f) {
        if (match[f] < 0 || frames[f].image.isNull())
            continue;
        const QRect geometry = screens[match[f]].geometry;
        const QImage &image = frames[f].image;
        const QPointF origin((geometry.x() - virtualGeometry.x()) * dpr,
                             (geometry.y() - virtualGeometry.y()) * dpr);
        const bool native = qAbs(frameDevicePixelRatio(image, geometry) - dpr) < 0.01;
        const QRectF target = native ? QRectF(origin.toPoint(), QSizeF(image.size()))
                                     : QRectF(origin, QSizeF(geometry.size()) * dpr);
        placements.append({target, &image});
        extent |= target;
    }

    QImage canvas(qCeil(extent.right() - 0.001), qCeil(extent.bottom() - 0.001), QImage::Format_RGB32);
    canvas.fill(Qt::black);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const Placement &placement : placements) {
        if (placement.target.size() == QSizeF(placement.image->size()))
            painter.drawImage(placement.target.topLeft().toPoint(), *placement.image);
        else
            painter.drawImage(placement.target, *placement.image);
    }
    painter.end();

    canvas.setDevicePixelRatio(dpr);
    return canvas;
}

} // namespace Screen::Screencopy

#endif // SCREEN_SCREENCOPYGEOMETRY_H
