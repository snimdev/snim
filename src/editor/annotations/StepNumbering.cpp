#include "StepNumbering.h"

#include "Layer.h"
#include "tools/StepTool.h"

#include <QGraphicsItem>

namespace Editor {

namespace {

// Groups are expanded in place, so bottom-to-top order is kept across nesting.
void appendItems(const QList<Layer*> &layers, QList<QGraphicsItem*> *out)
{
    for (Layer *layer : layers) {
        if (!layer)
            continue;
        if (layer->isGroup())
            appendItems(layer->children(), out);
        else if (layer->item())
            out->append(layer->item());
    }
}

} // namespace

int nextStepNumber(const QList<QGraphicsItem*> &items)
{
    for (auto it = items.crbegin(); it != items.crend(); ++it)
        if (auto *step = dynamic_cast<const Tools::StepTool*>(*it))
            return step->number() + 1;
    return 1;
}

int nextStepNumber(const QList<Layer*> &topLevelLayers)
{
    QList<QGraphicsItem*> items;
    appendItems(topLevelLayers, &items);
    return nextStepNumber(items);
}

} // namespace Editor
