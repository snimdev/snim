#ifndef RECORDING_RECORDINGJOURNAL_H
#define RECORDING_RECORDINGJOURNAL_H

#include <QString>
#include <QStringList>

namespace Record {

/**
 * Remembers each recording from the moment it starts until its file is gone: the
 * checkpoint/rollback tactic's journal. The fragmented file is the checkpoint, and the
 * next launch rolls back to it by offering whatever a crash left behind. Saving or
 * discarding in the VideoEditor moves or deletes the temp file, which is all it takes to
 * leave the journal. Persisted through Core::Settings, whose writes land immediately.
 */
class RecordingJournal
{
public:
    // Smaller than this, a file holds headers at most and no footage.
    static constexpr qint64 kMinimumBytes = 1024;

    static void add(const QString &path);

    [[nodiscard]] static bool isRecoverable(const QString &path);

    // Journaled files that still hold footage, oldest first.
    [[nodiscard]] static QStringList recoverable();

    // Forgets files that are gone or hold no footage, deleting the latter. Only while
    // nothing records: a recording that just started has no footage yet.
    static void prune();

    // Deletes the file and forgets it.
    static void discard(const QString &path);
};

} // namespace Record

#endif // RECORDING_RECORDINGJOURNAL_H
