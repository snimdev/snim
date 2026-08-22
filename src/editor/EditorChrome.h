#ifndef EDITOR_EDITORCHROME_H
#define EDITOR_EDITORCHROME_H

#include <QIcon>
#include <QString>
#include <functional>

class QAction;
class QMainWindow;
class QMenu;
class QToolBar;
class QWidget;

// The window chrome both editors share, so the two windows read as one app.
namespace Editor {

// Applies the shared stylesheet to window and adds its icon-only, fixed toolbar.
QToolBar *addEditorToolBar(QMainWindow *window, const QString &title);

// The SVG in the theme's tone, at the toolbar's icon size so the glyph fills its slot.
QIcon themedIcon(const QString &svgPath, const QToolBar *toolbar);

// A button that runs defaultAction on click and opens the returned menu from its arrow.
QMenu *addSplitButton(QToolBar *toolbar, QAction *defaultAction);

// Lists the upload destinations each time menu opens; onPick gets the id, "" for the default.
void fillWithUploadProfiles(QMenu *menu, std::function<void(const QString &)> onPick);

// False, after telling the user, when the chosen destination is not set up.
bool uploadConfigured(QWidget *parent, const QString &profileId);

} // namespace Editor

#endif // EDITOR_EDITORCHROME_H
