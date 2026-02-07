#ifndef IMAGEEDITOR_ANNOTATIONBUILDER_H
#define IMAGEEDITOR_ANNOTATIONBUILDER_H

#include <QObject>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QString>
#include <functional>
#include "tools/TextTool.h"

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

    // A text item currently in inline-edit mode (not only a freshly placed one)?
    [[nodiscard]] bool isEditingText() const;
    // End editing of the freshly placed text box now: non-empty goes to the sink.
    void commitPendingText();

signals:
    // A new empty text box is in the scene (not owned); editing starts right after.
    void textPlaced(Tools::TextTool *item);

private:
    void registerFactories();
    // On focus-out a freshly placed text box is committed (non-empty) or discarded (empty).
    void finalizePendingText(Tools::TextTool *item);
    // factory(template, signal args...) returns a styled item for the sink, or null to reject.
    template <typename Factory, typename Interaction, typename... Args>
    void connectFactory(const QString &id, void (Interaction::*signal)(Args...), Factory factory);

    QGraphicsScene *m_scene;
    IAnnotationSink *m_sink;
    QHash<QString, Interactions::IDrawingInteraction*> m_interactions;
    QHash<QString, Tools::ITool*> m_templates;   // not QObjects, deleted by hand
    QPixmap m_sourcePixmap;
    std::function<int()> m_stepNumberProvider;
    QPointer<Tools::TextTool> m_pendingTextItem;   // text box being created/edited inline (uncommitted)
};

} // namespace Editor

#endif // IMAGEEDITOR_ANNOTATIONBUILDER_H
