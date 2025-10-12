#include "src/core/ScreenshotApp.h"
#include <QLoggingCategory>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[]) {

    Core::ScreenshotApp app(argc, argv);

    // Set application properties
    Core::ScreenshotApp::setApplicationName("Screenshot App");
    Core::ScreenshotApp::setApplicationVersion("1.0");
    Core::ScreenshotApp::setOrganizationName("Screenshot Tools");

    // Test debug output
    qDebug() << "Application starting...";
    qDebug() << "Debug output is working!";

    return app.exec();
}