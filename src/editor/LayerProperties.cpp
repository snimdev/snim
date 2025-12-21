#include "LayerProperties.h"
#include "Layer.h"
#include "tools/ITool.h"
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QColorDialog>
#include <QPushButton>
#include <QComboBox>
#include <QVariant>
#include <QSlider>

namespace ImageEditor {

LayerProperties::LayerProperties(QWidget *parent)
    : QWidget(parent)
    , m_toolWidget(nullptr)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Section header
    auto *header = new QLabel("Properties");
    header->setObjectName("sectionHeader");
    mainLayout->addWidget(header);

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setObjectName("propertiesStack");
    m_originalPalette = m_stackedWidget->palette();
    mainLayout->addWidget(m_stackedWidget);

    m_emptyWidget = new QWidget();
    m_stackedWidget->addWidget(m_emptyWidget);

    setMinimumWidth(180);
    setWindowTitle("Properties");
}

void LayerProperties::setLayer(Layer *layer)
{
    if (!layer) {
        hidePropertiesStyle();
        m_stackedWidget->setCurrentWidget(m_emptyWidget);
        return;
    }

    showPropertiesStyle();
    if (m_layerWidgetMap.contains(layer)) {
        m_stackedWidget->setCurrentWidget(m_layerWidgetMap[layer]);
    } else {
        buildPropertiesUI(layer);
    }
}

void LayerProperties::setTool(Tools::ITool *tool, const QString &toolName)
{
    if (!tool) {
        hidePropertiesStyle();
        m_stackedWidget->setCurrentWidget(m_emptyWidget);
        return;
    }

    showPropertiesStyle();
    buildPropertiesUIForTool(tool, toolName);
}

void LayerProperties::removeLayer(Layer *layer)
{
    if (m_layerWidgetMap.contains(layer)) {
        QWidget *widget = m_layerWidgetMap.take(layer);
        m_stackedWidget->removeWidget(widget);
        widget->deleteLater();
        if (m_layerWidgetMap.isEmpty()) {
            hidePropertiesStyle();
        }
    }
}

void LayerProperties::buildPropertiesUI(Layer *layer)
{
    auto* tool = dynamic_cast<Tools::ITool*>(layer->getTool());
    if (!tool) {
        m_stackedWidget->setCurrentWidget(m_emptyWidget);
        return;
    }

    auto* propertiesWidget = new QWidget();
    auto* layout = new QVBoxLayout(propertiesWidget);
    layout->setSpacing(12);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setAlignment(Qt::AlignTop);

    QList<Tools::ToolProperty> properties = tool->getProperties();

    for (const auto& prop : properties) {
        auto *propLayout = new QVBoxLayout();
        propLayout->setSpacing(4);

        auto *label = new QLabel(prop.name);
        QFont labelFont = label->font();
        labelFont.setPointSize(labelFont.pointSize() - 1);
        label->setFont(labelFont);
        propLayout->addWidget(label);

        if (prop.controlType == "color") {
            auto *swatch = new QPushButton(propertiesWidget);
            swatch->setFixedSize(32, 32);
            swatch->setToolTip("Click to change color");
            QColor initialColor = prop.value.value<QColor>();
            swatch->setStyleSheet(
                QString("QPushButton { background: %1; border: 1px solid palette(mid); border-radius: 6px; }"
                        "QPushButton:hover { border-color: palette(dark); }")
                .arg(initialColor.name()));
            connect(swatch, &QPushButton::clicked, this, [this, tool, swatch, p = prop]() {
                auto currentColor = p.value.value<QColor>();
                QColor color = QColorDialog::getColor(currentColor, this, "Select Color");
                if (color.isValid()) {
                    tool->setProperty(p.id, color);
                    swatch->setStyleSheet(
                        QString("QPushButton { background: %1; border: 1px solid palette(mid); border-radius: 6px; }"
                                "QPushButton:hover { border-color: palette(dark); }")
                        .arg(color.name()));
                }
            });
            propLayout->addWidget(swatch);
        } else if (prop.controlType == "slider") {
            auto *sliderRow = new QHBoxLayout();
            auto *slider = new QSlider(Qt::Horizontal, propertiesWidget);
            slider->setMinimum(prop.options.value("min").toInt());
            slider->setMaximum(prop.options.value("max").toInt());
            slider->setValue(prop.value.toInt());
            auto *valueLabel = new QLabel(QString::number(prop.value.toInt()), propertiesWidget);
            valueLabel->setFixedWidth(28);
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            QFont valFont = valueLabel->font();
            valFont.setPointSize(valFont.pointSize() - 1);
            valueLabel->setFont(valFont);
            connect(slider, &QSlider::valueChanged, this, [tool, valueLabel, p = prop](int value) {
                tool->setProperty(p.id, value);
                valueLabel->setText(QString::number(value));
            });
            sliderRow->addWidget(slider, 1);
            sliderRow->addWidget(valueLabel);
            propLayout->addLayout(sliderRow);
        } else if (prop.controlType == "dropdown") {
            auto *comboBox = new QComboBox(propertiesWidget);
            QStringList items = prop.options.value("items").toStringList();
            comboBox->addItems(items);
            comboBox->setCurrentText(prop.value.toString());
            connect(comboBox, &QComboBox::currentTextChanged, this, [tool, p = prop](const QString& text) {
                tool->setProperty(p.id, text);
            });
            propLayout->addWidget(comboBox);
        }
        layout->addLayout(propLayout);
    }

    layout->addStretch();
    m_stackedWidget->addWidget(propertiesWidget);
    m_layerWidgetMap[layer] = propertiesWidget;
    m_stackedWidget->setCurrentWidget(propertiesWidget);
}

