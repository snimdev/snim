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
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);  // Minimal margins instead of default
    m_stackedWidget = new QStackedWidget(this);
    m_originalPalette = m_stackedWidget->palette();
    mainLayout->addWidget(m_stackedWidget);

    m_emptyWidget = new QWidget();
    m_stackedWidget->addWidget(m_emptyWidget);

    setFixedWidth(200);
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
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop);

    QList<Tools::ToolProperty> properties = tool->getProperties();

    for (const auto& prop : properties) {
        auto *propLayout = new QVBoxLayout();
        propLayout->addWidget(new QLabel(prop.name + ":"));

        if (prop.controlType == "color") {
            auto *button = new QPushButton("Select Color", propertiesWidget);
            connect(button, &QPushButton::clicked, this, [this, tool, p = prop]() {
                auto initialColor = p.value.value<QColor>();
                QColor color = QColorDialog::getColor(initialColor, this, "Select Color");
                if (color.isValid()) {
                    tool->setProperty(p.id, color);
                }
            });
            propLayout->addWidget(button);
        } else if (prop.controlType == "slider") {
            auto *slider = new QSlider(Qt::Horizontal, propertiesWidget);
            slider->setMinimum(prop.options.value("min").toInt());
            slider->setMaximum(prop.options.value("max").toInt());
            slider->setValue(prop.value.toInt());
            connect(slider, &QSlider::valueChanged, this, [tool, p = prop](int value) {
                tool->setProperty(p.id, value);
            });
            propLayout->addWidget(slider);
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

    m_stackedWidget->addWidget(propertiesWidget);
    m_layerWidgetMap[layer] = propertiesWidget;
    m_stackedWidget->setCurrentWidget(propertiesWidget);
}

void LayerProperties::showPropertiesStyle()
{
    //m_stackedWidget->setAutoFillBackground(true);
    QPalette p = palette();
    //p.setColor(QPalette::Window, p.color(QPalette::Base));
    //m_stackedWidget->setPalette(p);
    m_stackedWidget->setStyleSheet("QStackedWidget { border: 1px solid " + p.color(QPalette::Mid).name() + "; padding: 5px; }");
}

void LayerProperties::hidePropertiesStyle()
{
    //m_stackedWidget->setAutoFillBackground(false);
    //m_stackedWidget->setPalette(m_originalPalette);
    m_stackedWidget->setStyleSheet("");
}

} // namespace ImageEditor
