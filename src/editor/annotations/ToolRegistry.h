#ifndef IMAGEEDITOR_TOOLREGISTRY_H
#define IMAGEEDITOR_TOOLREGISTRY_H

#include <QString>
#include <QList>
#include <functional>
#include "Layer.h"

class QObject;

namespace Editor {

namespace Interactions { class IDrawingInteraction; }
namespace Tools { class ITool; }

/**
 * Declarative description of one editor tool. Bundles the objects a tool needs: its
 * drawing interaction and its property "template" (freshly-drawn items are styled
 * from it), plus the metadata the toolbar and layer-naming use. Adding a tool is one
 * ToolSpec entry.
 */
struct ToolSpec {
    QString id;                       // stable key: "pointer", "arrow", ...
    Layer::LayerType layerType;       // Layer kind a committed item becomes
    QString displayName;              // Properties-panel title, e.g. "Arrow Tool"
    QString namePrefix;               // Layer name prefix, e.g. "Arrow"
    QString iconPath;                 // ":/icons/icons/arrow.svg"
    QString tooltip;                  // toolbar tooltip, without the key
    QChar shortcut;                   // single-key shortcut, upper case

    bool isDrawingTool = true;        // false for the pointer (no template/commit)
    bool switchToPointerAfter = false;// true for Text (one-shot placement)

    // Build this tool's interaction (owned by the QObject parent).
    std::function<Interactions::IDrawingInteraction*(QObject *parent)> makeInteraction;

    // Build the property "template" tool (null for the pointer).
    std::function<Tools::ITool*()> makeTemplate;

    // Optional: push the template's current style onto the live interaction so the
    // in-progress preview matches what a committed item will look like (used by
    // freehand/highlight/blur). Null when the interaction has no style of its own.
    std::function<void(Interactions::IDrawingInteraction *strategy,
                       Tools::ITool *templateTool)> syncStrategy;
};

/** The built-in tool set, in toolbar order (pointer first). */
class ToolRegistry {
public:
    static const QList<ToolSpec>& tools();
    static const ToolSpec* find(const QString &id);
    // Case-insensitive; null when no tool uses the key.
    static const ToolSpec* findByShortcut(QChar key);
};

} // namespace Editor

#endif // IMAGEEDITOR_TOOLREGISTRY_H
