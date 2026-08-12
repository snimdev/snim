#include "LayerManager.h"
#include "Layer.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QHBoxLayout>
#include <QTreeWidgetItem>
#include <QFont>

namespace Editor {

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

    // Tree (rather than a flat list) so groups show as expandable nodes.
    m_tree = new QTreeWidget();
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);   // multi-select for grouping
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &LayerManager::onItemSelectionChanged);
    layout->addWidget(m_tree, 1);

    // Action row: Group / Ungroup / Delete.
    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(6);
    m_groupButton = new QPushButton("Group");
    m_groupButton->setEnabled(false);
    m_groupButton->setToolTip("Group the selected layers");
    connect(m_groupButton, &QPushButton::clicked, this, &LayerManager::onGroupButtonClicked);
    buttons->addWidget(m_groupButton);

    m_ungroupButton = new QPushButton("Ungroup");
    m_ungroupButton->setEnabled(false);
    m_ungroupButton->setToolTip("Dissolve the selected group");
    connect(m_ungroupButton, &QPushButton::clicked, this, &LayerManager::onUngroupButtonClicked);
    buttons->addWidget(m_ungroupButton);

    m_deleteButton = new QPushButton("Delete");
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &LayerManager::onDeleteButtonClicked);
    buttons->addWidget(m_deleteButton);
    layout->addLayout(buttons);
}

void LayerManager::addLayer(Layer *layer) {
    m_layers.append(layer);
    updateLayerList();

    // Avoid stacking duplicate connections if this layer is re-added (undo/redo of
    // a delete or group operation re-inserts the same Layer object).
    disconnect(layer, &Layer::visibilityChanged, this, nullptr);
    connect(layer, &Layer::visibilityChanged, this, [this, layer](bool visible) {
        emit layerVisibilityChanged(layer, visible);
    });

    emit layerAdded(layer);
}

void LayerManager::removeLayer(Layer *layer) {
    if (!m_layers.removeOne(layer)) {
        // Not a top-level node: detach it from whichever group contains it (e.g.
        // deleting a single layer that lives inside a group).
        for (Layer *top : m_layers)
            if (top->isGroup() && top->children().contains(layer)) {
                top->removeChild(layer);
                break;
            }
    }
    updateLayerList();
}

Layer* LayerManager::layerForItem(QTreeWidgetItem *item) const {
    return item ? m_itemToLayer.value(item, nullptr) : nullptr;
}

Layer *LayerManager::selectedLayer() const {
    return layerForItem(m_tree->currentItem());
}

QList<Layer*> LayerManager::selectedLayers() const {
    QList<Layer*> result;
    for (QTreeWidgetItem *item : m_tree->selectedItems())
        if (Layer *l = layerForItem(item))
            result.append(l);
    return result;
}

void LayerManager::selectLayer(Layer *layer) {
    if (!layer) {
        m_tree->setCurrentItem(nullptr);
        m_tree->clearSelection();
        return;
    }
    for (auto it = m_itemToLayer.cbegin(); it != m_itemToLayer.cend(); ++it) {
        if (it.value() == layer) {
            m_tree->setCurrentItem(it.key());
            return;
        }
    }
}

void LayerManager::onItemSelectionChanged() {
    Layer *current = selectedLayer();
    const QList<Layer*> selected = selectedLayers();

    // Group: two or more selected, all of them top-level editable layers.
    int groupable = 0;
    for (Layer *l : selected)
        if (l && l->isEditable() && m_layers.contains(l)) ++groupable;
    m_groupButton->setEnabled(groupable >= 2 && groupable == selected.size());

    // Ungroup: exactly one group selected.
    m_ungroupButton->setEnabled(selected.size() == 1 && current && current->isGroup());

    // Delete: a single editable layer (groups are removed via Ungroup).
    m_deleteButton->setEnabled(current && current->isEditable());

    if (current)
        emit layerSelected(current);
}

void LayerManager::onDeleteButtonClicked() {
    deleteCurrentLayer();
}

