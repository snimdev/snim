#include "LayerProperties.h"
#include "Layer.h"
#include "ArrowItem.h"
#include "EditableTextItem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QColorDialog>
#include <QGraphicsPixmapItem>
#include <QBrush>
#include <QGraphicsLineItem>
#include <QPainter>
#include <QPen>

namespace ImageEditor {

LayerProperties::LayerProperties(QWidget *parent)
    : QWidget(parent)
    , m_currentLayer(nullptr)
    , m_backgroundGroup(nullptr)
    , m_backgroundColorButton(nullptr)
    , m_textGroup(nullptr)
    , m_textColorButton(nullptr)
    , m_arrowGroup(nullptr)
    , m_arrowColorButton(nullptr)
{
    setFixedWidth(200);
    setWindowTitle("Layer Properties");
    setupUI();
}

void LayerProperties::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    m_backgroundGroup = new QGroupBox("Background Properties");
    auto *backgroundLayout = new QVBoxLayout(m_backgroundGroup);

    auto *bgColorLayout = new QHBoxLayout();
    bgColorLayout->addWidget(new QLabel("Background Color:"));
    m_backgroundColorButton = createColorButton(QColor(255, 255, 255));
    connect(m_backgroundColorButton, &QPushButton::clicked,
            this, &LayerProperties::onBackgroundColorButtonClicked);
    bgColorLayout->addWidget(m_backgroundColorButton);
    backgroundLayout->addLayout(bgColorLayout);

    layout->addWidget(m_backgroundGroup);

    m_textGroup = new QGroupBox("Text Properties");
    auto *textLayout = new QVBoxLayout(m_textGroup);

    auto *textColorLayout = new QHBoxLayout();
    textColorLayout->addWidget(new QLabel("Text Color:"));
    m_textColorButton = createColorButton(QColor(0, 0, 0));
    connect(m_textColorButton, &QPushButton::clicked,
            this, &LayerProperties::onTextColorButtonClicked);
    textColorLayout->addWidget(m_textColorButton);
    textLayout->addLayout(textColorLayout);

    layout->addWidget(m_textGroup);

    m_arrowGroup = new QGroupBox("Arrow Properties");
    auto *arrowLayout = new QVBoxLayout(m_arrowGroup);

    auto *arrowColorLayout = new QHBoxLayout();
    arrowColorLayout->addWidget(new QLabel("Arrow Color:"));
    m_arrowColorButton = createColorButton(QColor(255, 0, 0));
    connect(m_arrowColorButton, &QPushButton::clicked,
            this, &LayerProperties::onArrowColorButtonClicked);
    arrowColorLayout->addWidget(m_arrowColorButton);
    arrowLayout->addLayout(arrowColorLayout);

    layout->addWidget(m_arrowGroup);

    layout->addStretch();

    m_backgroundGroup->hide();
    m_textGroup->hide();
    m_arrowGroup->hide();
}

QPushButton* LayerProperties::createColorButton(const QColor &color)
{
    auto *button = new QPushButton();
    button->setFixedSize(30, 20);
    button->setStyleSheet(QString("background-color: %1; border: 1px solid black;").arg(color.name()));
    return button;
}

void LayerProperties::setLayer(Layer *layer)
{
    m_currentLayer = layer;
    updatePropertiesForLayer();
}

void LayerProperties::updatePropertiesForLayer()
{
    m_backgroundGroup->hide();
    m_textGroup->hide();
    m_arrowGroup->hide();

    if (!m_currentLayer) {
        return;
    }

    switch (m_currentLayer->type()) {
        case Layer::Background:
            m_backgroundGroup->show();
            break;
        case Layer::Text:
            m_textGroup->show();
            break;
        case Layer::Arrow:
            m_arrowGroup->show();
            break;
    }
}

void LayerProperties::onBackgroundColorButtonClicked()
{
    if (!m_currentLayer || m_currentLayer->type() != Layer::Background) {
        return;
    }

    QColor color = QColorDialog::getColor(Qt::white, this, "Select Background Color");
    if (color.isValid()) {
        m_backgroundColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid black;").arg(color.name()));

        if (m_currentLayer->item()) {
            auto *pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(m_currentLayer->item());
            if (pixmapItem) {
                // Update background color by recreating pixmap with new background
                QPixmap originalPixmap = pixmapItem->pixmap();
                QPixmap newPixmap(originalPixmap.size());
                newPixmap.fill(color);

                QPainter painter(&newPixmap);
                painter.drawPixmap(0, 0, originalPixmap);
                painter.end();

                pixmapItem->setPixmap(newPixmap);
            }
        }
    }
}

void LayerProperties::onTextColorButtonClicked()
{
    if (!m_currentLayer || m_currentLayer->type() != Layer::Text) {
        return;
    }

    QColor color = QColorDialog::getColor(Qt::black, this, "Select Text Color");
    if (color.isValid()) {
        m_textColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid black;").arg(color.name()));

        if (m_currentLayer->item()) {
            auto *textItem = qgraphicsitem_cast<EditableTextItem*>(m_currentLayer->item());
            if (textItem) {
                textItem->setDefaultTextColor(color);
            }
        }
    }
}

void LayerProperties::onArrowColorButtonClicked()
{
    if (!m_currentLayer || m_currentLayer->type() != Layer::Arrow) {
        return;
    }

    QColor color = QColorDialog::getColor(Qt::red, this, "Select Arrow Color");
    if (color.isValid()) {
        m_arrowColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid black;").arg(color.name()));

        if (m_currentLayer->item()) {
            // Try to cast to ArrowItem first
            auto *arrowItem = qgraphicsitem_cast<ArrowItem*>(m_currentLayer->item());
            if (arrowItem) {
                QPen pen(color, 3);
                arrowItem->setPen(pen);
            } else {
                // Fallback for old line items (if any still exist)
                auto *lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(m_currentLayer->item());
                if (lineItem) {
                    QPen pen = lineItem->pen();
                    pen.setColor(color);
                    lineItem->setPen(pen);
                }
            }
        }
    }
}

} // namespace ImageEditor
