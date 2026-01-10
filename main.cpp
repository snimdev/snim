#include "src/core/ScreenshotApp.h"
#include <QLoggingCategory>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[]) {

    QLoggingCategory::setFilterRules(
        "qt.*.debug=false\n"
        "Niceshot.debug=true\n"
        "default.debug=true"  // For qDebug() without category
    );
    // Static setters, set before construction: ScreenshotApp's constructor already
    // reads QSettings, which resolves its scope from these names.
    Core::ScreenshotApp::setApplicationName("Screenshot App");
    Core::ScreenshotApp::setApplicationVersion("1.0");
    Core::ScreenshotApp::setOrganizationName("Screenshot Tools");

    Core::ScreenshotApp app(argc, argv);

    // Test debug output
    qDebug() << "Application starting...";
    qDebug() << "Debug output is working!";

    return app.exec();
}