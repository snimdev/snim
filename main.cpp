#include "screenshotapp.h"

int main(int argc, char *argv[]) {
    ScreenshotApp app(argc, argv);

    // Set application properties
    app.setApplicationName("Screenshot App");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Screenshot Tools");

    return app.exec();
}