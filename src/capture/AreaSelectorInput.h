// Private to AreaSelector: the overlay's input chain (Chain of Responsibility).
#ifndef CAPTURE_AREASELECTORINPUT_H
#define CAPTURE_AREASELECTORINPUT_H

#include "capture/AreaSelector.h"

#include <QLatin1StringView>

namespace Capture {

// Tools the overlay offers, in strip order.
inline constexpr QLatin1StringView kOverlayTools[] = {
    QLatin1StringView("arrow"), QLatin1StringView("rectangle"),
    QLatin1StringView("ellipse"), QLatin1StringView("freehand"),
    QLatin1StringView("highlight"), QLatin1StringView("step"),
};

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

// While text is being typed every key belongs to it; Esc commits the text.
class AreaSelector::TextEditingHandler : public AreaSelector::InputHandler
{
public:
    using InputHandler::InputHandler;
    bool keyPress(QKeyEvent *event) override;
};

// With a tool armed, presses inside the selection draw instead of moving or committing.
class AreaSelector::StrokeHandler : public AreaSelector::InputHandler
{
public:
    using InputHandler::InputHandler;
    bool keyPress(QKeyEvent *event) override;
    bool mousePress(QMouseEvent *event) override;
    bool mouseMove(QMouseEvent *event) override;
    bool mouseRelease(QMouseEvent *event) override;
    bool mouseDoubleClick(QMouseEvent *event) override;

private:
    [[nodiscard]] bool armed() const;
};

// Tool letters, undo and redo, and Esc to disarm.
class AreaSelector::ToolHandler : public AreaSelector::InputHandler
{
public:
    using InputHandler::InputHandler;
    bool keyPress(QKeyEvent *event) override;
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

} // namespace Capture

#endif // CAPTURE_AREASELECTORINPUT_H
