#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>

#include "core/DesktopIntegration.h"

using namespace Core;

// The KDE desktop-entry assistant. Test mode redirects GenericDataLocation, so
// install() writes into a throwaway data home instead of ~/.local/share, and the
// cache refreshers are skipped. applicationFilePath() is the test binary here,
// which is exactly what install() writes and status() then has to match.
class tst_DesktopIntegration : public QObject
{
    Q_OBJECT

    static QString readEntry()
    {
        QFile file(DesktopIntegration::desktopFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    }

    static bool writeEntry(const QString &contents)
    {
        QFile file(DesktopIntegration::desktopFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return false;
        return file.write(contents.toUtf8()) == contents.toUtf8().size();
    }

    // Stands in for the mounted .AppImage file the runtime points $APPIMAGE at.
    QString makeFakeAppImage(const QString &name)
    {
        const QString path = m_appImageDir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write("fake");
        file.close();
        return QFileInfo(path).canonicalFilePath();
    }

    QTemporaryDir m_appImageDir;

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_desktopintegration");
        QStandardPaths::setTestModeEnabled(true);   // redirect the data home to a throwaway tree
        qunsetenv("APPIMAGE");                      // a real AppImage session must not skew the tests
        QVERIFY(m_appImageDir.isValid());
    }

    void init()
    {
        QFile::remove(DesktopIntegration::desktopFilePath());
        QFile::remove(DesktopIntegration::iconFilePath());
    }

    void cleanup()
    {
        qunsetenv("APPIMAGE");
    }

    void cleanupTestCase()
    {
        QFile::remove(DesktopIntegration::desktopFilePath());
        QFile::remove(DesktopIntegration::iconFilePath());
    }

    void templateContents_carryTheAuthorizationKey()
    {
        const QString entry = DesktopIntegration::desktopEntryContents("/opt/snim/snim");
        const QStringList lines = entry.split('\n');
        QVERIFY(lines.contains("[Desktop Entry]"));
        QVERIFY(lines.contains("Type=Application"));
        QVERIFY(lines.contains("Name=Snim"));
        QVERIFY(lines.contains("Icon=dev.snim.Snim"));
        QVERIFY(lines.contains("Categories=Utility;Qt;"));
        QVERIFY(lines.contains("StartupWMClass=snim"));
        QVERIFY(lines.contains("Exec=/opt/snim/snim"));
        QVERIFY(lines.contains("X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2"));
    }

    void paths_liveUnderTheDataHome()
    {
        const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QVERIFY(!dataHome.isEmpty());
        QCOMPARE(DesktopIntegration::desktopFilePath(),
                 dataHome + "/applications/dev.snim.Snim.desktop");
        QCOMPARE(DesktopIntegration::iconFilePath(),
                 dataHome + "/icons/hicolor/scalable/apps/dev.snim.Snim.svg");
    }

    void status_notInstalled_onEmptyDataHome()
    {
#ifndef Q_OS_LINUX
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::NotApplicable);
        return;
#else
        QVERIFY(!QFile::exists(DesktopIntegration::desktopFilePath()));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::NotInstalled);
#endif
    }

    void install_thenStatusIsInstalled()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        QString error = "unset";
        QVERIFY2(DesktopIntegration::install(&error), qPrintable(error));
        QVERIFY(error.isEmpty());
        QVERIFY(QFile::exists(DesktopIntegration::desktopFilePath()));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);

        // The written Exec is the running binary, canonicalized.
        const QString expected = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
        QVERIFY(readEntry().contains("Exec=" + expected + "\n"));
#endif
    }

    void install_writesTheIcon()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        QVERIFY(DesktopIntegration::install(nullptr));
        const QFileInfo icon(DesktopIntegration::iconFilePath());
        QVERIFY(icon.exists());
        QVERIFY(icon.size() > 0);
        QVERIFY(icon.isWritable());   // a read-only resource copy would block the next install
#endif
    }

    void install_isRepeatable()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        QVERIFY(DesktopIntegration::install(nullptr));
        QVERIFY(DesktopIntegration::install(nullptr));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);
#endif
    }

    void missingAuthorizationKey_isDetected()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        QVERIFY(DesktopIntegration::install(nullptr));

        QStringList kept;
        const QStringList lines = readEntry().split('\n');
        for (const QString &line : lines) {
            if (!line.startsWith("X-KDE-DBUS-Restricted-Interfaces"))
                kept.append(line);
        }
        QVERIFY(writeEntry(kept.join('\n')));

        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::MissingAuthorizationKey);
#endif
    }

    void execMismatch_isDetected()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        QVERIFY(writeEntry(DesktopIntegration::desktopEntryContents("/usr/bin/some-other-binary")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::ExecMismatch);

        // A bare command name (the shipped template's Exec=snim) is a mismatch too.
        QVERIFY(writeEntry(DesktopIntegration::desktopEntryContents("snim")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::ExecMismatch);
#endif
    }

    void execWithFieldCodes_stillMatches()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        const QString appPath = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
        QVERIFY(writeEntry(DesktopIntegration::desktopEntryContents(appPath + " %U")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);
#endif
    }

    void appImage_execIsTheOuterPath()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        const QString appImage = makeFakeAppImage("Snim-x86_64.AppImage");
        QVERIFY(!appImage.isEmpty());
        qputenv("APPIMAGE", appImage.toUtf8());

        QVERIFY(DesktopIntegration::install(nullptr));
        QVERIFY(readEntry().contains("Exec=" + appImage + "\n"));
        // The per-launch mount path must not leak into the entry.
        QVERIFY(!readEntry().contains(QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath()));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);
#endif
    }

    void appImage_execWithSpacesIsQuoted()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        const QString appImage = makeFakeAppImage("Snim Nightly x86_64.AppImage");
        QVERIFY(!appImage.isEmpty());
        QVERIFY(appImage.contains(' '));
        qputenv("APPIMAGE", appImage.toUtf8());

        QVERIFY(DesktopIntegration::install(nullptr));
        QVERIFY(readEntry().contains("Exec=\"" + appImage + "\"\n"));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);
#endif
    }

    void appImage_missingPathFallsBackToTheAppPath()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        const QString gone = m_appImageDir.filePath("never-extracted.AppImage");
        QVERIFY(!QFile::exists(gone));
        qputenv("APPIMAGE", gone.toUtf8());

        QVERIFY(DesktopIntegration::install(nullptr));
        const QString expected = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
        QVERIFY(readEntry().contains("Exec=" + expected + "\n"));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);
#endif
    }

    void appImage_statusMatchesAnEntryWrittenByAnEarlierRun()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        const QString appImage = makeFakeAppImage("Snim-stable.AppImage");
        QVERIFY(!appImage.isEmpty());
        qputenv("APPIMAGE", appImage.toUtf8());

        // What a previous AppImage launch left behind; the mount path it ran from is long gone.
        QVERIFY(writeEntry(DesktopIntegration::desktopEntryContents(appImage + " %U")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);

        // The stale mount path is what the old code wrote, and it must read as a mismatch.
        QVERIFY(writeEntry(DesktopIntegration::desktopEntryContents("/tmp/.mount_SnimAbc123/usr/bin/snim")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::ExecMismatch);
#endif
    }
};

QTEST_MAIN(tst_DesktopIntegration)
#include "tst_desktopintegration.moc"
