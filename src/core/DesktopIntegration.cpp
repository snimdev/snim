#include "core/DesktopIntegration.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

namespace Core::DesktopIntegration {

namespace {

constexpr auto kDesktopFileName = "dev.snim.Snim.desktop";
constexpr auto kIconName = "dev.snim.Snim";
constexpr auto kIconResource = ":/icons/icons/app-icon.svg";

QString dataHome()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

#ifdef Q_OS_LINUX

// Falls back to the plain absolute path when the target is gone (canonical is empty then).
QString canonical(const QString &path)
{
    const QFileInfo info(path);
    const QString resolved = info.canonicalFilePath();
    return resolved.isEmpty() ? info.absoluteFilePath() : resolved;
}

QString executablePath()
{
    return canonical(QCoreApplication::applicationFilePath());
}

// Desktop entry spec: an Exec argument containing spaces has to be quoted.
QString quoteExec(const QString &path)
{
    return path.contains(u' ') ? u'"' + path + u'"' : path;
}

QString execTarget(const QString &contents)
{
    const QStringList lines = contents.split(u'\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QLatin1String("Exec=")))
            continue;
        const QString value = trimmed.mid(5).trimmed();
        if (value.startsWith(u'"')) {
            const int end = value.indexOf(u'"', 1);
            return end > 0 ? value.mid(1, end - 1) : value.mid(1);
        }
        const int space = value.indexOf(u' ');
        return space > 0 ? value.left(space) : value;
    }
    return {};
}

bool declaresScreenShot2(const QString &contents)
{
    const QStringList lines = contents.split(u'\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("X-KDE-DBUS-Restricted-Interfaces"))
            && trimmed.contains(QLatin1String("org.kde.KWin.ScreenShot2")))
            return true;
    }
    return false;
}

void refreshCaches(const QString &applicationsDir)
{
    if (QStandardPaths::isTestModeEnabled())   // tests must not rebuild the live session caches
        return;
    const QString updateDb = QStandardPaths::findExecutable(QStringLiteral("update-desktop-database"));
    if (!updateDb.isEmpty())
        QProcess::startDetached(updateDb, {applicationsDir});
    const QString sycoca = QStandardPaths::findExecutable(QStringLiteral("kbuildsycoca6"));
    if (!sycoca.isEmpty())
        QProcess::startDetached(sycoca, {});
}

#endif // Q_OS_LINUX

} // namespace

QString desktopFilePath()
{
    return dataHome() + QStringLiteral("/applications/") + QLatin1String(kDesktopFileName);
}

QString iconFilePath()
{
    return dataHome() + QStringLiteral("/icons/hicolor/scalable/apps/")
           + QLatin1String(kIconName) + QStringLiteral(".svg");
}

QString desktopEntryContents(const QString &execPath)
{
    return QStringLiteral(
               "[Desktop Entry]\n"
               "Type=Application\n"
               "Name=Snim\n"
               "GenericName=Screenshot Tool\n"
               "Comment=Screenshot, screen recording, OCR and upload tool\n"
               "Exec=%1\n"
               "Icon=dev.snim.Snim\n"
               "Terminal=false\n"
               "Categories=Utility;Qt;\n"
               "StartupWMClass=snim\n"
               "# KWin only authorizes ScreenShot2 for apps whose desktop entry declares it.\n"
               "X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2\n")
        .arg(execPath);
}

Status status()
{
#ifndef Q_OS_LINUX
    return Status::NotApplicable;
#else
    QFile entry(desktopFilePath());
    if (!entry.exists() || !entry.open(QIODevice::ReadOnly | QIODevice::Text))
        return Status::NotInstalled;
    const QString contents = QString::fromUtf8(entry.readAll());
    entry.close();

    if (!declaresScreenShot2(contents))
        return Status::MissingAuthorizationKey;

    const QString target = execTarget(contents);
    if (target.isEmpty() || canonical(target) != executablePath())
        return Status::ExecMismatch;

    return Status::Installed;
#endif
}

bool install(QString *errorOut)
{
    const auto fail = [errorOut](const QString &message) {
        if (errorOut)
            *errorOut = message;
        return false;
    };

#ifndef Q_OS_LINUX
    return fail(QStringLiteral("Desktop integration is only needed on Linux."));
#else
    const QString desktopPath = desktopFilePath();
    const QString applicationsDir = QFileInfo(desktopPath).absolutePath();
    if (!QDir().mkpath(applicationsDir))
        return fail(QStringLiteral("Cannot create %1").arg(applicationsDir));

    QSaveFile entry(desktopPath);
    if (!entry.open(QIODevice::WriteOnly | QIODevice::Text))
        return fail(QStringLiteral("Cannot write %1: %2").arg(desktopPath, entry.errorString()));
    const QString contents = desktopEntryContents(quoteExec(executablePath()));
    entry.write(contents.toUtf8());
    if (!entry.commit())
        return fail(QStringLiteral("Cannot write %1: %2").arg(desktopPath, entry.errorString()));

    // The icon is cosmetic: a missing one never blocks the ScreenShot2 authorization.
    const QString iconPath = iconFilePath();
    if (QDir().mkpath(QFileInfo(iconPath).absolutePath())) {
        QFile::remove(iconPath);
        if (QFile::copy(QLatin1String(kIconResource), iconPath)) {
            // Resource copies inherit read-only permissions, so a later reinstall could not replace it.
            QFile::setPermissions(iconPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                | QFileDevice::ReadGroup | QFileDevice::ReadOther);
        } else {
            qWarning() << "DesktopIntegration: could not write icon" << iconPath;
        }
    }

    refreshCaches(applicationsDir);
    if (errorOut)
        errorOut->clear();
    return true;
#endif
}

} // namespace Core::DesktopIntegration
