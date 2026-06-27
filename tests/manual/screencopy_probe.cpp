// Manual check against a live wlroots-family compositor (not run by ctest): captures
// every output with the native screencopy client, stitches them the way the app does
// and writes the PNG. Usage: snim_screencopy_probe [auto|ext|wlr] <out.png>
#include "screen/ScreencopyClient.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QTextStream>

using namespace Screen::Screencopy;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);

    const QStringList args = app.arguments().mid(1);
    if (args.size() != 2) {
        out << "usage: snim_screencopy_probe [auto|ext|wlr] <out.png>\n";
        return 2;
    }
    const Protocol forced = args[0] == QLatin1String("ext") ? Protocol::ExtImageCopyCapture
                            : args[0] == QLatin1String("wlr") ? Protocol::WlrScreencopy
                                                              : Protocol::None;

    const Globals globals = advertisedGlobals();
    out << "platform " << QGuiApplication::platformName() << ", ext-image-copy-capture "
        << globals.ext << ", wlr-screencopy " << globals.wlr << "\n";

    const Protocol protocol = pickProtocol(globals.ext, globals.wlr, forced);
    out << "protocol " << (protocol == Protocol::ExtImageCopyCapture ? "ext-image-copy-capture"
                           : protocol == Protocol::WlrScreencopy   ? "wlr-screencopy"
                                                                   : "none") << "\n";

    QString error;
    const QList<OutputFrame> frames = captureOutputs(protocol, &error);
    if (frames.isEmpty()) {
        out << "capture failed: " << error << "\n";
        return 1;
    }

    QList<ScreenSlot> screens;
    QRect virtualGeometry;
    for (QScreen *screen : QGuiApplication::screens()) {
        screens.append({screen->name(), screen->geometry()});
        virtualGeometry = virtualGeometry.united(screen->geometry());
        out << "screen " << screen->name() << " geometry " << screen->geometry().x() << ","
            << screen->geometry().y() << " " << screen->geometry().width() << "x"
            << screen->geometry().height() << " dpr " << screen->devicePixelRatio() << "\n";
    }

    const QFileInfo target(args[1]);
    for (const OutputFrame &frame : frames) {
        out << "output " << frame.name << " at " << frame.position.x() << "," << frame.position.y()
            << " frame " << frame.image.width() << "x" << frame.image.height() << "\n";
        frame.image.save(target.dir().filePath(target.completeBaseName() + QLatin1Char('-')
                                               + frame.name + QLatin1String(".png")));
    }

    const QImage stitched = stitchFrames(frames, screens, virtualGeometry);
    if (stitched.isNull()) {
        out << "stitch failed: no output matched a screen\n";
        return 1;
    }
    out << "stitched " << stitched.width() << "x" << stitched.height() << " dpr "
        << stitched.devicePixelRatio() << "\n";
    return stitched.save(args[1]) ? 0 : 1;
}
