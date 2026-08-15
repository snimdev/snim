#include "screen/SelectorGroup.h"

#include "screen/LayerShellSupport.h"
#include "screen/OverlayWindows.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

namespace Screen {

SelectorGroup::SelectorGroup(const QPixmap &frame, const QRect &virtualGeometry,
                             const Options &options, QObject *parent)
    : QObject(parent)
{
    // Wayland needs fullscreen; elsewhere a plain window skips the macOS Spaces animation.
    const bool fullScreen = !options.layerSurface
                            && QGuiApplication::platformName() == QLatin1String("wayland");

    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect geometry = screen->geometry();
        auto *selector = new AreaSelector();
        selector->setScreenshot(frame);
        selector->setVirtualGeometry(virtualGeometry);
        selector->setScreenOffset(geometry.topLeft());
        selector->setMode(options.mode);
        if (options.mode == AreaSelector::Mode::WindowPick)
            selector->setWindowInfos(options.windows);
        selector->setActionsEnabled(options.actions);

        selector->setGeometry(geometry);
        if (fullScreen)
            selector->setWindowState(Qt::WindowFullScreen);
        selector->winId();   // the native window must exist before it is placed
        if (QWindow *window = selector->windowHandle()) {
            window->setScreen(screen);
            // Else the compositor shrinks the overlay to the work area, leaving the panels out.
            if (options.layerSurface)
                attachOverlayLayerSurface(window, OverlayAnchorAll, /*exclusiveZone=*/-1,
                                          OverlayKeyboard::Exclusive);
        }
        if (fullScreen) {
            selector->showFullScreen();
        } else {
            selector->show();
            selector->raise();
            selector->activateWindow();
            configureOverlayWindow(selector);   // after show(), which resets the window level
        }
        m_selectors.append(selector);
    }

    for (AreaSelector *selector : std::as_const(m_selectors)) {
        connect(selector, &AreaSelector::areaSelected, this, [this](const QRect &area) {
            close();
            emit areaSelected(area);
        });
        connect(selector, &AreaSelector::copyRequested, this, [this](const QRect &area) {
            close();
            emit copyRequested(area);
        });
        connect(selector, &AreaSelector::saveRequested, this, [this](const QRect &area) {
            close();
            emit saveRequested(area);
        });
        // Closing first also silences the areaSelected that follows a pick.
        connect(selector, &AreaSelector::windowPicked, this, [this](const QRect &area, quint64 id) {
            close();
            emit windowPicked(area, id);
        });
        connect(selector, &AreaSelector::liveStateChanged, this,
                [this, selector](const QRect &selection, int phase, int mode, const QPoint &cursor) {
            for (AreaSelector *other : std::as_const(m_selectors)) {
                if (other != selector)
                    other->applyPeerState(selection, phase, mode, cursor);
            }
            emit liveStateChanged(selector, selection, phase);
        });
    }
}

SelectorGroup::~SelectorGroup()
{
    tearDown();
}

void SelectorGroup::close()
{
    tearDown();
    deleteLater();
}

void SelectorGroup::tearDown()
{
    for (AreaSelector *selector : std::as_const(m_selectors)) {
        selector->blockSignals(true);   // the other overlays must not answer as well
        selector->disconnect();
        selector->close();
        selector->deleteLater();
    }
    m_selectors.clear();
}

} // namespace Screen
