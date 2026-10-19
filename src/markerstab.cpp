#include "markerstab.h"
#include "mainwindow.h"
#include "cubiomes/util.h"

#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>

MarkerEditor::MarkerEditor(MarkerStore *store, SavedMarker marker, QWidget *parent) : QDialog(parent)
{
    setObjectName("markerEditor");
    setWindowTitle(marker.id.isEmpty() ? tr("Save marker") : tr("Edit marker"));
    auto *layout = new QVBoxLayout(this);
    auto *world = new QLabel(tr("Seed %1 | Java %2 | %3%4")
        .arg(QString::number((qint64)marker.world.seed), mc2str(marker.world.mc),
             SavedMarker::dimensionName(marker.dimension), marker.world.large ? tr(" | Large biomes") : QString()));
    world->setWordWrap(true);
    layout->addWidget(world);
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(marker.name);
    name->setObjectName("markerName");
    name->setMaxLength(200);
    auto *x = new QDoubleSpinBox;
    auto *z = new QDoubleSpinBox;
    x->setObjectName("markerX"); z->setObjectName("markerZ");
    for (auto *spin : {x, z})
    {
        spin->setRange(-30000000, 30000000);
        spin->setDecimals(1);
        spin->setReadOnly(marker.kind != D_NONE);
    }
    x->setValue(marker.x); z->setValue(marker.z);
    auto *notes = new QPlainTextEdit(marker.notes);
    notes->setObjectName("markerNotes");
    notes->setPlaceholderText(tr("Route, loot, portal connection, or anything to remember (up to 4000 characters)."));
    notes->setMaximumHeight(110);
    auto *done = new QCheckBox(tr("Completed / visited"));
    done->setObjectName("markerCompleted");
    done->setChecked(marker.completed);
    form->addRow(tr("Name"), name);
    form->addRow(tr("Type"), new QLabel(marker.typeName()));
    form->addRow(tr("X"), x); form->addRow(tr("Z"), z);
    form->addRow(tr("Notes"), notes); form->addRow(done);
    layout->addLayout(form);
    auto *error = new QLabel;
    error->setWordWrap(true);
    error->setStyleSheet("color: #d03030");
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->setObjectName("markerButtons");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [=]() mutable {
        marker.name = name->text(); marker.notes = notes->toPlainText();
        marker.x = x->value(); marker.z = z->value(); marker.completed = done->isChecked();
        if (store->put(marker)) accept(); else error->setText(store->lastError());
    });
    resize(430, sizeHint().height());
    name->selectAll();
}

class MarkerItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& other) const override
    {
        const int column = treeWidget()->sortColumn();
        if (column == 0) return checkState(0) < other.checkState(0);
        if (column == 4 || column == 5) return text(column).toDouble() < other.text(column).toDouble();
        return QTreeWidgetItem::operator<(other);
    }
};

