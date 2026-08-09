#ifndef IMAGEEDITOR_DRAWINGINTERACTIONS_H
#define IMAGEEDITOR_DRAWINGINTERACTIONS_H

#include "IDrawingInteraction.h"
#include <QList>
#include <QObject>
#include <QPoint>
#include <QRect>

class QGraphicsItem;

namespace Editor::Tools { class ITool; }

namespace Editor::Interactions {

// Selection only: reports the clicked item and leaves dragging to the scene.
class PointerInteraction : public QObject, public IDrawingInteraction
{
    Q_OBJECT

public:
    explicit PointerInteraction(QObject *parent = nullptr) : QObject(parent) {}

    bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) override;
    [[nodiscard]] Qt::CursorShape getCursor() const override { return Qt::ArrowCursor; }

signals:
    void itemClicked(QGraphicsItem *item);   // the top-level item, never a handle
};

// One click places one item (text box, step badge).
class ClickInteraction : public QObject, public IDrawingInteraction
{
    Q_OBJECT

public:
    explicit ClickInteraction(Qt::CursorShape cursor, QObject *parent = nullptr);

    bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) override;
    [[nodiscard]] Qt::CursorShape getCursor() const override { return m_cursor; }

signals:
    void clicked(const QPointF &scenePos);

private:
    Qt::CursorShape m_cursor;
};

// Press, drag, release, with a preview item in the scene meanwhile. Moves and the
// release are clamped to the image bounds.
class DragInteraction : public QObject, public IDrawingInteraction
{
    Q_OBJECT

public:
    bool onMousePress(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseMove(const QPointF &scenePos, QGraphicsScene *scene) override;
    bool onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene) override;
    void cleanup(QGraphicsScene *scene) override;
    [[nodiscard]] bool isDrawing() const override { return m_isDrawing; }
    [[nodiscard]] Qt::CursorShape getCursor() const override { return Qt::CrossCursor; }

    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }
    // The tool's style template (not owned); previews read it on every press.
    void setTemplate(const Tools::ITool *tmpl) { m_template = tmpl; }

protected:
    explicit DragInteraction(QObject *parent) : QObject(parent) {}

    virtual QGraphicsItem *createPreview(const QPointF &startPos) = 0;
    virtual void updatePreview(const QPointF &pos) = 0;
    // Emits the finished gesture; false when it is too small to keep.
    virtual bool finish() = 0;

    const Tools::ITool *m_template = nullptr;
    QGraphicsItem *m_previewItem = nullptr;
    QPointF m_startPoint;
    QPointF m_endPoint;

private:
    [[nodiscard]] QPointF clampToImageBounds(const QPointF &point) const;

    bool m_isDrawing = false;
    QRect m_imageBounds;
};

// Rectangle or ellipse: a dashed rubber band, then the normalized rect.
class RectDragInteraction : public DragInteraction
{
    Q_OBJECT

public:
    enum Shape { Rectangle, Ellipse };
    explicit RectDragInteraction(Shape shape, QObject *parent = nullptr);

signals:
    void rectDrawn(const QRect &rect);

protected:
    QGraphicsItem *createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &pos) override;
    bool finish() override;

private:
    Shape m_shape;
};

// Freehand, highlight and blur: the preview is the stroke itself, growing point by point.
class PathDragInteraction : public DragInteraction
{
    Q_OBJECT

public:
    explicit PathDragInteraction(QObject *parent = nullptr) : DragInteraction(parent) {}

signals:
    void pathDrawn(const QList<QPointF> &points);

protected:
    QGraphicsItem *createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &pos) override;
    bool finish() override;
};

// Arrow: the preview is the arrow itself, re-pointed in place as the mouse moves.
class ArrowInteraction : public DragInteraction
{
    Q_OBJECT

public:
    explicit ArrowInteraction(QObject *parent = nullptr) : DragInteraction(parent) {}

    [[nodiscard]] Qt::CursorShape getCursor() const override { return Qt::ArrowCursor; }
    [[nodiscard]] Qt::CursorShape getDrawingCursor() const override { return Qt::CrossCursor; }

signals:
    void arrowDrawn(const QPoint &start, const QPoint &end);

protected:
    QGraphicsItem *createPreview(const QPointF &startPos) override;
    void updatePreview(const QPointF &pos) override;
    bool finish() override;
};

} // namespace Editor::Interactions

#endif // IMAGEEDITOR_DRAWINGINTERACTIONS_H
