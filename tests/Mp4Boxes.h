#ifndef TESTS_MP4BOXES_H
#define TESTS_MP4BOXES_H

/**
 * Just enough ISO BMFF to read what a muxer wrote (brand, durations, track handlers)
 * with no decoder involved, so the exporter and encoder tests can check real files.
 * A fragmented file's durations come from its moof sample runs.
 */

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QString>

#include <bit>

namespace TestSupport {

struct Mp4Track {
    QByteArray handler;   // "vide", "soun", ...
    qint64 durationMs = -1;
    quint32 trackId = 0;
    quint64 timescale = 0;
    quint64 trexSampleDuration = 0;   // the moov/mvex default for its fragments
    quint64 fragmentTicks = 0;        // sample durations summed over every moof
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

inline qint64 ticksToMs(quint64 ticks, quint64 timescale)
{
    return timescale ? qint64(ticks * 1000 / timescale) : -1;
}

// mvhd, mdhd and tkhd share their layout up to the field after the two times.
inline qsizetype afterTimes(const QByteArray &data, qsizetype body)
{
    const bool wide = data.at(body) == 1;
    return body + 4 + (wide ? 16 : 8);
}

// mvhd and mdhd: the timescale, then a duration as wide as the times.
inline qint64 headerDurationMs(const QByteArray &data, qsizetype body)
{
    const bool wide = data.at(body) == 1;
    const qsizetype timescaleAt = afterTimes(data, body);
    const quint64 timescale = readBe(data, timescaleAt, 4);
    const quint64 duration = readBe(data, timescaleAt + 4, wide ? 8 : 4);
    return ticksToMs(duration, timescale);
}

// Calls visit(type, body, boxEnd) for each box in [begin, end), stopping at a truncated one.
template<typename Visit>
inline void forEachBox(const QByteArray &data, qsizetype begin, qsizetype end, Visit visit)
{
    qsizetype at = begin;
    while (at + 8 <= end) {
        quint64 size = readBe(data, at, 4);
        const QByteArray type = data.mid(at + 4, 4);
        qsizetype header = 8;
        if (size == 1 && at + 16 <= end) {
            size = readBe(data, at + 8, 8);
            header = 16;
        } else if (size == 0) {
            size = quint64(end - at);
        }
        if (size < quint64(header) || at + qsizetype(size) > end)
            return;
        visit(type, at + header, at + qsizetype(size));
        at += qsizetype(size);
    }
}

inline Mp4Track *trackWithId(Mp4Info *info, quint64 trackId)
{
    for (Mp4Track &track : info->tracks) {
        if (track.trackId == trackId)
            return &track;
    }
    return nullptr;
}

// A trun's sample durations: its own when flag 0x100 is set, else the fragment default.
inline quint64 runTicks(const QByteArray &data, qsizetype body, qsizetype end,
                        quint64 defaultDuration)
{
    const quint64 flags = readBe(data, body + 1, 3);
    quint64 count = readBe(data, body + 4, 4);
    if (!(flags & 0x100))
        return count * defaultDuration;
    // Duration, size, flags and composition offset each take 4 bytes per sample when set.
    const qsizetype stride = 4 * std::popcount(flags & 0xf00);
    qsizetype at = body + 8 + (flags & 0x01 ? 4 : 0) + (flags & 0x04 ? 4 : 0);
    quint64 ticks = 0;
    for (; count > 0 && at + 4 <= end; --count, at += stride)
        ticks += readBe(data, at, 4);
    return ticks;
}

// Adds a traf's sample durations to its track, defaulting to the tfhd, then the trex value.
inline void readTraf(const QByteArray &data, qsizetype begin, qsizetype end, Mp4Info *info)
{
    Mp4Track *track = nullptr;
    quint64 defaultDuration = 0;
    forEachBox(data, begin, end, [&](const QByteArray &type, qsizetype body, qsizetype boxEnd) {
        if (type == "tfhd") {
            const quint64 flags = readBe(data, body + 1, 3);
            track = trackWithId(info, readBe(data, body + 4, 4));
            const qsizetype durationAt = body + 8 + (flags & 0x01 ? 8 : 0)
                                         + (flags & 0x02 ? 4 : 0);
            if (flags & 0x08)
                defaultDuration = readBe(data, durationAt, 4);
            else
                defaultDuration = track ? track->trexSampleDuration : 0;
        } else if (type == "trun" && track) {
            track->fragmentTicks += runTicks(data, body, boxEnd, defaultDuration);
        }
    });
}

inline void walkBoxes(const QByteArray &data, qsizetype begin, qsizetype end, Mp4Info *info,
                      Mp4Track *track)
{
    forEachBox(data, begin, end, [&](const QByteArray &type, qsizetype body, qsizetype boxEnd) {
        if (begin == 0)
            info->topLevel.append(type);

        if (type == "ftyp") {
            info->brand = data.mid(body, 4);
        } else if (type == "moov" || type == "mdia" || type == "mvex" || type == "moof") {
            walkBoxes(data, body, boxEnd, info, track);
        } else if (type == "trak") {
            Mp4Track found;
            walkBoxes(data, body, boxEnd, info, &found);
            info->tracks.append(found);
        } else if (type == "traf") {
            readTraf(data, body, boxEnd, info);
        } else if (type == "mvhd") {
            info->durationMs = headerDurationMs(data, body);
        } else if (type == "tkhd" && track) {
            track->trackId = quint32(readBe(data, afterTimes(data, body), 4));
        } else if (type == "mdhd" && track) {
            track->timescale = readBe(data, afterTimes(data, body), 4);
            track->durationMs = headerDurationMs(data, body);
        } else if (type == "hdlr" && track) {
            track->handler = data.mid(body + 8, 4);
        } else if (type == "trex") {
            if (Mp4Track *target = trackWithId(info, readBe(data, body + 4, 4)))
                target->trexSampleDuration = readBe(data, body + 12, 4);
        }
    });
}

// empty_moov leaves 0 in mvhd and mdhd, so the fragments stand in for them.
inline void applyFragmentDurations(Mp4Info *info)
{
    qint64 longest = 0;
    for (Mp4Track &track : info->tracks) {
        if (track.durationMs == 0 && track.fragmentTicks > 0)
            track.durationMs = ticksToMs(track.fragmentTicks, track.timescale);
        longest = qMax(longest, track.durationMs);
    }
    if (info->durationMs == 0)
        info->durationMs = longest;
}

inline Mp4Info readMp4(const QString &path)
{
    QFile file(path);
    Mp4Info info;
    if (!file.open(QIODevice::ReadOnly))
        return info;
    const QByteArray data = file.readAll();
    walkBoxes(data, 0, data.size(), &info, nullptr);
    applyFragmentDurations(&info);
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
