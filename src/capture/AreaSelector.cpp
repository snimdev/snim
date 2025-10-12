#include "AreaSelector.h"
#include <QPainter>

namespace Capture {

AreaSelector::AreaSelector(QWidget *parent)
    : QWidget(parent)
    , m_selecting(false)
    , m_rubberBand(nullptr)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
}

void AreaSelector::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_selecting = true;
        m_rubberBand->setGeometry(QRect(m_startPoint, QSize()));
        m_rubberBand->show();
    }
}

void AreaSelector::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        m_endPoint = event->pos();
        m_rubberBand->setGeometry(QRect(m_startPoint, m_endPoint).normalized());
    }
}

void AreaSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_endPoint = event->pos();
        m_selectedArea = QRect(m_startPoint, m_endPoint).normalized();
        m_rubberBand->hide();

        emit areaSelected(m_selectedArea);
    }
}

void AreaSelector::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if (!m_screenshot.isNull()) {
        // Draw the screenshot as background
        painter.drawPixmap(rect(), m_screenshot, m_screenshot.rect());

        // Add a semi-transparent dark overlay
        painter.fillRect(rect(), QColor(0, 0, 0, 50));
    } else {
        // Fallback to dark background if no screenshot
        painter.fillRect(rect(), QColor(0, 0, 0, 100));
    }
}

void AreaSelector::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit areaSelected(QRect());
    }
}

} // namespace Capture
