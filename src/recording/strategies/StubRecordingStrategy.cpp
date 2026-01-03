#include "recording/strategies/StubRecordingStrategy.h"

namespace Recording {

void StubRecordingStrategy::start(const RecordTarget &, const QString &)
{
    emit failed(QStringLiteral("Screen recording is not supported on this platform yet."));
}

} // namespace Recording
