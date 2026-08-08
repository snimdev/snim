#ifndef UPLOAD_UPLOADMENU_H
#define UPLOAD_UPLOADMENU_H

#include "upload/UploadProfiles.h"

#include <QAction>
#include <QMenu>
#include <QObject>
#include <functional>

namespace Upload {

/**
 * (Re)populate `menu` from the saved upload profiles: a first "Upload to default" item
 * (empty id) then one item per profile (the default one marked with a star). `onPick`
 * is invoked with the chosen profile id - empty means "the default". Call this from the
 * menu's aboutToShow so it reflects profile/default changes made while an editor is open.
 */
inline void rebuildUploadMenu(QMenu *menu, const std::function<void(const QString &)> &onPick)
{
    menu->clear();
    const QString defId = UploadProfiles::defaultId();

    QAction *def = menu->addAction(QObject::tr("Upload to default"));
    QObject::connect(def, &QAction::triggered, menu, [onPick] { onPick(QString()); });

    const QVector<UploadProfile> profiles = UploadProfiles::all();
    if (!profiles.isEmpty())
        menu->addSeparator();
    for (const UploadProfile &p : profiles) {
        QAction *a = menu->addAction(profileLabel(p, p.id == defId));
        const QString id = p.id;
        QObject::connect(a, &QAction::triggered, menu, [onPick, id] { onPick(id); });
    }
}

} // namespace Upload

#endif // UPLOAD_UPLOADMENU_H
