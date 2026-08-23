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
#include <QSpinBox>
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

const QString kSwatchStyle =
    QStringLiteral("QPushButton { background: %1; border: 1px solid palette(mid); border-radius: 6px; }"
                   "QPushButton:hover { border-color: palette(dark); }");

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
    mainLayout->addWidget(m_stackedWidget);

    m_emptyWidget = new QWidget();
    m_stackedWidget->addWidget(m_emptyWidget);

    setMinimumWidth(180);
    setWindowTitle("Properties");
}

void LayerProperties::setLayer(Layer *layer)
{
    if (QWidget *page = m_layerWidgetMap.value(layer))
        m_stackedWidget->setCurrentWidget(page);
    else if (auto *tool = layer ? dynamic_cast<Tools::ITool*>(layer->item()) : nullptr)
        m_layerWidgetMap[layer] = addPage(tool, layer);
    else
        m_stackedWidget->setCurrentWidget(m_emptyWidget);
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
        m_stackedWidget->setCurrentWidget(m_emptyWidget);
        return;
    }
    if (m_toolWidget) {
        m_stackedWidget->removeWidget(m_toolWidget);
        m_toolWidget->deleteLater();
    }
    m_toolWidget = addPage(tool, nullptr, toolName);
}

void LayerProperties::removeLayer(Layer *layer)
{
    if (m_layerWidgetMap.contains(layer)) {
        QWidget *widget = m_layerWidgetMap.take(layer);
        m_stackedWidget->removeWidget(widget);
        widget->deleteLater();
    }
}

// Builds one labelled control (color / slider / dropdown / swatches) for a property.
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
        propLayout->addWidget(createSwatch(tool, prop.id, "Click to change color",
                                           prop.value.value<QColor>(), parent));

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

    } else if (prop.controlType == "spinbox") {
        auto *spin = new QSpinBox(parent);
        spin->setMinimum(prop.options.value("min", 1).toInt());
        spin->setMaximum(prop.options.value("max", 999).toInt());
        spin->setValue(prop.value.toInt());
        spin->setKeyboardTracking(false);   // one change per committed edit, not per keystroke
        connect(spin, &QSpinBox::valueChanged, this, [this, tool, p = prop](int value) {
            emit propertyChangeRequested(tool, p.id, value);
        });
        propLayout->addWidget(spin);

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
        auto *row = new QHBoxLayout();
        row->setSpacing(8);
        row->addWidget(createSwatch(tool, prop.options.value("startId").toString(),
                                    prop.options.value("startName").toString(),
                                    prop.options.value("startValue").value<QColor>(), parent));
        row->addWidget(new QLabel("→", parent));
        row->addWidget(createSwatch(tool, prop.options.value("endId").toString(),
                                    prop.options.value("endName").toString(),
                                    prop.options.value("endValue").value<QColor>(), parent));
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

QPushButton *LayerProperties::createSwatch(Tools::ITool *tool, const QString &propertyId,
                                          const QString &tip, const QColor &initial,
                                          QWidget *parent)
{
    auto *swatch = new QPushButton(parent);
    swatch->setFixedSize(32, 32);
    swatch->setToolTip(tip);
    swatch->setStyleSheet(kSwatchStyle.arg(initial.name()));
    connect(swatch, &QPushButton::clicked, this, [this, tool, swatch, propertyId, initial]() {
        const QColor color = QColorDialog::getColor(initial, this, "Select Color");
        if (color.isValid()) {
            emit propertyChangeRequested(tool, propertyId, color);
            swatch->setStyleSheet(kSwatchStyle.arg(color.name()));
        }
    });
    return swatch;
}

QWidget *LayerProperties::addPage(Tools::ITool *tool, Layer *layer, const QString &title)
{
    auto* propertiesWidget = new QWidget();
    auto* layout = new QVBoxLayout(propertiesWidget);
    layout->setSpacing(12);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setAlignment(Qt::AlignTop);

    if (!layer) {
        auto* titleLabel = new QLabel(title);
        titleLabel->setObjectName("sectionHeader");
        titleLabel->setAlignment(Qt::AlignLeft);
        layout->addWidget(titleLabel);
    }

    for (const auto& prop : tool->getProperties())
        layout->addLayout(createPropertyControl(tool, prop, propertiesWidget));

    // Backdrop layers get a "Save as preset…" action so the current config can be
    // stored without going back to the popover. Editor handles the save.
    if (layer && layer->type() == Layer::Backdrop) {
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
    m_stackedWidget->setCurrentWidget(scroll);
    return scroll;
}

} // namespace Editor
