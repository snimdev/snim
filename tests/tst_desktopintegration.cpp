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
// which is exactly what install() writes and status() then has to match. XDG_DATA_DIRS
// points at a scratch tree that stands in for a package's /usr/share.
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

    QString systemEntryPath() const
    {
        return m_systemDataDir.filePath("applications/dev.snim.Snim.desktop");
    }

    bool writeSystemEntry(const QString &contents) const
    {
        if (!QDir().mkpath(m_systemDataDir.filePath("applications")))
            return false;
        QFile file(systemEntryPath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return false;
        return file.write(contents.toUtf8()) == contents.toUtf8().size();
    }

    QTemporaryDir m_systemDataDir;

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_desktopintegration");
        QStandardPaths::setTestModeEnabled(true);   // redirect the data home to a throwaway tree
        QVERIFY(m_systemDataDir.isValid());
        qputenv("XDG_DATA_DIRS", QFile::encodeName(m_systemDataDir.path()));
        qunsetenv("FLATPAK_ID");   // a test run inside a Flatpak must not skew the results
    }

    void init()
    {
        QFile::remove(DesktopIntegration::desktopFilePath());
        QFile::remove(DesktopIntegration::iconFilePath());
        QFile::remove(systemEntryPath());
    }

    void cleanup()
    {
        qunsetenv("FLATPAK_ID");
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
        QVERIFY(!entry.contains('@'));   // every placeholder of the shared template is filled
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

        // A bare command name (what the AppDir and Flatpak entries carry) is a mismatch too.
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

    void packagedEntry_forThisExecutable_isInstalled()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        const QString appPath = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
        QVERIFY(writeSystemEntry(DesktopIntegration::desktopEntryContents(appPath)));
        QVERIFY(!QFile::exists(DesktopIntegration::desktopFilePath()));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);
#endif
    }

    void packagedEntry_forAnotherExecutable_isNotAccepted()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        QVERIFY(writeSystemEntry(DesktopIntegration::desktopEntryContents("/opt/snim/usr/bin/snim")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::ExecMismatch);
#endif
    }

    void userEntry_shadowsThePackagedOne()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        // KWin sees only the user's entry, so a stale one breaks a correct package.
        const QString appPath = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
        QVERIFY(writeSystemEntry(DesktopIntegration::desktopEntryContents(appPath)));
        QVERIFY(writeEntry(DesktopIntegration::desktopEntryContents("/usr/bin/some-other-binary")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::ExecMismatch);

        // Writing the user's entry repairs it.
        QVERIFY(DesktopIntegration::install(nullptr));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::Installed);
#endif
    }

    void flatpak_isNotApplicable()
    {
#ifndef Q_OS_LINUX
        QSKIP("Desktop integration is Linux only");
#else
        qputenv("FLATPAK_ID", "dev.snim.Snim");
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::NotApplicable);

        // Even a broken user entry is left alone: the sandbox exports its own.
        QVERIFY(writeEntry(DesktopIntegration::desktopEntryContents("/usr/bin/some-other-binary")));
        QCOMPARE(DesktopIntegration::status(), DesktopIntegration::Status::NotApplicable);

        QString error;
        QVERIFY(!DesktopIntegration::install(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(readEntry().contains("Exec=/usr/bin/some-other-binary\n"));
#endif
    }
};

QTEST_MAIN(tst_DesktopIntegration)
#include "tst_desktopintegration.moc"