void LayerProperties::buildPropertiesUIForTool(Tools::ITool *tool, const QString &title)
{
    if (m_toolWidget) {
        m_stackedWidget->removeWidget(m_toolWidget);
        m_toolWidget->deleteLater();
        m_toolWidget = nullptr;
    }

    auto* propertiesWidget = new QWidget();
    auto* layout = new QVBoxLayout(propertiesWidget);
    layout->setSpacing(12);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setAlignment(Qt::AlignTop);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("sectionHeader");
    titleLabel->setAlignment(Qt::AlignLeft);
    layout->addWidget(titleLabel);

    QList<Tools::ToolProperty> properties = tool->getProperties();

    for (const auto& prop : properties) {
        auto *propLayout = new QVBoxLayout();
        propLayout->setSpacing(4);

        auto *label = new QLabel(prop.name);
        QFont labelFont = label->font();
        labelFont.setPointSize(labelFont.pointSize() - 1);
        label->setFont(labelFont);
        propLayout->addWidget(label);

        if (prop.controlType == "color") {
            auto *swatch = new QPushButton(propertiesWidget);
            swatch->setFixedSize(32, 32);
            swatch->setToolTip("Click to change color");
            QColor initialColor = prop.value.value<QColor>();
            swatch->setStyleSheet(
                QString("QPushButton { background: %1; border: 1px solid palette(mid); border-radius: 6px; }"
                        "QPushButton:hover { border-color: palette(dark); }")
                .arg(initialColor.name()));
            connect(swatch, &QPushButton::clicked, this, [this, tool, swatch, p = prop]() {
                auto currentColor = p.value.value<QColor>();
                QColor color = QColorDialog::getColor(currentColor, this, "Select Color");
                if (color.isValid()) {
                    tool->setProperty(p.id, color);
                    swatch->setStyleSheet(
                        QString("QPushButton { background: %1; border: 1px solid palette(mid); border-radius: 6px; }"
                                "QPushButton:hover { border-color: palette(dark); }")
                        .arg(color.name()));
                }
            });
            propLayout->addWidget(swatch);
        } else if (prop.controlType == "slider") {
            auto *sliderRow = new QHBoxLayout();
            auto *slider = new QSlider(Qt::Horizontal, propertiesWidget);
            slider->setMinimum(prop.options.value("min").toInt());
            slider->setMaximum(prop.options.value("max").toInt());
            slider->setValue(prop.value.toInt());
            auto *valueLabel = new QLabel(QString::number(prop.value.toInt()), propertiesWidget);
            valueLabel->setFixedWidth(28);
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            QFont valFont = valueLabel->font();
            valFont.setPointSize(valFont.pointSize() - 1);
            valueLabel->setFont(valFont);
            connect(slider, &QSlider::valueChanged, this, [tool, valueLabel, p = prop](int value) {
                tool->setProperty(p.id, value);
                valueLabel->setText(QString::number(value));
            });
            sliderRow->addWidget(slider, 1);
            sliderRow->addWidget(valueLabel);
            propLayout->addLayout(sliderRow);
        } else if (prop.controlType == "dropdown") {
            auto *comboBox = new QComboBox(propertiesWidget);
            QStringList items = prop.options.value("items").toStringList();
            comboBox->addItems(items);
            comboBox->setCurrentText(prop.value.toString());
            connect(comboBox, &QComboBox::currentTextChanged, this, [tool, p = prop](const QString& text) {
                tool->setProperty(p.id, text);
            });
            propLayout->addWidget(comboBox);
        }
        layout->addLayout(propLayout);
    }

    layout->addStretch();
    m_toolWidget = propertiesWidget;
    m_stackedWidget->addWidget(propertiesWidget);
    m_stackedWidget->setCurrentWidget(propertiesWidget);
}

void LayerProperties::showPropertiesStyle()
{
    // Styling handled by editor.qss via #propertiesStack object name
}

void LayerProperties::hidePropertiesStyle()
{
    // Styling handled by editor.qss via #propertiesStack object name
}

} // namespace ImageEditor
