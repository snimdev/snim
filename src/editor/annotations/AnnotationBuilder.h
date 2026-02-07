#ifndef IMAGEEDITOR_ANNOTATIONBUILDER_H
#define IMAGEEDITOR_ANNOTATIONBUILDER_H

#include <QObject>
#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <functional>

class QGraphicsItem;
class QGraphicsScene;

namespace Editor {

class IAnnotationSink;

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
    // Neither scene nor sink is owned; both must stay valid while gestures arrive.
    AnnotationBuilder(QGraphicsScene *scene, IAnnotationSink *sink, QObject *parent = nullptr);
    ~AnnotationBuilder() override;

    [[nodiscard]] Interactions::IDrawingInteraction *interaction(const QString &id) const;
    [[nodiscard]] Tools::ITool *templateFor(const QString &id) const;

    void setImageBounds(const QRect &bounds);
    void setSourcePixmap(const QPixmap &pixmap);

    // Step badges number themselves from this unless the panel set an override.
    void setStepNumberProvider(std::function<int()> provider);

    // "Remember last settings": the next item of this tool starts from item's style.
    void rememberStyle(QGraphicsItem *item, const QString &toolId);

private:
    void registerFactories();
    // factory(template, signal args...) returns a styled item for the sink, or null to reject.
    template <typename Factory, typename Interaction, typename... Args>
    void connectFactory(const QString &id, void (Interaction::*signal)(Args...), Factory factory);

    QGraphicsScene *m_scene;
    IAnnotationSink *m_sink;
    QHash<QString, Interactions::IDrawingInteraction*> m_interactions;
    QHash<QString, Tools::ITool*> m_templates;   // not QObjects, deleted by hand
    QPixmap m_sourcePixmap;
    std::function<int()> m_stepNumberProvider;
};

} // namespace Editor

#endif // IMAGEEDITOR_ANNOTATIONBUILDER_H
