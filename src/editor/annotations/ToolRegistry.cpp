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

#include "interactions/PointerToolInteraction.h"
#include "interactions/ArrowDrawingInteraction.h"
#include "interactions/TextDrawingInteraction.h"
#include "interactions/RectangleDrawingInteraction.h"
#include "interactions/EllipseDrawingInteraction.h"
#include "interactions/FreehandDrawingInteraction.h"
#include "interactions/HighlightDrawingInteraction.h"
#include "interactions/BlurDrawingInteraction.h"
#include "interactions/StepDrawingInteraction.h"
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
    static const QList<ToolSpec> specs = [] {
        QList<ToolSpec> v;

        // --- Pointer (selection only; not a drawing tool) ---
        {
            ToolSpec s;
            s.id = "pointer";
            s.layerType = Layer::Background;   // unused (never commits)
            s.displayName = "Pointer";
            s.iconPath = ":/icons/icons/pointer.svg";
            s.tooltip = "Pointer (V)";
            s.isDrawingTool = false;
            s.autoRevealPanel = false;
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new PointerToolInteraction(p); };
            v.push_back(s);
        }

        // --- Arrow ---
        {
            ToolSpec s;
            s.id = "arrow";
            s.layerType = Layer::Arrow;
            s.displayName = "Arrow Tool";
            s.namePrefix = "Arrow";
            s.iconPath = ":/icons/icons/arrow.svg";
            s.tooltip = "Arrow";
            s.switchToPointerAfter = true;   // select the new arrow so it's movable immediately
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new ArrowDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new ArrowTool(QPointF(0, 0), QPointF(100, 100), nullptr);
                t->setPen(QPen(foregroundColor(), 3));
                return t;
            };
            v.push_back(s);
        }

        // --- Text (one-shot click placement) ---
        {
            ToolSpec s;
            s.id = "text";
            s.layerType = Layer::Text;
            s.displayName = "Text Tool";
            s.namePrefix = "Text";
            s.iconPath = ":/icons/icons/text.svg";
            s.tooltip = "Text";
            s.switchToPointerAfter = true;
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new TextDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new TextTool("Sample Text", nullptr);
                t->setDefaultTextColor(foregroundColor());
                return t;
            };
            v.push_back(s);
        }

        // --- Rectangle ---
        {
            ToolSpec s;
            s.id = "rectangle";
            s.layerType = Layer::Rectangle;
            s.displayName = "Rectangle Tool";
            s.namePrefix = "Rectangle";
            s.iconPath = ":/icons/icons/rectangle.svg";
            s.tooltip = "Rectangle";
            s.switchToPointerAfter = true;   // select the new shape so it's movable immediately
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new RectangleDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new RectangleTool(QRect(0, 0, 100, 100), nullptr);
                t->setPen(QPen(foregroundColor(), 2));
                return t;
            };
            v.push_back(s);
        }

        // --- Ellipse ---
        {
            ToolSpec s;
            s.id = "ellipse";
            s.layerType = Layer::Ellipse;
            s.displayName = "Ellipse Tool";
            s.namePrefix = "Ellipse";
            s.iconPath = ":/icons/icons/ellipse.svg";
            s.tooltip = "Ellipse";
            s.switchToPointerAfter = true;   // select the new shape so it's movable immediately
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new EllipseDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new EllipseTool(QRect(0, 0, 100, 100), nullptr);
                t->setPen(QPen(foregroundColor(), 2));
                return t;
            };
            v.push_back(s);
        }

        // --- Freehand ---
        {
            ToolSpec s;
            s.id = "freehand";
            s.layerType = Layer::Freehand;
            s.displayName = "Freehand Tool";
            s.namePrefix = "Freehand";
            s.iconPath = ":/icons/icons/freehand.svg";
            s.tooltip = "Freehand";
            s.switchToPointerAfter = true;   // select the new stroke so it's movable immediately
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new FreehandDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new FreehandTool(nullptr);
                t->setPen(QPen(foregroundColor(), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                return t;
            };
            s.syncStrategy = [](IDrawingInteraction *strategy, ITool *tmpl) {
                auto *si = dynamic_cast<FreehandDrawingInteraction*>(strategy);
                auto *ft = dynamic_cast<FreehandTool*>(tmpl);
                if (si && ft) si->setPen(ft->pen());
            };
            v.push_back(s);
        }

        // --- Highlight ---
        {
            ToolSpec s;
            s.id = "highlight";
            s.layerType = Layer::Highlight;
            s.displayName = "Highlight Tool";
            s.namePrefix = "Highlight";
            s.iconPath = ":/icons/icons/highlight.svg";
            s.tooltip = "Highlight";
            s.switchToPointerAfter = true;   // select the new stroke so it's movable immediately
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new HighlightDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new HighlightTool(nullptr);
                t->setColor(QColor("#b3ff61"));
                t->setWidth(HighlightTool::HIGHLIGHT_WIDTH_MEDIUM);
                return t;
            };
            s.syncStrategy = [](IDrawingInteraction *strategy, ITool *tmpl) {
                auto *si = dynamic_cast<HighlightDrawingInteraction*>(strategy);
                auto *ht = dynamic_cast<HighlightTool*>(tmpl);
                if (si && ht) { si->setColor(ht->color()); si->setWidth(ht->width()); }
            };
            v.push_back(s);
        }

        // --- Blur ---
        {
            ToolSpec s;
            s.id = "blur";
            s.layerType = Layer::Blur;
            s.displayName = "Blur Tool";
            s.namePrefix = "Blur";
            s.iconPath = ":/icons/icons/blur.svg";
            s.tooltip = "Blur";
            s.switchToPointerAfter = true;   // select the new stroke so it's movable immediately
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new BlurDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new BlurTool(nullptr);
                t->setBlurRadius(10.0);
                t->setBrushWidth(30.0);
                return t;
            };
            s.syncStrategy = [](IDrawingInteraction *strategy, ITool *tmpl) {
                auto *si = dynamic_cast<BlurDrawingInteraction*>(strategy);
                auto *bt = dynamic_cast<BlurTool*>(tmpl);
                if (si && bt) { si->setBlurRadius(bt->blurRadius()); si->setBrushWidth(bt->brushWidth()); }
            };
            v.push_back(s);
        }

        // --- Step numbers (one badge per click) ---
        {
            ToolSpec s;
            s.id = "step";
            s.layerType = Layer::Step;
            s.displayName = "Step Numbers";
            s.namePrefix = "Step";
            s.iconPath = ":/icons/icons/step.svg";
            s.tooltip = "Step numbers";
            // Stays armed after a stamp: numbering several steps in a row is the point.
            s.makeInteraction = [](QObject *p) -> IDrawingInteraction* { return new StepDrawingInteraction(p); };
            s.makeTemplate = []() -> ITool* {
                auto *t = new StepTool(nullptr);
                t->setColor(foregroundColor());
                t->setTemplateMode(true);   // its "number" field is a one-shot override
                return t;
            };
            v.push_back(s);
        }

        return v;
    }();
    return specs;
}

const ToolSpec* ToolRegistry::find(const QString &id)
{
    for (const ToolSpec &s : tools())
        if (s.id == id)
            return &s;
    return nullptr;
}

} // namespace Editor