void LayerManager::deleteCurrentLayer() {
    Layer *selected = selectedLayer();
    if (!selected || !selected->isEditable())
        return;

    // Pick a neighbour in the visible tree order to select after the deletion.
    QTreeWidgetItem *cur = m_tree->currentItem();
    QTreeWidgetItem *neighbourItem = m_tree->itemBelow(cur);
    if (!neighbourItem)
        neighbourItem = m_tree->itemAbove(cur);
    Layer *neighbour = neighbourItem ? layerForItem(neighbourItem) : nullptr;

    emit deleteLayerRequested(selected);   // Editor pushes RemoveLayerCommand + rebuilds the tree
    if (neighbour)
        selectLayer(neighbour);            // neighbour Layer* survives the rebuild
}

void LayerManager::onGroupButtonClicked() {
    QList<Layer*> members;
    for (Layer *l : selectedLayers())
        if (l && l->isEditable() && m_layers.contains(l))
            members.append(l);
    if (members.size() >= 2)
        emit groupRequested(members);
}

void LayerManager::onUngroupButtonClicked() {
    Layer *selected = selectedLayer();
    if (selected && selected->isGroup())
        emit ungroupRequested(selected);
}

void LayerManager::addRow(QTreeWidgetItem *parentItem, Layer *layer) {
    auto *item = parentItem ? new QTreeWidgetItem(parentItem)
                            : new QTreeWidgetItem(m_tree);
    m_itemToLayer.insert(item, layer);

    auto *widget = new QWidget();
    widget->setStyleSheet("background: transparent;");
    auto *row = new QHBoxLayout(widget);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(6);

    // Visibility toggle styled as an eye icon.
    auto *visBtn = new QPushButton();
    visBtn->setFixedSize(20, 20);
    visBtn->setCheckable(true);
    visBtn->setChecked(layer->isVisible());
    visBtn->setToolTip("Toggle visibility");
    visBtn->setStyleSheet(
        "QPushButton { border: none; border-radius: 3px; font-size: 13px; background: transparent; }"
        "QPushButton:hover { background: palette(midlight); }");
    visBtn->setText(layer->isVisible() ? "\xF0\x9F\x91\x81" : "\xE2\x80\x94");
    connect(visBtn, &QPushButton::toggled, this, [this, layer, visBtn](bool checked) {
        visBtn->setText(checked ? "\xF0\x9F\x91\x81" : "\xE2\x80\x94");
        // Route through Editor so the toggle is undoable; the command flips
        // the layer (don't call setVisible here, to avoid doing it twice).
        emit visibilityToggleRequested(layer, checked);
    });

    QString displayName = layer->name();
    if (layer->isGroup()) {
        displayName = "\xF0\x9F\x93\x81 " + displayName;   // 📁 folder glyph
    } else if (displayName.startsWith("Text: ")) {
        QString textContent = displayName.mid(6);
        if (textContent.length() > 12)
            displayName = "Text: " + textContent.left(12) + "...";
    } else if (displayName.length() > 16 &&
               !displayName.startsWith("Background") &&
               !displayName.startsWith("Arrow ")) {
        displayName = displayName.left(16) + "...";
    }

    auto *label = new QLabel(displayName);
    label->setStyleSheet("background: transparent;");
    QFont font = label->font();
    font.setPointSize(font.pointSize() - 1);
    if (layer->isGroup())
        font.setBold(true);
    label->setFont(font);

    row->addWidget(visBtn);
    row->addWidget(label, 1);

    item->setSizeHint(0, QSize(0, 28));
    m_tree->setItemWidget(item, 0, widget);

    // Render a group's children beneath it.
    if (layer->isGroup()) {
        const QList<Layer*> kids = layer->children();
        for (int i = kids.size() - 1; i >= 0; --i)
            addRow(item, kids[i]);
        item->setExpanded(true);
    }
}

void LayerManager::updateLayerList() {
    m_tree->clear();          // deletes items + their row widgets
    m_itemToLayer.clear();

    // Top of the list == top of the z-order, matching the previous flat list.
    for (int i = m_layers.size() - 1; i >= 0; --i)
        addRow(nullptr, m_layers[i]);
}

} // namespace Editor
