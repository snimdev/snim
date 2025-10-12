#include "AreaSelector.h"
#include <QPainter>
#include <QPainterPath>

namespace Capture {
    AreaSelector::AreaSelector(QWidget *parent)
        : QWidget(parent)
          , m_selecting(false)
          , m_rubberBand(nullptr) {
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint |
                       Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setCursor(Qt::CrossCursor);
        setMouseTracking(true);

        m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

        // Style the rubber band
        QPalette palette;
        palette.setBrush(QPalette::Highlight, QBrush(QColor(0, 120, 215, 100)));
        m_rubberBand->setPalette(palette);
    }

    void AreaSelector::mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            m_startPoint = event->pos();
            m_selecting = true;
            m_rubberBand->setGeometry(QRect(m_startPoint, QSize()));
            m_rubberBand->show();
        }
    }

    void AreaSelector::mouseMoveEvent(QMouseEvent *event) {
        if (m_selecting) {
            m_endPoint = event->pos();
            QRect rect = QRect(m_startPoint, m_endPoint).normalized();
            m_rubberBand->setGeometry(rect);
            update(); // Trigger repaint to show the selection
        }
    }

    void AreaSelector::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton && m_selecting) {
            m_selecting = false;
            m_endPoint = event->pos();
            m_selectedArea = QRect(m_startPoint, m_endPoint).normalized();
            m_rubberBand->hide();

            emit areaSelected(m_selectedArea);
            close(); // Close the selector
        }
    }

    void AreaSelector::paintEvent(QPaintEvent *event) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (!m_screenshot.isNull()) {
            // Draw the screenshot as background
            QPixmap scaled = m_screenshot.scaled(
                size() * devicePixelRatio(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
            scaled.setDevicePixelRatio(devicePixelRatio());
            painter.drawPixmap(rect(), scaled, scaled.rect());

            // Add semi-transparent dark overlay
            painter.fillRect(rect(), QColor(0, 0, 0, 100));

            // Clear the selected area to show the original screenshot
            if (m_selecting && !m_rubberBand->geometry().isEmpty()) {
                QPainterPath path;
                path.addRect(rect());
                path.addRect(m_rubberBand->geometry());

                painter.setCompositionMode(QPainter::CompositionMode_Clear);
                painter.fillPath(path, Qt::transparent);
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

                // Redraw the selected area without overlay
                QRect selected = m_rubberBand->geometry();
                painter.drawPixmap(selected, scaled, selected);

                // Draw selection border
                painter.setPen(QPen(QColor(0, 120, 215), 2));
                painter.drawRect(selected);
            }
        } else {
            painter.fillRect(rect(), QColor(0, 0, 0, 150));
        }

        // Draw instructions
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(12);
        painter.setFont(font);

        QString instructions = m_selecting
                                   ? "Release to capture selected area"
                                   : "Click and drag to select area (ESC to cancel)";

        QRect textRect = painter.fontMetrics().boundingRect(instructions);
        textRect.moveCenter(QPoint(width() / 2, 30));

        // Draw text background
        painter.fillRect(textRect.adjusted(-10, -5, 10, 5),
                         QColor(0, 0, 0, 180));
        painter.drawText(textRect, Qt::AlignCenter, instructions);
    }

    void AreaSelector::keyPressEvent(QKeyEvent *event) {
        if (event->key() == Qt::Key_Escape) {
            emit areaSelected(QRect()); // Empty rect signals cancellation
            close();
        }
    }
} // namespace Capture
