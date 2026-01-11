#include "src/core/ScreenshotApp.h"
#include <QLoggingCategory>
#include <QSettings>
#include <QDebug>
#include <iostream>

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

int main(int argc, char *argv[]) {

    QLoggingCategory::setFilterRules(
        "qt.*.debug=false\n"
        "Snim.debug=true\n"
        "Snim.perf.debug=true\n"
        "default.debug=true"  // For qDebug() without category
    );
    // Static setters, set before construction: ScreenshotApp's constructor already
    // reads QSettings, which resolves its scope from these names.
    Core::ScreenshotApp::setApplicationName("Snim");
    Core::ScreenshotApp::setApplicationVersion("1.0");
    Core::ScreenshotApp::setOrganizationName("darkog");

    // Must run before the app object exists: its ctor already reads settings.
    migrateLegacySettings();

    Core::ScreenshotApp app(argc, argv);

    // Test debug output
    qDebug() << "Application starting...";
    qDebug() << "Debug output is working!";

    return app.exec();
}