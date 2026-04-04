#ifndef TESTS_MP4BOXES_H
#define TESTS_MP4BOXES_H

/**
 * Just enough ISO BMFF to read what a muxer wrote (brand, durations, track handlers)
 * with no decoder involved, so the exporter and encoder tests can check real files.
 */

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QString>

namespace TestSupport {

struct Mp4Track {
    QByteArray handler;   // "vide", "soun", ...
    qint64 durationMs = -1;
};

struct Mp4Info {
    QByteArray brand;
    qint64 durationMs = -1;
    QList<Mp4Track> tracks;
    QList<QByteArray> topLevel;   // box types in file order
};

inline quint64 readBe(const QByteArray &data, qsizetype at, int bytes)
{
    quint64 value = 0;
    for (int i = 0; i < bytes; ++i)
        value = (value << 8) | quint8(data.at(at + i));
    return value;
}

// mvhd and mdhd share their layout up to the duration.
inline qint64 headerDurationMs(const QByteArray &data, qsizetype body)
{
    const bool wide = data.at(body) == 1;
    const qsizetype timescaleAt = body + 4 + (wide ? 16 : 8);
    const quint64 timescale = readBe(data, timescaleAt, 4);
    const quint64 duration = readBe(data, timescaleAt + 4, wide ? 8 : 4);
    return timescale ? qint64(duration * 1000 / timescale) : -1;
}

inline void walkBoxes(const QByteArray &data, qsizetype begin, qsizetype end, Mp4Info *info,
                      Mp4Track *track)
{
    qsizetype at = begin;
    while (at + 8 <= end) {
        quint64 size = readBe(data, at, 4);
        const QByteArray type = data.mid(at + 4, 4);
        qsizetype header = 8;
        if (size == 1) {
            size = readBe(data, at + 8, 8);
            header = 16;
        } else if (size == 0) {
            size = quint64(end - at);
        }
        if (size < quint64(header) || at + qsizetype(size) > end)
            return;
        const qsizetype body = at + header;
        const qsizetype boxEnd = at + qsizetype(size);
        if (begin == 0)
            info->topLevel.append(type);

        if (type == "ftyp") {
            info->brand = data.mid(body, 4);
        } else if (type == "moov" || type == "mdia") {
            walkBoxes(data, body, boxEnd, info, track);
        } else if (type == "trak") {
            Mp4Track found;
            walkBoxes(data, body, boxEnd, info, &found);
            info->tracks.append(found);
        } else if (type == "mvhd") {
            info->durationMs = headerDurationMs(data, body);
        } else if (type == "mdhd" && track) {
            track->durationMs = headerDurationMs(data, body);
        } else if (type == "hdlr" && track) {
            track->handler = data.mid(body + 8, 4);
        }
        at = boxEnd;
    }
}

inline Mp4Info readMp4(const QString &path)
{
    QFile file(path);
    Mp4Info info;
    if (!file.open(QIODevice::ReadOnly))
        return info;
    const QByteArray data = file.readAll();
    walkBoxes(data, 0, data.size(), &info, nullptr);
    return info;
}

inline const Mp4Track *trackOf(const Mp4Info &info, const char *handler)
{
    for (const Mp4Track &track : info.tracks) {
        if (track.handler == handler)
            return &track;
    }
    return nullptr;
}

} // namespace TestSupport

#endif // TESTS_MP4BOXES_H
