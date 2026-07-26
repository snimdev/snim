#ifndef CAPTURE_OVERLAYANNOTATIONS_H
#define CAPTURE_OVERLAYANNOTATIONS_H

#include <QCursor>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVariant>
#include <memory>
#include "capture/CaptureGeometry.h"
#include "editor/annotations/AnnotationSet.h"
#include "editor/annotations/IAnnotationSink.h"
#include "screen/SelectionLayer.h"

class QGraphicsItem;
class QGraphicsScene;
class QInputMethodEvent;
class QKeyEvent;
class QPainter;
class QUndoStack;

namespace Editor { class AnnotationBuilder; }
namespace Editor::Interactions { class IDrawingInteraction; }
namespace Editor::Tools { class TextTool; }

namespace Capture {

/**
 * Facade over the area selector's quick-annotation state: one scene shared by every
 * monitor's overlay, the annotation builder and an undo stack. Plugs into the
 * selector as its Screen::SelectionLayer (tool strip, tool keys, strokes, typing).
 * The API takes VIRTUAL-DESKTOP LOGICAL coords; the scene is in frozen-frame logical
 * coords (virtual minus virtualGeometry.topLeft()), so blur samples the frame unchanged.
 */
class OverlayAnnotations : public Screen::SelectionLayer
{
    Q_OBJECT

public:
    // frame: the frozen virtual-desktop shot, DPR-tagged, logical size == virtualGeometry.size().
    OverlayAnnotations(const QPixmap &frame, const QRect &virtualGeometry, QObject *parent = nullptr);
    ~OverlayAnnotations() override;

    // Empty or a non-drawing tool (pointer) means none. Tools stay armed after each shape.
    void setActiveTool(const QString &id);

    bool press(const QPoint &virt);
    bool move(const QPoint &virt);
    bool release(const QPoint &virt);
    [[nodiscard]] bool isDrawing() const;
    void cancelStroke();
    [[nodiscard]] QCursor cursor() const override;

    // Strokes start inside and clamp to this; empty means the whole frame.
    void setSelection(const QRect &virt) override;

    [[nodiscard]] bool isEditingText() const;
    void forwardKey(QKeyEvent *event);
    // Composed text (dead keys, IMEs) for the text box; geometry answers are virtual coords.
    void forwardInputMethod(QInputMethodEvent *event) override;
    [[nodiscard]] QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void commitPendingText();

    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] bool hasItems() const;
    // Discards every item, the undo history and any unfinished stroke or text.
    void clear() override;

    // Paints the annotations over virtSource into target (frame pixels are not drawn).
    void render(QPainter *painter, const QRectF &target, const QRect &virtSource) override;
    // Per-screen native grabs: flattenedCrop prefers the one holding the whole area.
    void setScreenGrabs(const QList<ScreenGrab> &grabs) { m_screenGrabs = grabs; }
    // The frozen frame cropped to virtArea at native resolution, annotations burned in.
    [[nodiscard]] QPixmap flattenedCrop(const QRect &virtArea);
    // Committed items touching virtArea, positioned relative to its top-left.
    [[nodiscard]] Editor::AnnotationSet snapshot(const QRect &virtArea);

    // Screen::SelectionLayer; changed() fires on scene, undo history or armed tool changes.
    [[nodiscard]] bool isArmed() const override { return !m_activeTool.isEmpty(); }
    [[nodiscard]] bool capturesKeyboard() const override { return isEditingText(); }
    [[nodiscard]] std::optional<QString> hint() const override;
    [[nodiscard]] QVector<ToolbarSlot> toolbarSlots() const override;
    void activateSlot(const QString &id) override;
    bool handleKey(QKeyEvent *event, Screen::SelectionContext &context) override;
    bool handleMouse(MouseAction action, const QPoint &virt, Qt::MouseButton button,
                     Screen::SelectionContext &context) override;

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
    // Tool letters, undo and redo, and Esc to disarm.
    bool handleToolKey(QKeyEvent *event, Screen::SelectionContext &context);
    void toggleTool(const QString &id);
    [[nodiscard]] Editor::Interactions::IDrawingInteraction *activeInteraction() const;
    // Stack order, which is paint order: every item shares z 0.
    [[nodiscard]] QList<QGraphicsItem*> committedItems() const;

    QPixmap m_frame;
    QRect m_virtualGeometry;
    QList<ScreenGrab> m_screenGrabs;
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
