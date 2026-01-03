#include "LayerManager.h"
#include "Layer.h"
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QTimer>

namespace ImageEditor {
    LayerManager::LayerManager(QWidget *parent) : QWidget(parent) {
        setMinimumWidth(180);
        setMinimumHeight(80);   // can shrink in the splitter, but never vanish
        setWindowTitle("Layers");

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        // Section header
        auto *header = new QLabel("Layers");
        header->setObjectName("sectionHeader");
        layout->addWidget(header);

        m_layerList = new QListWidget();
        m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
        // Let the list scroll and follow the splitter instead of growing to fit
        // every row (which would crowd out the Properties panel below).
        m_layerList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        m_layerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        connect(m_layerList, &QListWidget::itemSelectionChanged, this, &LayerManager::onItemSelectionChanged);
        layout->addWidget(m_layerList, 1);

        m_deleteButton = new QPushButton("Delete");
        m_deleteButton->setEnabled(false);
        connect(m_deleteButton, &QPushButton::clicked, this, &LayerManager::onDeleteButtonClicked);
        layout->addWidget(m_deleteButton);
    }

    void LayerManager::addLayer(Layer *layer) {
        m_layers.append(layer);
        updateLayerList();

        connect(layer, &Layer::visibilityChanged, [this, layer](bool visible) {
            emit layerVisibilityChanged(layer, visible);
        });

        emit layerAdded(layer);
    }

    void LayerManager::removeLayer(Layer *layer) {
        m_layers.removeAll(layer);
        updateLayerList();
    }

    void LayerManager::removeSelectedLayer() {
        Layer *selected = selectedLayer();
        if (selected && selected->type() != Layer::Background && selected->type() != Layer::Backdrop) {
            emit deleteLayerRequested(selected);
        }
    }

    Layer *LayerManager::selectedLayer() const {
        int currentRow = m_layerList->currentRow();
        if (currentRow >= 0 && currentRow < m_layers.size()) {
            return m_layers[m_layers.size() - 1 - currentRow];
        }
        return nullptr;
    }

    void LayerManager::selectLayer(Layer *layer) {
        if (!layer) {
            // Deselect all layers
            m_layerList->setCurrentRow(-1);
            return;
        }

        int layerIndex = m_layers.indexOf(layer);
        if (layerIndex >= 0) {
            int listIndex = m_layers.size() - 1 - layerIndex;
            m_layerList->setCurrentRow(listIndex);
        }
    }

    void LayerManager::onItemSelectionChanged() {
        Layer *selected = selectedLayer();
        m_deleteButton->setEnabled(selected && selected->type() != Layer::Background && selected->type() != Layer::Backdrop);
        if (selected) {
            emit layerSelected(selected);
        }
    }

    void LayerManager::onDeleteButtonClicked() {
        int currentRow = m_layerList->currentRow();
        Layer *selected = selectedLayer();

        if (selected && selected->type() != Layer::Background && selected->type() != Layer::Backdrop) {
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

    void LayerManager::updateLayerList() {
        m_layerList->clear();

        for (int i = m_layers.size() - 1; i >= 0; --i) {
            Layer *layer = m_layers[i];
            auto *item = new QListWidgetItem();

            auto *widget = new QWidget();
            widget->setStyleSheet("background: transparent;");
            auto *layout = new QHBoxLayout(widget);
            layout->setContentsMargins(4, 2, 4, 2);
            layout->setSpacing(6);

            // Visibility toggle styled as eye icon
            auto *visBtn = new QPushButton();
            visBtn->setFixedSize(20, 20);
            visBtn->setCheckable(true);
            visBtn->setChecked(layer->isVisible());
            visBtn->setToolTip("Toggle visibility");
            visBtn->setStyleSheet(
                "QPushButton { border: none; border-radius: 3px; font-size: 13px; background: transparent; }"
                "QPushButton:hover { background: palette(midlight); }"
            );
            visBtn->setText(layer->isVisible() ? "\xF0\x9F\x91\x81" : "\xE2\x80\x94");
            connect(visBtn, &QPushButton::toggled, [layer, visBtn](bool checked) {
                layer->setVisible(checked);
                visBtn->setText(checked ? "\xF0\x9F\x91\x81" : "\xE2\x80\x94");
            });

            QString displayName = layer->name();
            if (displayName.startsWith("Text: ")) {
                QString textContent = displayName.mid(6);
                if (textContent.length() > 12) {
                    displayName = "Text: " + textContent.left(12) + "...";
                }
            } else if (displayName.length() > 16 &&
                       !displayName.startsWith("Background") &&
                       !displayName.startsWith("Arrow ")) {
                displayName = displayName.left(16) + "...";
            }

            auto *label = new QLabel(displayName);
            label->setStyleSheet("background: transparent;");
            QFont font = label->font();
            font.setPointSize(font.pointSize() - 1);
            label->setFont(font);

            layout->addWidget(visBtn);
            layout->addWidget(label, 1);

            item->setSizeHint(QSize(0, 28));
            m_layerList->addItem(item);
            m_layerList->setItemWidget(item, widget);
        }
    }
} // namespace ImageEditor