MarkersTab::MarkersTab(MainWindow *window, MarkerStore *store)
    : QWidget(window), window(window), store(store)
{
    setObjectName("markersTab");
    auto *layout = new QVBoxLayout(this);
    auto *help = new QLabel(tr("Right-click a structure to save it or mark it completed. Right-click anywhere to add a marker. Changes save immediately."));
    help->setWordWrap(true); layout->addWidget(help);
    context = new QLabel; context->setWordWrap(true); layout->addWidget(context);
    search = new QLineEdit;
    search->setObjectName("markerSearch");
    search->setPlaceholderText(tr("Find a marker by name, type, coordinates, or notes"));
    layout->addWidget(search);
    auto *filters = new QHBoxLayout;
    status = new QComboBox; status->setObjectName("markerStatusFilter");
    status->addItems({tr("All progress"), tr("To do"), tr("Completed")});
    kind = new QComboBox; kind->setObjectName("markerTypeFilter");
    kind->addItem(tr("All types"), -2);
    for (int opt : {D_BASTION, D_FORTESS, D_STRONGHOLD, D_VILLAGE, D_NONE})
        kind->addItem(opt == D_NONE ? tr("Custom markers") : mapopt2display(opt), opt);
    dimension = new QComboBox; dimension->setObjectName("markerDimensionFilter");
    dimension->addItems({tr("Current dimension"), tr("All dimensions")});
    filters->addWidget(status); filters->addWidget(kind); filters->addWidget(dimension);
    layout->addLayout(filters);
    table = new QTreeWidget;
    table->setObjectName("markerTable");
    table->setColumnCount(6);
    table->setHeaderLabels({tr("Done"), tr("Name"), tr("Type"), tr("Dimension"), "X", "Z"});
    table->setRootIsDecorated(false);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSortingEnabled(true);
    table->sortByColumn(1, Qt::AscendingOrder);
    table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(table);
    summary = new QLabel; layout->addWidget(summary);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add at center..."));
    add->setObjectName("addSavedMarker");
    go = new QPushButton(tr("Go to")); go->setObjectName("goToSavedMarker");
    edit = new QPushButton(tr("Edit...")); edit->setObjectName("editSavedMarker");
    copy = new QPushButton(tr("Copy X Z")); copy->setObjectName("copySavedMarker");
    remove = new QPushButton(tr("Delete...")); remove->setObjectName("deleteSavedMarker");
    for (auto *button : {add, go, edit, copy, remove}) buttons->addWidget(button);
    layout->addLayout(buttons);
    hide = new QCheckBox(tr("Hide completed on map")); hide->setObjectName("hideCompletedMarkers");
    labels = new QCheckBox(tr("Show marker names")); labels->setObjectName("showMarkerLabels");
    auto *options = new QHBoxLayout; options->addWidget(hide); options->addWidget(labels);
    options->addStretch(); layout->addLayout(options);
    auto *backups = new QHBoxLayout;
    auto *exportButton = new QPushButton(tr("Export backup..."));
    auto *importButton = new QPushButton(tr("Import backup..."));
    backups->addWidget(exportButton); backups->addWidget(importButton); backups->addStretch();
    layout->addLayout(backups);
    storage = new QLabel; storage->setWordWrap(true); storage->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(storage);

    connect(search, &QLineEdit::textChanged, this, &MarkersTab::refresh);
    for (auto *combo : {status, kind, dimension})
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MarkersTab::refresh);
    connect(store, &MarkerStore::changed, this, &MarkersTab::refresh, Qt::QueuedConnection);
    connect(store, &MarkerStore::statusChanged, this, [=] {
        storage->setText(store->lastError().isEmpty() ? tr("Saved automatically to: %1").arg(store->filePath()) :
            tr("NOT SAVED: %1\nFile: %2").arg(store->lastError(), store->filePath()));
        storage->setStyleSheet(store->lastError().isEmpty() ? QString() : "color: #d03030");
    });
    connect(window->mapView, &MapView::worldChanged, this, &MarkersTab::refresh);
    connect(add, &QPushButton::clicked, window->mapView, &MapView::addMarkerAtCenter);
    connect(go, &QPushButton::clicked, this, &MarkersTab::goToSelected);
    connect(edit, &QPushButton::clicked, this, &MarkersTab::editSelected);
    connect(copy, &QPushButton::clicked, this, [=] {
        const SavedMarker marker = store->find(selectedId());
        if (!marker.id.isEmpty()) QApplication::clipboard()->setText(
            SavedMarker::coordinate(marker.x) + " " + SavedMarker::coordinate(marker.z));
    });
    connect(remove, &QPushButton::clicked, this, [=] {
        const SavedMarker marker = store->find(selectedId());
        if (!marker.id.isEmpty() && QMessageBox::question(this, tr("Delete marker"),
            tr("Delete '%1'? This removes its saved progress too.").arg(marker.name)) == QMessageBox::Yes)
            if (!store->remove(marker.id)) showSaveError();
    });
    connect(table, &QTreeWidget::itemSelectionChanged, this, [=] {
        const bool selected = !selectedId().isEmpty();
        for (auto *button : {go, edit, remove, copy}) button->setEnabled(selected);
    });
    connect(table, &QTreeWidget::itemActivated, this, [=](QTreeWidgetItem*, int) {
        goToSelected();
    });
    connect(table, &QTreeWidget::itemChanged, this, [=](QTreeWidgetItem *item, int column) {
        if (column == 0 && !store->setCompleted(item->data(0, Qt::UserRole).toString(), item->checkState(0) == Qt::Checked))
        { showSaveError(); refresh(); }
    });
    auto preferences = [=] {
        if (!store->setPreferences(hide->isChecked(), labels->isChecked())) { showSaveError(); refresh(); }
    };
    connect(hide, &QCheckBox::toggled, this, preferences);
    connect(labels, &QCheckBox::toggled, this, preferences);
    connect(exportButton, &QPushButton::clicked, this, [=] {
        const QString path = QFileDialog::getSaveFileName(this, tr("Back up all saved markers (all worlds)"),
            "cubiomes-markers-backup.json", tr("Marker backups (*.json)"));
        if (!path.isEmpty() && !store->exportBackup(path)) showSaveError();
    });
    connect(importButton, &QPushButton::clicked, this, [=] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import marker backup (keeps existing progress)"),
            QString(), tr("Marker backups (*.json)"));
        if (path.isEmpty()) return;
        int added;
        if (!store->importBackup(path, &added)) showSaveError();
        else QMessageBox::information(this, tr("Markers imported"), tr("Added %1 marker(s). Existing markers and progress were kept.").arg(added));
    });
    refresh();
    emit store->statusChanged();
}

