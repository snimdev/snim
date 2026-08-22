#include "editor/EditorChrome.h"

#include "core/IconUtil.h"
#include "upload/UploadConfig.h"
#include "upload/UploadProfiles.h"

#include <QApplication>
#include <QFile>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QToolBar>
#include <QToolButton>

namespace Editor {

QToolBar *addEditorToolBar(QMainWindow *window, const QString &title)
{
    QFile styleFile(QStringLiteral(":/styles/styles/editor.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        window->setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    QToolBar *toolbar = window->addToolBar(title);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    return toolbar;
}

QIcon themedIcon(const QString &svgPath, const QToolBar *toolbar)
{
    const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const int size = toolbar ? toolbar->iconSize().width() : 20;
    return Core::themedSvgIcon(svgPath, QColor(dark ? "#d0d0d0" : "#333333"), size);
}

QMenu *addSplitButton(QToolBar *toolbar, QAction *defaultAction)
{
    auto *button = new QToolButton(toolbar);
    button->setDefaultAction(defaultAction);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    auto *menu = new QMenu(button);
    button->setMenu(menu);
    toolbar->addWidget(button);
    return menu;
}

void fillWithUploadProfiles(QMenu *menu, std::function<void(const QString &)> onPick)
{
    // Rebuilt on open, so profiles added or re-defaulted while an editor is open show up.
    QObject::connect(menu, &QMenu::aboutToShow, menu, [menu, onPick] {
        menu->clear();
        QObject::connect(menu->addAction(QObject::tr("Upload to default")), &QAction::triggered,
                         menu, [onPick] { onPick(QString()); });
        const QString defId = Upload::UploadProfiles::defaultId();
        const QVector<Upload::UploadProfile> profiles = Upload::UploadProfiles::all();
        if (!profiles.isEmpty())
            menu->addSeparator();
        for (const Upload::UploadProfile &p : profiles) {
            QObject::connect(menu->addAction(Upload::profileLabel(p, p.id == defId)),
                             &QAction::triggered, menu, [onPick, id = p.id] { onPick(id); });
        }
    });
}

bool uploadConfigured(QWidget *parent, const QString &profileId)
{
    // The CHOSEN destination (empty id = default), not just the default.
    if (Upload::UploadConfig::forProfile(profileId).isComplete())
        return true;
    QMessageBox::information(parent, QObject::tr("Upload not configured"),
                             QObject::tr("Set up an upload destination in Settings → Upload first."));
    return false;
}

} // namespace Editor
