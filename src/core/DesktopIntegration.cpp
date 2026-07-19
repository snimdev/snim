#include "core/DesktopIntegration.h"
#include "core/Sandbox.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include <optional>

namespace Core::DesktopIntegration {

namespace {

constexpr auto kDesktopFileName = "dev.snim.Snim.desktop";
constexpr auto kIconName = "dev.snim.Snim";
constexpr auto kIconResource = ":/icons/icons/app-icon.svg";
constexpr auto kEntryTemplateResource = ":/desktop/dev.snim.Snim.desktop.in";

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

// One entry's verdict for this executable; status() weighs the user's against the system's.
Status judge(const QString &contents)
{
    if (!declaresScreenShot2(contents))
        return Status::MissingAuthorizationKey;
    const QString target = execTarget(contents);
    if (target.isEmpty() || canonical(target) != executablePath())
        return Status::ExecMismatch;
    return Status::Installed;
}

std::optional<QString> readEntry(const QString &path)
{
    QFile entry(path);
    if (!entry.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;
    return QString::fromUtf8(entry.readAll());
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
    QFile entryTemplate(QString::fromLatin1(kEntryTemplateResource));
    if (!entryTemplate.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(entryTemplate.readAll())
        .replace(QLatin1String("@SNIM_DESKTOP_EXEC@"), execPath);
}

Status status()
{
#ifndef Q_OS_LINUX
    return Status::NotApplicable;
#else
    // Flatpak exports its own desktop entry, and a host entry cannot point into the sandbox.
    if (Sandbox::isFlatpak())
        return Status::NotApplicable;

    // The first match is the one launchers and KWin see: the user's entry shadows a packaged one.
    const QStringList entries =
        QStandardPaths::locateAll(QStandardPaths::ApplicationsLocation, QLatin1String(kDesktopFileName));
    const std::optional<QString> contents =
        entries.isEmpty() ? std::nullopt : readEntry(entries.first());
    if (!contents)
        return Status::NotInstalled;

    const Status verdict = judge(*contents);
    if (verdict != Status::Installed && entries.size() > 1
        && QFileInfo(entries.first()) == QFileInfo(desktopFilePath())) {
        const std::optional<QString> next = readEntry(entries.at(1));
        if (next && judge(*next) == Status::Installed)
            return Status::StaleUserEntry;
    }
    return verdict;
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
    if (Sandbox::isFlatpak())
        return fail(QStringLiteral("Flatpak manages Snim's desktop entry."));

    const QString desktopPath = desktopFilePath();
    const QString applicationsDir = QFileInfo(desktopPath).absolutePath();
    if (!QDir().mkpath(applicationsDir))
        return fail(QStringLiteral("Cannot create %1").arg(applicationsDir));

    const QString contents = desktopEntryContents(quoteExec(executablePath()));
    if (contents.isEmpty())
        return fail(QStringLiteral("This build lacks its desktop entry template."));
    QSaveFile entry(desktopPath);
    if (!entry.open(QIODevice::WriteOnly | QIODevice::Text))
        return fail(QStringLiteral("Cannot write %1: %2").arg(desktopPath, entry.errorString()));
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

bool repair(QString *errorOut)
{
#ifdef Q_OS_LINUX
    if (status() == Status::StaleUserEntry) {
        const QString desktopPath = desktopFilePath();
        if (!QFile::remove(desktopPath)) {
            if (errorOut)
                *errorOut = QStringLiteral("Cannot remove %1").arg(desktopPath);
            return false;
        }
        refreshCaches(QFileInfo(desktopPath).absolutePath());
        if (errorOut)
            errorOut->clear();
        return true;
    }
#endif
    return install(errorOut);
}

} // namespace Core::DesktopIntegration
