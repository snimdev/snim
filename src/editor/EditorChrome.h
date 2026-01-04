#ifndef EDITOR_EDITORCHROME_H
#define EDITOR_EDITORCHROME_H

#include <QFile>
#include <QWidget>

namespace Editor {

/**
 * Apply the shared editor stylesheet (toolbar/button chrome, split-button rules, panel
 * styling) to an editor window. Both the image and video editors call this so their
 * shells stay visually identical; the sheet lives in resources/styles/editor.qss.
 */
inline void applyEditorStyleSheet(QWidget *window)
{
    QFile styleFile(QStringLiteral(":/styles/styles/editor.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        window->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
}

} // namespace Editor

#endif // EDITOR_EDITORCHROME_H
