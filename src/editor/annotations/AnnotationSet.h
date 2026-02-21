#ifndef IMAGEEDITOR_ANNOTATIONSET_H
#define IMAGEEDITOR_ANNOTATIONSET_H

#include <QList>
#include <QMetaType>
#include <QPointF>
#include <QString>
#include <memory>

class QGraphicsItem;
class QPixmap;

namespace Editor {

/**
 * A copyable snapshot of annotation items (Prototype pattern): each entry holds a
 * clone that never joins a scene, and instantiate() clones it again. Copies share
 * the prototypes, which die with the last copy.
 */
class AnnotationSet
{
public:
    struct Entry {
        QString toolId;
        QPointF pos;
        std::shared_ptr<QGraphicsItem> prototype;
    };

    struct Instance {
        QString toolId;
        QGraphicsItem *item = nullptr;
    };

    // Clones source through ITool::clone(); items that are not tools are skipped.
    void add(const QString &toolId, const QGraphicsItem &source, const QPointF &pos);

    [[nodiscard]] bool isEmpty() const { return m_entries.isEmpty(); }
    [[nodiscard]] int size() const { return int(m_entries.size()); }
    [[nodiscard]] const QList<Entry> &entries() const { return m_entries; }

    // Fresh items placed at their entry pos, in entry order; the caller owns them.
    // Blur items sample blurSource, which must share the positions' coordinate space.
    [[nodiscard]] QList<Instance> instantiate(const QPixmap &blurSource) const;

private:
    QList<Entry> m_entries;
};

} // namespace Editor

Q_DECLARE_METATYPE(Editor::AnnotationSet)

#endif // IMAGEEDITOR_ANNOTATIONSET_H
