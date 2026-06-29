#include "app/Application.h"
#include "core/AppScope.h"
#include "core/Sandbox.h"
#include "core/SelfTest.h"
#include "core/Settings.h"
#include "core/Version.h"
#include <QApplication>
#include <QLoggingCategory>
#include <QIcon>
#include <QDebug>
#include <iostream>
#include <string_view>

#ifdef Q_OS_WIN
#include "core/SingleInstance.h"
#include <QFileInfo>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <windows.h>
#include <shobjidl.h>
#endif

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

static QString exeDirectory()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    return QFileInfo(QString::fromWCharArray(path.data(), int(length))).absolutePath();
}
#else
static void attachParentConsole() {}
#endif

int main(int argc, char *argv[]) {

    // Scanned before the app object exists: Application is a QApplication and raises a
    // tray icon in its constructor, which no version query should do.
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--version" || arg == "-v") {
            attachParentConsole();
            std::cout << "Snim " << Core::Version::kVersion << '\n';
            return 0;
        }
        if (arg == "--self-test") {
            attachParentConsole();
#ifdef Q_OS_WIN
            // A broken bundle must fail the run, not leave a crash dialog up on a CI runner.
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
            QApplication app(argc, argv);
            return Core::SelfTest::run(std::cout, App::Application::selfTestChecks());
        }
        if (arg == "--help" || arg == "-h") {
            attachParentConsole();
            std::cout << "Usage: snim [--version] [--self-test] [--help]\n"
                         "  --version   Print the version and exit.\n"
                         "  --self-test Check the bundled plugins and libraries, then exit.\n"
                         "Snim runs in the system tray; everything else is configured from "
                         "its tray menu.\n";
            return 0;
        }
    }

#ifdef Q_OS_WIN
    // After the flags above, which must still work while Snim runs, and before any settings.
    if (!Core::SingleInstance::claim()) {
        QCoreApplication pinger(argc, argv);
        Core::SingleInstance::notifyRunningInstance();
        return 0;
    }
#endif

    QLoggingCategory::setFilterRules(
        "qt.*.debug=false\n"
        "Snim.debug=true\n"
        "Snim.perf.debug=true\n"
        "default.debug=true"  // For qDebug() without category
    );
    // Static setters, set before construction: Application's constructor already
    // reads QSettings, which resolves its scope from these names.
    App::Application::setApplicationName("Snim");
    App::Application::setApplicationVersion(QString::fromLatin1(Core::Version::kVersion));
    App::Application::setOrganizationName("darkog");
    // Wayland matches windows to the installed desktop entry by this name.
    App::Application::setDesktopFileName(QStringLiteral("dev.snim.Snim"));
#ifdef Q_OS_WIN
    // Before any window or tray icon exists, so the taskbar and toasts group under it.
    SetCurrentProcessExplicitAppUserModelID(L"dev.snim.Snim");
#endif

#ifdef Q_OS_WIN
    // The portable zip ships an empty snim.ini beside snim.exe: then the registry stays untouched.
    Core::Settings::setPortableFile(Core::Settings::portableFileIn(exeDirectory()));
#endif

    App::Application app(argc, argv);

#ifdef Q_OS_LINUX
    // Earliest point that works: the session bus wants a QCoreApplication, and the portals
    // read our app id off the cgroup on their first request, which cannot arrive before exec().
    // Flatpak already gives the portals our app id, and its scope must stay as it is.
    if (!Core::Sandbox::isFlatpak() && !Core::AppScope::inAppScope()) {
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