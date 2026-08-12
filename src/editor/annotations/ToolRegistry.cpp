#include "ToolRegistry.h"

#include "tools/ITool.h"
#include "tools/TextTool.h"
#include "tools/ArrowTool.h"
#include "tools/RectangleTool.h"
#include "tools/EllipseTool.h"
#include "tools/FreehandTool.h"
#include "tools/HighlightTool.h"
#include "tools/BlurTool.h"
#include "tools/StepTool.h"

#include "interactions/DrawingInteractions.h"
#include "core/Settings.h"

#include <QPen>
#include <QColor>

namespace Editor {

using namespace Interactions;
using namespace Tools;

namespace {

// Default annotation color, shared by the tool templates.
QColor foregroundColor()
{
    return Core::Settings::editorForeground();
}

} // namespace

const QList<ToolSpec>& ToolRegistry::tools()
{
    // Tools that switch to the pointer leave the new item selected, so it can be moved
    // at once; with the drawing tool still armed, a click would draw another.
    static const QList<ToolSpec> specs = {
        {
            .id = "pointer",
            .displayName = "Pointer",
            .tooltip = "Pointer",
            .shortcut = u'V',
            .isDrawingTool = false,
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new PointerInteraction(p); },
        },
        {
            .id = "arrow",
            .layerType = Layer::Arrow,
            .displayName = "Arrow Tool",
            .namePrefix = "Arrow",
            .tooltip = "Arrow",
            .shortcut = u'A',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new ArrowInteraction(p); },
            .makeTemplate = []() -> ITool* {
                auto *t = new ArrowTool(QPointF(0, 0), QPointF(100, 100), nullptr);
                t->setPen(QPen(foregroundColor(), 3));
                return t;
            },
        },
        {
            .id = "text",
            .layerType = Layer::Text,
            .displayName = "Text Tool",
            .namePrefix = "Text",
            .tooltip = "Text",
            .shortcut = u'T',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* {
                return new ClickInteraction(Qt::IBeamCursor, p);
            },
            .makeTemplate = []() -> ITool* {
                auto *t = new TextTool("Sample Text", nullptr);
                t->setDefaultTextColor(foregroundColor());
                return t;
            },
        },
        {
            .id = "rectangle",
            .layerType = Layer::Rectangle,
            .displayName = "Rectangle Tool",
            .namePrefix = "Rectangle",
            .tooltip = "Rectangle",
            .shortcut = u'R',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* {
                return new RectDragInteraction(RectDragInteraction::Rectangle, p);
            },
            .makeTemplate = []() -> ITool* {
                auto *t = new RectangleTool(QRect(0, 0, 100, 100), nullptr);
                t->setPen(QPen(foregroundColor(), 2));
                return t;
            },
        },
        {
            .id = "ellipse",
            .layerType = Layer::Ellipse,
            .displayName = "Ellipse Tool",
            .namePrefix = "Ellipse",
            .tooltip = "Ellipse",
            .shortcut = u'E',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* {
                return new RectDragInteraction(RectDragInteraction::Ellipse, p);
            },
            .makeTemplate = []() -> ITool* {
                auto *t = new EllipseTool(QRect(0, 0, 100, 100), nullptr);
                t->setPen(QPen(foregroundColor(), 2));
                return t;
            },
        },
        {
            .id = "freehand",
            .layerType = Layer::Freehand,
            .displayName = "Freehand Tool",
            .namePrefix = "Freehand",
            .tooltip = "Freehand",
            .shortcut = u'P',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new PathDragInteraction(p); },
            .makeTemplate = []() -> ITool* {
                auto *t = new FreehandTool(nullptr);
                t->setPen(QPen(foregroundColor(), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                return t;
            },
        },
        {
            .id = "highlight",
            .layerType = Layer::Highlight,
            .displayName = "Highlight Tool",
            .namePrefix = "Highlight",
            .tooltip = "Highlight",
            .shortcut = u'H',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new PathDragInteraction(p); },
            .makeTemplate = []() -> ITool* {
                auto *t = new HighlightTool(nullptr);
                t->setColor(QColor("#b3ff61"));
                t->setWidth(HighlightTool::HIGHLIGHT_WIDTH_MEDIUM);
                return t;
            },
        },
        {
            .id = "blur",
            .layerType = Layer::Blur,
            .displayName = "Blur Tool",
            .namePrefix = "Blur",
            .tooltip = "Blur",
            .shortcut = u'B',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new PathDragInteraction(p); },
            .makeTemplate = []() -> ITool* {
                auto *t = new BlurTool(nullptr);
                t->setBlurRadius(10.0);
                t->setBrushWidth(30.0);
                return t;
            },
        },
        {
            // Stays armed after a stamp: numbering several steps in a row is the point.
            .id = "step",
            .layerType = Layer::Step,
            .displayName = "Step Numbers",
            .namePrefix = "Step",
            .tooltip = "Step numbers",
            .shortcut = u'N',
            .makeInteraction = [](QObject *p) -> IDrawingInteraction* {
                return new ClickInteraction(Qt::CrossCursor, p);
            },
            .makeTemplate = []() -> ITool* {
                auto *t = new StepTool(nullptr);
                t->setColor(foregroundColor());
                t->setTemplateMode(true);   // its "number" field is a one-shot override
                return t;
            },
        },
    };
    return specs;
}

const ToolSpec* ToolRegistry::find(const QString &id)
{
    for (const ToolSpec &s : tools())
        if (s.id == id)
            return &s;
    return nullptr;
}

const ToolSpec* ToolRegistry::findByShortcut(QChar key)
{
    const QChar upper = key.toUpper();
    for (const ToolSpec &s : tools())
        if (!s.shortcut.isNull() && s.shortcut == upper)
            return &s;
    return nullptr;
}

} // namespace Editor
