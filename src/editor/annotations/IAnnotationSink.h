#ifndef IMAGEEDITOR_IANNOTATIONSINK_H
#define IMAGEEDITOR_IANNOTATIONSINK_H

#include <QString>

class QGraphicsItem;

namespace Editor {

/**
 * Where a finished annotation goes (editor layer, overlay undo stack, ...). commit()
 * receives an owning raw pointer and must take it; the builder never deletes it.
 */
class IAnnotationSink
{
public:
    virtual ~IAnnotationSink() = default;
    virtual void commit(QGraphicsItem *item, const QString &toolId) = 0;
};

} // namespace Editor

#endif // IMAGEEDITOR_IANNOTATIONSINK_H
