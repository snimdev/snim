#include "upload/strategies/StubUploader.h"

#include <QMetaObject>

namespace Upload {

void StubUploader::upload(const QString &localPath, const QString &keyHint)
{
    Q_UNUSED(localPath); Q_UNUSED(keyHint);
    const QString message = m_message.isEmpty() ? tr("Upload is not configured.") : m_message;
    QMetaObject::invokeMethod(this, [this, message] {
        emit failed(message);
    }, Qt::QueuedConnection);
}

} // namespace Upload
