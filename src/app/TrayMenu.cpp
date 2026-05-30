#include "app/TrayMenu.h"
#include "app/TextSnipWorkflow.h"
#include "core/IconUtil.h"
#include "core/Sandbox.h"
#include "hotkeys/HotkeyBindings.h"
#ifdef Q_OS_MACOS
#include "core/MacTrayWorkaround.h"
#endif
#ifdef SNIM_HAVE_WIN_RECORDER
#include "recording/strategies/windows/WindowsRecordingStrategy.h"
#endif

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QMenu>
#include <QPainter>
#include <QSvgRenderer>

namespace App {

    TrayMenu::TrayMenu(QObject *parent)
        : QObject(parent) {
        m_captureAreaAction = new QAction("Capture Area", this);
        m_captureWindowAction = new QAction("Capture Window", this);
        m_captureFullScreenAction = new QAction("Capture Full Screen", this);
        m_textSnipAction = new QAction("Extract Text (OCR)", this);

        // Disable text snip if OCR is not available
        if (!TextSnipWorkflow::isOCRAvailable()) {
            m_textSnipAction->setEnabled(false);
            m_textSnipAction->setText("Extract Text (OCR not available)");
            m_textSnipAction->setToolTip("Install tesseract-ocr to enable this feature");
        }

        m_recordAreaAction = new QAction("Record Area", this);
        m_recordWindowAction = new QAction("Record Window", this);

        m_aboutAction = new QAction("About", this);
        m_checkUpdatesAction = new QAction("Check for updates...", this);
        // Flatpak updates through `flatpak update`, so a GitHub release link would only mislead.
        m_checkUpdatesAction->setVisible(!Core::Sandbox::isFlatpak());
        m_settingsAction = new QAction("Settings", this);

#ifdef Q_OS_LINUX
        m_desktopIntegrationAction = new QAction("Set up desktop integration...", this);
        m_desktopIntegrationAction->setToolTip(
            "Register Snim's desktop entry so KDE allows instant, dialog-free captures");
#endif

        m_quitAction = new QAction("Quit", this);

        m_menu = new QMenu();
#ifdef Q_OS_WIN
        // The Windows tray menu is a plain QMenu, which hides tooltips such as the recording reason.
        m_menu->setToolTipsVisible(true);
#endif
        m_menu->addAction(m_captureAreaAction);
        m_menu->addAction(m_captureWindowAction);
        m_menu->addAction(m_captureFullScreenAction);
        m_menu->addAction(m_recordAreaAction);
        m_menu->addAction(m_recordWindowAction);
        m_menu->addAction(m_textSnipAction);
        m_menu->addSeparator();
        m_menu->addAction(m_settingsAction);
#ifdef Q_OS_LINUX
        m_menu->addAction(m_desktopIntegrationAction);
#endif
        m_menu->addAction(m_checkUpdatesAction);
        m_menu->addAction(m_aboutAction);
        m_menu->addSeparator();
        m_menu->addAction(m_quitAction);
    }

    TrayMenu::~TrayMenu() = default;

    void TrayMenu::show() {
#ifdef Q_OS_MACOS
        // Before the status item exists: Qt's menu-tracking callback aborts on macOS 27.
        Core::applyTrayMenuTrackingWorkaround();
#endif
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setContextMenu(m_menu);
        m_trayIcon->setIcon(createThemedTrayIcon(":/icons/icons/tray-icon.svg"));
        m_trayIcon->setToolTip("Snim - Screenshot App");
        m_trayIcon->show();

        connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                emit doubleClicked();
            }
#ifdef Q_OS_WIN
            // Windows opens the context menu on right click only; left click is what users try first.
            if (reason == QSystemTrayIcon::Trigger)
                m_menu->popup(QCursor::pos());
#endif
            // Elsewhere a single click (Trigger) shows the context menu automatically
        });
    }

    void TrayMenu::hide() {
        if (m_trayIcon)
            m_trayIcon->hide();
    }

    void TrayMenu::notify(const QString &title, const QString &text,
                          const QSystemTrayIcon::MessageIcon icon, const int msecs) {
        if (m_trayIcon)
            m_trayIcon->showMessage(title, text, icon, msecs);
    }

    QIcon TrayMenu::createThemedTrayIcon(const QString &iconPath) {
#ifdef Q_OS_MACOS
        // The macOS menu bar is translucent and its shade varies by display, wallpaper,
        // and appearance, so a fixed icon colour can't track it (hence "black on the
        // built-in screen, white on an external one"). Render the glyph opaque and mark
        // the QIcon as a template/mask: macOS then tints it to match the menu bar and
        // highlights it when the menu is open, like a native status item. Only the alpha
        // matters for a mask, so the colour is moot.
        QIcon icon = Core::themedSvgIcon(iconPath, QColor(Qt::black), 22);
        icon.setIsMask(true);
        return icon;
#else
        // A panel's shade and slot size are unknowable here (Mint: dark panel, light windows),
        // so use the colour app icon at every common size and let the panel pick its fit.
        Q_UNUSED(iconPath)
        QIcon icon;
        QSvgRenderer renderer(QStringLiteral(":/icons/icons/app-icon.svg"));
        for (const int size : {16, 20, 22, 24, 32, 40, 48, 64}) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);
            renderer.render(&painter);
            painter.end();
            icon.addPixmap(pixmap);
        }
        return icon;
#endif
    }

    void TrayMenu::setRecordingUnavailable() {
#if defined(Q_OS_MACOS)
        const QString reason = "Screen recording requires macOS 12.3 or later";
#elif defined(Q_OS_LINUX)
        const QString reason = "Screen recording requires the ScreenCast portal and "
                               "GStreamer (with an H.264 encoder)";
#elif defined(SNIM_HAVE_WIN_RECORDER)
        QString reason = Recording::WindowsRecordingStrategy::unavailableReason();
        if (reason.isEmpty())
            reason = "Screen recording could not start its Windows recorder";
#else
        const QString reason = "Screen recording is not supported on this platform yet";
#endif
        m_recordAreaAction->setEnabled(false);
        m_recordAreaAction->setText("Record Area (unavailable)");
        m_recordAreaAction->setToolTip(reason);
        m_recordWindowAction->setEnabled(false);
        m_recordWindowAction->setToolTip(reason);
    }

    void TrayMenu::setRecordingActive(const bool recording) {
        m_recordAreaAction->setText(recording ? "Stop Recording" : "Record Area");
        m_recordWindowAction->setEnabled(!recording);   // one session at a time
    }

    QAction *TrayMenu::actionFor(const Hotkeys::HotkeyAction action) const {
        switch (action) {
        case Hotkeys::HotkeyAction::CaptureArea:       return m_captureAreaAction;
        case Hotkeys::HotkeyAction::CaptureWindow:     return m_captureWindowAction;
        case Hotkeys::HotkeyAction::CaptureFullScreen: return m_captureFullScreenAction;
        case Hotkeys::HotkeyAction::OcrTextSnip:       return m_textSnipAction;
        case Hotkeys::HotkeyAction::RecordArea:        return m_recordAreaAction;
        case Hotkeys::HotkeyAction::RecordWindow:      return m_recordWindowAction;
        }
        return nullptr;
    }

    void TrayMenu::refreshShortcutHints() {
        for (const Hotkeys::HotkeyAction action : Hotkeys::allHotkeyActions()) {
            if (QAction *target = actionFor(action))
                target->setShortcut(Hotkeys::HotkeyBindings::sequence(action));
        }
    }

} // namespace App
