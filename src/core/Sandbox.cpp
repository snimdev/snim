#include "core/Sandbox.h"

#include <QFileInfo>
#include <QtGlobal>

namespace Core::Sandbox {

bool isFlatpak()
{
    return isFlatpak(QStringLiteral("/.flatpak-info"));
}

bool isFlatpak(const QString &infoFilePath)
{
    return qEnvironmentVariableIsSet("FLATPAK_ID") || QFileInfo::exists(infoFilePath);
}

} // namespace Core::Sandbox
