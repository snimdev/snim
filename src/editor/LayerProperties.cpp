#include "LayerProperties.h"
#include "Layer.h"
#include "tools/ITool.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QColorDialog>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QVariant>
#include <QSlider>
#include <QIcon>
#include <QPixmap>
#include <QMargins>

namespace Editor {

namespace {

// Left-to-right wrapping layout: lays items in a row and wraps to the next line
// when they don't fit the current width, so "swatch" tiles fill as many columns as
// fit (3 when narrow, 4+ when wider). Unlike a QGridLayout its minimum width is a
// single tile, so inside a scroll area it wraps to the viewport instead of
// overflowing. Adapted from the canonical Qt FlowLayout example; no Q_OBJECT needed.
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(int spacing = 6) : m_space(spacing)
    {
        setContentsMargins(0, 0, 0, 0);
    }
    ~FlowLayout() override
    {
        QLayoutItem *item;
        while ((item = takeAt(0)))
            delete item;
    }

    void addItem(QLayoutItem *item) override { m_items.append(item); }
    int count() const override { return int(m_items.size()); }
    QLayoutItem *itemAt(int index) const override { return m_items.value(index); }
    QLayoutItem *takeAt(int index) override
    {
        return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override
    {
        QSize size;
        for (QLayoutItem *item : m_items)
            size = size.expandedTo(item->minimumSize());
        const QMargins m = contentsMargins();
        return size + QSize(m.left() + m.right(), m.top() + m.bottom());
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const
    {
        const QMargins m = contentsMargins();
        const QRect area = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = area.x();
        int y = area.y();
        int lineHeight = 0;
        for (QLayoutItem *item : m_items) {
            const QSize hint = item->sizeHint();
            int nextX = x + hint.width() + m_space;
            if (nextX - m_space > area.right() + 1 && lineHeight > 0) {
                x = area.x();
                y += lineHeight + m_space;
                nextX = x + hint.width() + m_space;
                lineHeight = 0;
            }
            if (!testOnly)
                item->setGeometry(QRect(QPoint(x, y), hint));
            x = nextX;
            lineHeight = qMax(lineHeight, hint.height());
        }
        return y + lineHeight - rect.y() + m.bottom();
    }

    QList<QLayoutItem *> m_items;
    int m_space;
};

} // namespace

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

void LayerProperties::rebuildLayer(Layer *layer)
{
    // Force a fresh build (setLayer caches per-layer widgets, so a tool whose
    // property set changes dynamically, e.g. the backdrop fill, must clear first).
    if (!layer)
        return;
    removeLayer(layer);
    setLayer(layer);
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

// Builds one labelled control (color / slider / dropdown / swatches) for a property.
// Shared by the layer and tool-template builders so new control types land once.
QLayout* LayerProperties::createPropertyControl(Tools::ITool *tool,
                                                const Tools::ToolProperty &prop,
                                                QWidget *parent)
{
    auto *propLayout = new QVBoxLayout();
    propLayout->setSpacing(4);

    auto *label = new QLabel(prop.name, parent);
    QFont labelFont = label->font();
    labelFont.setPointSize(labelFont.pointSize() - 1);
    label->setFont(labelFont);
    propLayout->addWidget(label);

    if (prop.controlType == "color") {
        auto *swatch = new QPushButton(parent);
        swatch->setFixedSize(32, 32);
        swatch->setToolTip("Click to change color");
        const QColor initialColor = prop.value.value<QColor>();
        const QString style =
            "QPushButton { background: %1; border: 1px solid palette(mid); border-radius: 6px; }"
            "QPushButton:hover { border-color: palette(dark); }";
        swatch->setStyleSheet(style.arg(initialColor.name()));
        connect(swatch, &QPushButton::clicked, this, [this, tool, swatch, style, p = prop]() {
            const QColor color = QColorDialog::getColor(p.value.value<QColor>(), this, "Select Color");
            if (color.isValid()) {
                emit propertyChangeRequested(tool, p.id, color);
                swatch->setStyleSheet(style.arg(color.name()));
            }
        });
        propLayout->addWidget(swatch);

    } else if (prop.controlType == "slider") {
        auto *sliderRow = new QHBoxLayout();
        auto *slider = new QSlider(Qt::Horizontal, parent);
        slider->setMinimum(prop.options.value("min").toInt());
        slider->setMaximum(prop.options.value("max").toInt());
        slider->setValue(prop.value.toInt());
        auto *valueLabel = new QLabel(QString::number(prop.value.toInt()), parent);
        valueLabel->setFixedWidth(28);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QFont valFont = valueLabel->font();
        valFont.setPointSize(valFont.pointSize() - 1);
        valueLabel->setFont(valFont);
        connect(slider, &QSlider::valueChanged, this, [this, tool, valueLabel, p = prop](int value) {
            emit propertyChangeRequested(tool, p.id, value);
            valueLabel->setText(QString::number(value));
        });
        sliderRow->addWidget(slider, 1);
        sliderRow->addWidget(valueLabel);
        propLayout->addLayout(sliderRow);

    } else if (prop.controlType == "dropdown") {
        auto *comboBox = new QComboBox(parent);
        comboBox->addItems(prop.options.value("items").toStringList());
        comboBox->setCurrentText(prop.value.toString());
        connect(comboBox, &QComboBox::currentTextChanged, this, [this, tool, p = prop](const QString& text) {
            emit propertyChangeRequested(tool, p.id, text);
        });
        propLayout->addWidget(comboBox);

    } else if (prop.controlType == "colorpair") {
        // Two color swatches on one row (e.g. gradient Start → End) to save height.
        const QString swatchStyle =
            "QPushButton { background: %1; border: 1px solid palette(mid); border-radius: 6px; }"
            "QPushButton:hover { border-color: palette(dark); }";
        auto makeSwatch = [this, tool, parent, swatchStyle](const QString &subId, const QString &tip,
                                                            const QColor &initial) {
            auto *sw = new QPushButton(parent);
            sw->setFixedSize(32, 32);
            sw->setToolTip(tip);
            sw->setStyleSheet(swatchStyle.arg(initial.name()));
            connect(sw, &QPushButton::clicked, this, [this, tool, sw, swatchStyle, subId, initial]() {
                const QColor c = QColorDialog::getColor(initial, this, "Select Color");
                if (c.isValid()) {
                    emit propertyChangeRequested(tool, subId, c);
                    sw->setStyleSheet(swatchStyle.arg(c.name()));
                }
            });
            return sw;
        };

        auto *row = new QHBoxLayout();
        row->setSpacing(8);
        row->addWidget(makeSwatch(prop.options.value("startId").toString(),
                                  prop.options.value("startName").toString(),
                                  prop.options.value("startValue").value<QColor>()));
        row->addWidget(new QLabel("→", parent));
        row->addWidget(makeSwatch(prop.options.value("endId").toString(),
                                  prop.options.value("endName").toString(),
                                  prop.options.value("endValue").value<QColor>()));
        row->addStretch();
        propLayout->addLayout(row);

    } else if (prop.controlType == "swatches") {
        // Grid of clickable preview tiles (e.g. gradient / wallpaper / presets).
        // Tiles reflow to fit the panel width (3 when narrow, 4+ when wider).
        const QStringList items = prop.options.value("items").toStringList();
        const QVariantList previews = prop.options.value("previews").toList();
        const QString current = prop.value.toString();

        auto *grid = new QWidget(parent);
        auto *flow = new FlowLayout(6);
        grid->setLayout(flow);
        // Report height-for-width so the enclosing layout allocates the wrapped
        // height (otherwise tiles get a single row's worth and overlap/clip).
        QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sp.setHeightForWidth(true);
        grid->setSizePolicy(sp);
        auto *group = new QButtonGroup(grid);
        group->setExclusive(true);

        const QString tileStyle =
            "QToolButton { border: 2px solid transparent; border-radius: 7px; padding: 2px; }"
            "QToolButton:hover { border-color: palette(mid); }"
            "QToolButton:checked { border-color: #0096ff; }";

        for (int i = 0; i < items.size(); ++i) {
            auto *btn = new QToolButton(grid);
            btn->setCheckable(true);
            btn->setToolTip(items[i]);
            btn->setStyleSheet(tileStyle);
            if (i < previews.size()) {
                const QPixmap pm = previews[i].value<QPixmap>();
                btn->setIcon(QIcon(pm));
                btn->setIconSize(pm.size());
            } else {
                btn->setText(items[i]);
            }
            btn->setChecked(items[i] == current);
            group->addButton(btn);
            const QString name = items[i];
            connect(btn, &QToolButton::clicked, this, [this, tool, p = prop, name]() {
                emit propertyChangeRequested(tool, p.id, name);
            });
            flow->addWidget(btn);
        }
        propLayout->addWidget(grid);
    }

    return propLayout;
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

    for (const auto& prop : tool->getProperties())
        layout->addLayout(createPropertyControl(tool, prop, propertiesWidget));

    // Backdrop layers get a "Save as preset…" action so the current config can be
    // stored without going back to the popover. Editor handles the save.
    if (layer->type() == Layer::Backdrop) {
        auto *saveBtn = new QPushButton(QStringLiteral("＋  Save as preset…"), propertiesWidget);
        saveBtn->setObjectName("savePresetBtn");
        saveBtn->setCursor(Qt::PointingHandCursor);
        connect(saveBtn, &QPushButton::clicked, this, &LayerProperties::savePresetRequested);
        layout->addWidget(saveBtn);
    }

    layout->addStretch();

    // Wrap in a scroll area so long property lists (e.g. the backdrop panel) scroll
    // instead of overflowing. The scroll area is what's tracked / shown / removed.
    auto *scroll = new QScrollArea();
    scroll->setObjectName("propertiesScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(propertiesWidget);

    m_stackedWidget->addWidget(scroll);
    m_layerWidgetMap[layer] = scroll;
    m_stackedWidget->setCurrentWidget(scroll);
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

    for (const auto& prop : tool->getProperties())
        layout->addLayout(createPropertyControl(tool, prop, propertiesWidget));

    layout->addStretch();

    auto *scroll = new QScrollArea();
    scroll->setObjectName("propertiesScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(propertiesWidget);

    m_toolWidget = scroll;
    m_stackedWidget->addWidget(scroll);
    m_stackedWidget->setCurrentWidget(scroll);
}

void LayerProperties::showPropertiesStyle()
{
    // Styling handled by editor.qss via #propertiesStack object name
}

void LayerProperties::hidePropertiesStyle()
{
    // Styling handled by editor.qss via #propertiesStack object name
}

} // namespace Editor
