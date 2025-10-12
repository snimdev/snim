#include "LayerManager.h"
#include "Layer.h"
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QTimer>

LayerManager::LayerManager(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(200);
    setWindowTitle("Layers");

    auto *layout = new QVBoxLayout(this);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setMaximumHeight(300);

    m_layerList = new QListWidget();
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    connect(m_layerList, &QListWidget::itemSelectionChanged,
            this, &LayerManager::onItemSelectionChanged);

    scrollArea->setWidget(m_layerList);
    layout->addWidget(scrollArea);

    m_deleteButton = new QPushButton("Delete Layer");
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &LayerManager::onDeleteButtonClicked);
    layout->addWidget(m_deleteButton);
}

void LayerManager::addLayer(Layer *layer)
{
    m_layers.append(layer);
    updateLayerList();

    connect(layer, &Layer::visibilityChanged, [this, layer](bool visible) {
        emit layerVisibilityChanged(layer, visible);
    });
}

void LayerManager::removeLayer(Layer *layer)
{
    m_layers.removeAll(layer);
    updateLayerList();
}

void LayerManager::removeSelectedLayer()
{
    Layer *selected = selectedLayer();
    if (selected && selected->type() != Layer::Background) {
        emit deleteLayerRequested(selected);
    }
}

Layer* LayerManager::selectedLayer() const
{
    int currentRow = m_layerList->currentRow();
    if (currentRow >= 0 && currentRow < m_layers.size()) {
        return m_layers[m_layers.size() - 1 - currentRow];
    }
    return nullptr;
}

void LayerManager::selectLayer(Layer *layer)
{
    if (!layer) return;

    int layerIndex = m_layers.indexOf(layer);
    if (layerIndex >= 0) {
        int listIndex = m_layers.size() - 1 - layerIndex;
        m_layerList->setCurrentRow(listIndex);
    }
}

void LayerManager::onItemSelectionChanged()
{
    Layer *selected = selectedLayer();
    m_deleteButton->setEnabled(selected && selected->type() != Layer::Background);
    if (selected) {
        emit layerSelected(selected);
    }
}

void LayerManager::onDeleteButtonClicked()
{
    int currentRow = m_layerList->currentRow();
    Layer *selected = selectedLayer();

    if (selected && selected->type() != Layer::Background) {
        emit deleteLayerRequested(selected);

        int newRow = currentRow;
        if (newRow >= m_layerList->count() - 1) {
            newRow = m_layerList->count() - 2;
        }

        if (newRow >= 0 && newRow < m_layerList->count()) {
            QTimer::singleShot(50, [this, newRow]() {
                if (newRow < m_layerList->count()) {
                    m_layerList->setCurrentRow(newRow);
                }
            });
        }
    }
}

void LayerManager::updateLayerList()
{
    m_layerList->clear();

    for (int i = m_layers.size() - 1; i >= 0; --i) {
        Layer *layer = m_layers[i];
        auto *item = new QListWidgetItem();

        auto *widget = new QWidget();
        auto *layout = new QHBoxLayout(widget);
        layout->setContentsMargins(2, 2, 2, 2);

        auto *checkbox = new QCheckBox();
        checkbox->setChecked(layer->isVisible());
        connect(checkbox, &QCheckBox::toggled, [layer](bool checked) {
            layer->setVisible(checked);
        });

        QString displayName = layer->name();

        if (displayName.startsWith("Text: ")) {
            QString textContent = displayName.mid(6);
            if (textContent.length() > 8) {
                displayName = "Text: " + textContent.left(8) + "...";
            }
        }
        else if (displayName.length() > 8 &&
                 !displayName.startsWith("Background") &&
                 !displayName.startsWith("Arrow ")) {
            displayName = displayName.left(8) + "...";
        }

        auto *label = new QLabel(displayName);

        layout->addWidget(checkbox);
        layout->addWidget(label);
        layout->addStretch();

        item->setSizeHint(widget->sizeHint());
        m_layerList->addItem(item);
        m_layerList->setItemWidget(item, widget);
    }
}
