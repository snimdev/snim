#ifndef EDITOR_VIDEO_ANIMATIONENCODER_H
#define EDITOR_VIDEO_ANIMATIONENCODER_H

#include "editor/video/AnimationParams.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <memory>

class QIODevice;
class QSaveFile;

namespace Editor::Video {

/**
 * Streaming writer for one short-loop format (Strategy): frames go in one at a time and
 * the file is committed on finish(). The public steps are a Template Method: they own
 * everything the formats share (the output file, frame conversion, the timeline, failure
 * cleanup) and call the format's hooks for the encoding itself. create() is the Factory
 * Method that picks the format.
 */
class AnimationEncoder
{
public:
    virtual ~AnimationEncoder();

    AnimationEncoder(const AnimationEncoder &) = delete;
    AnimationEncoder &operator=(const AnimationEncoder &) = delete;

    // The encoder for `format`.
    static std::unique_ptr<AnimationEncoder> create(AnimationFormat format);

    // Opens `path` for a `size`-pixel canvas and clamps `params`. The file is created
    // here, not in finish(), so a permission problem surfaces before the caller spends
    // minutes decoding frames. Timestamps are SOURCE milliseconds; only their spacing
    // matters, since it becomes each frame's on-screen duration.
    bool begin(const QString &path, const QSize &size, const AnimationParams &params,
               QString *errorOut = nullptr);

    // Appends one frame, shown until the next one. A frame that does not match the
    // canvas is scaled to it. Timestamps that do not advance are nudged forward so a
    // repeated frame holds for at least 1 ms instead of failing the encode.
    bool addFrame(const QImage &frame, qint64 sourceMs, QString *errorOut = nullptr);

    // Closes the last frame's duration and writes the file. A false return leaves no
    // file behind.
    bool finish(QString *errorOut = nullptr);

    // Drops the encoder and the partial file. Safe at any point, including after
    // finish() and from the destructor.
    void cancel();

protected:
    AnimationEncoder();

    // Starts the format's encoder; params are already clamped and the output is open.
    virtual bool onBegin(const QSize &size, const AnimationParams &params,
                         QString *errorOut) = 0;
    // One RGBA8888 frame at the canvas size, at `ms` on the rebased output timeline.
    virtual bool onFrame(const QImage &rgba, qint64 ms, QString *errorOut) = 0;
    // Ends the last frame at `endMs` and writes whatever is left to output().
    virtual bool onFinish(qint64 endMs, QString *errorOut) = 0;
    // Drops the format's encoder state; the base discards the file afterwards.
    virtual void onCancel() = 0;

    // The open output, valid from onBegin() until onFinish() returns.
    [[nodiscard]] QIODevice *output() const;
    // "Cannot write <path>: <reason>", for a failed write to output().
    [[nodiscard]] QString writeError() const;

private:
    bool fail(QString *errorOut, const QString &message);

    // QSaveFile so a half-written animation can never land: its destructor discards the
    // temp unless commit() runs, which only happens on a successful finish().
    std::unique_ptr<QSaveFile> m_file;
    QString m_path;
    QSize m_size;
    int m_fps = 10;
    qint64 m_firstMs = 0;    // frame 0 is rebased to 0 on the output timeline
    qint64 m_prevMs = 0;     // last accepted timestamp, already rebased
    bool m_hasPrev = false;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_ANIMATIONENCODER_H
