#include "StepNumbering.h"

#include "Layer.h"
#include "tools/StepTool.h"

namespace Editor {

namespace {

// Walks a layer list from top to bottom (the list is bottom-to-top) and reports the
// first Step badge found. Groups are searched the same way, in place.
bool findTopmostStepNumber(const QList<Layer*> &layers, int *number)
{
    for (auto it = layers.crbegin(); it != layers.crend(); ++it) {
        Layer *layer = *it;
        if (!layer)
            continue;
        if (layer->isGroup()) {
            if (findTopmostStepNumber(layer->children(), number))
                return true;
            continue;
        }
        if (auto *step = dynamic_cast<Tools::StepTool*>(layer->item())) {
            *number = step->number();
            return true;
        }
    }
    return false;
}

} // namespace

int nextStepNumber(const QList<Layer*> &topLevelLayers)
{
    int last = 0;
    if (findTopmostStepNumber(topLevelLayers, &last))
        return last + 1;
    return 1;
}

} // namespace Editor
