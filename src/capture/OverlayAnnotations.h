#ifndef CAPTURE_OVERLAYANNOTATIONS_H
#define CAPTURE_OVERLAYANNOTATIONS_H

#include <QCursor>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QString>
#include <memory>
#include "editor/annotations/IAnnotationSink.h"

class QGraphicsScene;
class QKeyEvent;
class QPainter;
class QUndoStack;

namespace Editor { class AnnotationBuilder; }
namespace Editor::Interactions { class IDrawingInteraction; }
namespace Editor::Tools { class TextTool; }

namespace Capture {

/**
 * Facade over the area selector's quick-annotation state: one scene shared by every
 * monitor's overlay, the annotation builder and an undo stack. The API takes
 * VIRTUAL-DESKTOP LOGICAL coords; the scene is in frozen-frame logical coords
 * (virtual minus virtualGeometry.topLeft()), so blur samples the frame unchanged.
 */
class OverlayAnnotations : public QObject
{
    Q_OBJECT

public:
    // frame: the frozen virtual-desktop shot, DPR-tagged, logical size == virtualGeometry.size().
    OverlayAnnotations(const QPixmap &frame, const QRect &virtualGeometry, QObject *parent = nullptr);
    ~OverlayAnnotations() override;

    // Empty or a non-drawing tool (pointer) means none. Tools stay armed after each shape.
    void setActiveTool(const QString &id);
    [[nodiscard]] QString activeTool() const { return m_activeTool; }

    bool press(const QPoint &virt);
    bool move(const QPoint &virt);
    bool release(const QPoint &virt);
    [[nodiscard]] bool isDrawing() const;
    void cancelStroke();
    [[nodiscard]] QCursor cursor() const;

    // Strokes start inside and clamp to this; empty means the whole frame.
    void setSelection(const QRect &virt);

    [[nodiscard]] bool isEditingText() const;
    void forwardKey(QKeyEvent *event);
    void commitPendingText();

    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] bool hasItems() const;
    // Discards every item, the undo history and any unfinished stroke or text.
    void clear();

    // Paints the annotations over virtSource into target (frame pixels are not drawn).
    void render(QPainter *painter, const QRectF &target, const QRect &virtSource);
    // The frozen frame cropped to virtArea at native resolution, annotations burned in.
    [[nodiscard]] QPixmap flattenedCrop(const QRect &virtArea);

signals:
    void changed();

private:
    class UndoStackSink : public Editor::IAnnotationSink
    {
    public:
        explicit UndoStackSink(OverlayAnnotations *session) : m_session(session) {}
        void commit(QGraphicsItem *item, const QString &toolId) override;

    private:
        OverlayAnnotations *m_session;
    };

    [[nodiscard]] QPoint toScene(const QPoint &virt) const;
    [[nodiscard]] Editor::Interactions::IDrawingInteraction *activeInteraction() const;
    [[nodiscard]] int nextStepNumber() const;

    QPixmap m_frame;
    QRect m_virtualGeometry;
    QRect m_selection;   // scene coords, empty until setSelection
    QString m_activeTool;
    std::unique_ptr<QGraphicsScene> m_scene;
    std::unique_ptr<QUndoStack> m_stack;   // declared after the scene so it dies first
    UndoStackSink m_sink;
    Editor::AnnotationBuilder *m_builder = nullptr;   // deleted first, by hand
    QPointer<Editor::Tools::TextTool> m_placedText;
};

} // namespace Capture

#endif // CAPTURE_OVERLAYANNOTATIONS_H
