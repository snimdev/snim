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

// Declarative description of one editor tool: its drawing interaction and its style
// template (previews and freshly drawn items are styled from it), plus the metadata
// the toolbar and layer naming use. Adding a tool is one ToolSpec entry.
struct ToolSpec {
    QString id;                       // stable key: "pointer", "arrow", ...
    Layer::LayerType layerType = Layer::Background;   // what a committed item becomes
    QString displayName;              // Properties-panel title, e.g. "Arrow Tool"
    QString namePrefix;               // Layer name prefix, e.g. "Arrow"
    QString tooltip;                  // toolbar tooltip, without the key
    QChar shortcut;                   // single-key shortcut, upper case

    bool isDrawingTool = true;        // false for the pointer (no template/commit)

    // Build this tool's interaction (owned by the QObject parent).
    std::function<Interactions::IDrawingInteraction*(QObject *parent)> makeInteraction;

    // Build the property "template" tool (null for the pointer).
    std::function<Tools::ITool*()> makeTemplate;

    [[nodiscard]] QString iconPath() const { return QStringLiteral(":/icons/icons/%1.svg").arg(id); }
};

// The built-in tool set, in toolbar order (pointer first).
class ToolRegistry {
public:
    static const QList<ToolSpec>& tools();
    static const ToolSpec* find(const QString &id);
    // Case-insensitive; null when no tool uses the key.
    static const ToolSpec* findByShortcut(QChar key);
};

} // namespace Editor

#endif // IMAGEEDITOR_TOOLREGISTRY_H
