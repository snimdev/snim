#include "recording/RecordingJournal.h"

#include "core/Settings.h"

#include <QFile>
#include <QFileInfo>

namespace Recording {

void RecordingJournal::add(const QString &path)
{
    QStringList entries = Core::Settings::recordingsInProgress();
    if (path.isEmpty() || entries.contains(path))
        return;
    entries.append(path);
    Core::Settings::setRecordingsInProgress(entries);
}

bool RecordingJournal::isRecoverable(const QString &path)
{
    const QFileInfo info(path);
    return info.isFile() && info.size() >= kMinimumBytes;
}

QStringList RecordingJournal::recoverable()
{
    QStringList found;
    for (const QString &path : Core::Settings::recordingsInProgress()) {
        if (isRecoverable(path))
            found.append(path);
    }
    return found;
}

void RecordingJournal::prune()
{
    QStringList kept;
    for (const QString &path : Core::Settings::recordingsInProgress()) {
        if (isRecoverable(path))
            kept.append(path);
        else if (QFileInfo(path).isFile())
            QFile::remove(path);
    }
    Core::Settings::setRecordingsInProgress(kept);
}

void RecordingJournal::discard(const QString &path)
{
    QFile::remove(path);
    QStringList entries = Core::Settings::recordingsInProgress();
    entries.removeAll(path);
    Core::Settings::setRecordingsInProgress(entries);
}

} // namespace Recording
