#include "upload/strategies/StubUploader.h"

#include <QMetaObject>

namespace Upload {

void StubUploader::upload(const QString &localPath, const QString &keyHint)
{
    Q_UNUSED(localPath); Q_UNUSED(keyHint);
    QMetaObject::invokeMethod(this, [this] {
        emit failed(tr("Upload is not configured."));
    }, Qt::QueuedConnection);
}

} // namespace Upload
