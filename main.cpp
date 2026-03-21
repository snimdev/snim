#include "src/core/ScreenshotApp.h"
#include "core/AppScope.h"
#include "core/Version.h"
#include <QLoggingCategory>
#include <QSettings>
#include <QIcon>
#include <QDebug>
#include <iostream>
#include <string_view>

#ifdef Q_OS_WIN
#include <cstdio>
#include <windows.h>
#endif

// Carry settings over from the pre-rebrand scope, once, if Snim has none yet.
static void migrateLegacySettings()
{
    QSettings current;
    if (!current.allKeys().isEmpty())
        return;

    QSettings legacy(QStringLiteral("Screenshot Tools"), QStringLiteral("Screenshot App"));
    const QStringList keys = legacy.allKeys();
    if (keys.isEmpty())
        return;

    for (const QString &key : keys)
        current.setValue(key, legacy.value(key));
    current.sync();
}

#ifdef Q_OS_WIN
// A GUI-subsystem exe starts without stdout; borrow the launching console, if any.
static void attachParentConsole()
{
    // Already redirected to a file or pipe: that handle works as is.
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_UNKNOWN)
        return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE *stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    std::cout.clear();
    std::cerr.clear();
}
#else
static void attachParentConsole() {}
#endif

int main(int argc, char *argv[]) {

    // Scanned before the app object exists: ScreenshotApp is a QApplication and raises a
    // tray icon in its constructor, which no version query should do.
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--version" || arg == "-v") {
            attachParentConsole();
            std::cout << "Snim " << Core::Version::kVersion << '\n';
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            attachParentConsole();
            std::cout << "Usage: snim [--version] [--help]\n"
                         "  --version   Print the version and exit.\n"
                         "Snim runs in the system tray; everything else is configured from "
                         "its tray menu.\n";
            return 0;
        }
    }

    QLoggingCategory::setFilterRules(
        "qt.*.debug=false\n"
        "Snim.debug=true\n"
        "Snim.perf.debug=true\n"
        "default.debug=true"  // For qDebug() without category
    );
    // Static setters, set before construction: ScreenshotApp's constructor already
    // reads QSettings, which resolves its scope from these names.
    Core::ScreenshotApp::setApplicationName("Snim");
    Core::ScreenshotApp::setApplicationVersion(QString::fromLatin1(Core::Version::kVersion));
    Core::ScreenshotApp::setOrganizationName("darkog");
    // Wayland matches windows to the installed desktop entry by this name.
    Core::ScreenshotApp::setDesktopFileName(QStringLiteral("dev.snim.Snim"));

    // Must run before the app object exists: its ctor already reads settings.
    migrateLegacySettings();

    Core::ScreenshotApp app(argc, argv);

#ifdef Q_OS_LINUX
    // Earliest point that works: the session bus wants a QCoreApplication, and the portals
    // read our app id off the cgroup on their first request, which cannot arrive before exec().
    if (!Core::AppScope::inAppScope()) {
        QString scopeError;
        if (Core::AppScope::adoptAppScope(&scopeError))
            qInfo() << "[AppScope] adopted app scope for portal identity";
        else
            qInfo() << "[AppScope] running without app scope:" << scopeError;
    }
#endif

    app.setWindowIcon(QIcon(QStringLiteral(":/icons/icons/app-icon.svg")));

    // Test debug output
    qDebug() << "Application starting...";
    qDebug() << "Debug output is working!";

    return app.exec();
}