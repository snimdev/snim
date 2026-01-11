#ifndef IMAGEEDITOR_STEPNUMBERING_H
#define IMAGEEDITOR_STEPNUMBERING_H

#include <QList>

namespace Editor {

class Layer;

/**
 * Number the next step badge would carry: one past the most recently added badge,
 * i.e. the topmost Step layer in z-order (groups are searched top-down too), or 1
 * when there is none. Deriving it from the stack rather than a counter makes undo
 * fall out for free, and lets a re-stamped "1" restart the sequence.
 */
int nextStepNumber(const QList<Layer*> &topLevelLayers);

} // namespace Editor

#endif // IMAGEEDITOR_STEPNUMBERING_H