QString MarkersTab::selectedId() const
{
    auto *item = table->currentItem();
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

void MarkersTab::showSaveError()
{
    QMessageBox::warning(this, tr("Marker changes were not saved"), store->lastError());
}

void MarkersTab::refresh()
{
    const QString selected = selectedId();
    const QSignalBlocker block(table), blockHide(hide), blockLabels(labels);
    hide->setChecked(store->hideCompleted()); labels->setChecked(store->showLabels());
    table->setSortingEnabled(false);
    table->clear();
    int total = 0, completed = 0;
    const QWorld *world = window->mapView->world;
    context->setText(world ? tr("Seed %1 | Java %2 | %3%4")
        .arg(QString::number((qint64)world->wi.seed), mc2str(world->wi.mc),
             SavedMarker::dimensionName(world->dim), world->wi.large ? tr(" | Large biomes") : QString()) : tr("No world selected"));
    for (const SavedMarker& marker : store->all())
    {
        if (!world || !marker.matchesWorld(world->wi)) continue;
        if (dimension->currentIndex() == 0 && marker.dimension != world->dim) continue;
        total++; completed += marker.completed;
        if ((status->currentIndex() == 1 && marker.completed) || (status->currentIndex() == 2 && !marker.completed)) continue;
        if (kind->currentData().toInt() != -2 && marker.kind != kind->currentData().toInt()) continue;
        const QString x = SavedMarker::coordinate(marker.x), z = SavedMarker::coordinate(marker.z);
        if (!(marker.name + " " + marker.typeName() + " " + marker.notes + " " + x + " " + z)
            .contains(search->text().trimmed(), Qt::CaseInsensitive)) continue;
        auto *item = new MarkerItem(table);
        item->setData(0, Qt::UserRole, marker.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, marker.completed ? Qt::Checked : Qt::Unchecked);
        item->setText(1, marker.name); item->setText(2, marker.typeName());
        item->setText(3, SavedMarker::dimensionName(marker.dimension));
        item->setText(4, x); item->setText(5, z);
        const QString tooltip = marker.name + "\n" + (marker.completed ? tr("Completed") : tr("To do")) +
            "\nX: " + x + "  Z: " + z + (marker.notes.isEmpty() ? QString() : "\n" + marker.notes);
        for (int col = 0; col < 6; col++) item->setToolTip(col, tooltip);
        if (marker.completed)
        {
            QFont font = item->font(1); font.setStrikeOut(true); item->setFont(1, font);
        }
        if (marker.id == selected) table->setCurrentItem(item);
    }
    table->setSortingEnabled(true);
    summary->setText(tr("%1 shown | %2 saved | %3 completed | %4 to do")
        .arg(table->topLevelItemCount()).arg(total).arg(completed).arg(total-completed));
    for (auto *button : {go, edit, remove, copy}) button->setEnabled(!selectedId().isEmpty());
}

void MarkersTab::goToSelected()
{
    const SavedMarker marker = store->find(selectedId());
    if (marker.id.isEmpty()) return;
    WorldInfo wi = marker.world;
    if (window->mapView->world) wi.y = window->mapView->world->wi.y;
    window->setSeed(wi, marker.dimension);
    window->mapView->focusSavedMarker(marker.id);
}

void MarkersTab::editSelected()
{
    const SavedMarker marker = store->find(selectedId());
    if (marker.id.isEmpty()) return;
    auto *dialog = new MarkerEditor(store, marker, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MarkersTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    store->reload();
    refresh();
}
