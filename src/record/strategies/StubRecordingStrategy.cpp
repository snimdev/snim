#include "record/strategies/StubRecordingStrategy.h"

namespace Record {

void StubRecordingStrategy::start(const RecordTarget &, const QString &)
{
    emit failed(QStringLiteral("Screen recording is not supported on this platform yet."));
}

} // namespace Record
