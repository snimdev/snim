#ifndef IMAGEEDITOR_ANNOTATIONBUILDER_H
#define IMAGEEDITOR_ANNOTATIONBUILDER_H

#include <QObject>
#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QString>

class QGraphicsItem;

namespace Editor {

namespace Interactions { class IDrawingInteraction; }
namespace Tools { class ITool; }

/**
 * Widget-free owner of every tool's drawing interaction and style template, built
 * from the ToolRegistry. Turns finished gestures into styled annotation items.
 */
class AnnotationBuilder : public QObject
{
    Q_OBJECT

public:
    explicit AnnotationBuilder(QObject *parent = nullptr);
    ~AnnotationBuilder() override;

    [[nodiscard]] Interactions::IDrawingInteraction *interaction(const QString &id) const;
    [[nodiscard]] Tools::ITool *templateFor(const QString &id) const;

    void setImageBounds(const QRect &bounds);
    void setSourcePixmap(const QPixmap &pixmap);

    // "Remember last settings": the next item of this tool starts from item's style.
    void rememberStyle(QGraphicsItem *item, const QString &toolId);

private:
    QHash<QString, Interactions::IDrawingInteraction*> m_interactions;
    QHash<QString, Tools::ITool*> m_templates;   // not QObjects, deleted by hand
    QPixmap m_sourcePixmap;
};

} // namespace Editor

#endif // IMAGEEDITOR_ANNOTATIONBUILDER_H
