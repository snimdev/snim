// Private to AreaSelector: the overlay's input chain (Chain of Responsibility).
#ifndef SCREEN_AREASELECTORINPUT_H
#define SCREEN_AREASELECTORINPUT_H

#include "screen/AreaSelector.h"

namespace Screen {

// One link of the chain; returning true stops the event there.
class AreaSelector::InputHandler
{
public:
    explicit InputHandler(AreaSelector &selector) : m_sel(selector) {}
    virtual ~InputHandler() = default;
    Q_DISABLE_COPY_MOVE(InputHandler)

    virtual bool keyPress(QKeyEvent *) { return false; }
    virtual bool mousePress(QMouseEvent *) { return false; }
    virtual bool mouseMove(QMouseEvent *) { return false; }
    virtual bool mouseRelease(QMouseEvent *) { return false; }
    virtual bool mouseDoubleClick(QMouseEvent *) { return false; }

protected:
    AreaSelector &m_sel;
};

// Toolbar clicks; the bar sits just outside the selection, so it goes first.
class AreaSelector::ToolbarHandler : public AreaSelector::InputHandler
{
public:
    using InputHandler::InputHandler;
    bool mousePress(QMouseEvent *event) override;
};

// Adapter handing input to the layer, and the selector's side of its SelectionContext.
class AreaSelector::LayerHandler : public AreaSelector::InputHandler, public SelectionContext
{
public:
    using InputHandler::InputHandler;
    bool keyPress(QKeyEvent *event) override;
    bool mousePress(QMouseEvent *event) override;
    bool mouseMove(QMouseEvent *event) override;
    bool mouseRelease(QMouseEvent *event) override;
    bool mouseDoubleClick(QMouseEvent *event) override;

    [[nodiscard]] bool adjusting() const override;
    [[nodiscard]] bool actionsAvailable() const override;
    [[nodiscard]] Hit hitTest(const QPoint &virt) const override;
    void setCursor(const QCursor &cursor) override;
    void refreshCursor() override;

private:
    bool forward(SelectionLayer::MouseAction action, QMouseEvent *event);
};

// Selecting, adjusting and the terminal actions; last in the chain.
class AreaSelector::SelectionHandler : public AreaSelector::InputHandler
{
public:
    using InputHandler::InputHandler;
    bool keyPress(QKeyEvent *event) override;
    bool mousePress(QMouseEvent *event) override;
    bool mouseMove(QMouseEvent *event) override;
    bool mouseRelease(QMouseEvent *event) override;
    bool mouseDoubleClick(QMouseEvent *event) override;

private:
    QPoint m_interiorPressLocal;     // press point for click-vs-drag inside selection
    bool   m_interiorMoved = false;  // whether an interior drag actually moved
};

} // namespace Screen

#endif // SCREEN_AREASELECTORINPUT_H
