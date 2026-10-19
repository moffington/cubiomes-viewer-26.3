// Opt-in offline integration checks; all storage paths are supplied by selftest.
#include "mainwindow.h"
#include "markerstab.h"
#include "gotodialog.h"
#include "ui_mainwindow.h"
#include "cubiomes/util.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLineEdit>
#include <QLockFile>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <cstdio>

namespace {
int failures;
void check(bool passed, const char *name)
{
    printf("%s %s\n", passed ? "PASS" : "FAIL", name);
    fflush(stdout);
    failures += !passed;
}
QByteArray bytes(const QString& path)
{
    QFile file(path); file.open(QIODevice::ReadOnly); return file.readAll();
}
void write(const QString& path, const QByteArray& data)
{
    QFile file(path); file.open(QIODevice::WriteOnly); file.write(data);
}
void storageTests(const QString& folder)
{
    const QString path = folder + "/saved-markers.json";
    MarkerStore store(path);
    SavedMarker marker;
    marker.world.seed = UINT64_C(9223372036854775813); // Exercises signed seeds beyond JSON's integer precision.
    marker.x = -1234.5; marker.z = 6789.5; marker.name = "Home island";
    MarkerEditor editor(&store, marker);
    editor.findChild<QLineEdit*>("markerName")->setText("Home base");
    editor.findChild<QPlainTextEdit*>("markerNotes")->setPlainText(QString::fromUtf8("Route → blaze rods"));
    editor.findChild<QDialogButtonBox*>("markerButtons")->button(QDialogButtonBox::Save)->click();
    check(editor.result() == QDialog::Accepted && store.all().size() == 1 && !editor.isVisible(),
          "saved marker editor commits without showing a window");
    if (store.all().isEmpty()) return;
    marker = store.all().front();
    MarkerStore reopened(path);
    const SavedMarker restored = reopened.find(marker.id);
    check(restored.world.seed == marker.world.seed && restored.x == -1234.5 && restored.z == 6789.5 &&
          restored.name == "Home base" && restored.notes == QString::fromUtf8("Route → blaze rods"),
          "restart restores exact 64-bit seed, coordinates, name and Unicode notes");
    check(store.setCompleted(marker.id, true) && reopened.reload() && reopened.find(marker.id).completed,
          "completion status persists across store instances");
    SavedMarker second = marker; second.id.clear(); second.name = "Second"; second.x += 100;
    SavedMarker third = marker; third.id.clear(); third.name = "Third"; third.x += 200;
    check(store.put(second) && reopened.put(third) && store.reload() && store.all().size() == 3 &&
          store.find(marker.id).completed, "two viewer instances merge independent edits without losing progress");
    SavedMarker duplicate = marker; duplicate.id.clear(); duplicate.name = "Do not replace existing notes";
    check(store.put(duplicate) && store.all().size() == 3 && duplicate.id == marker.id &&
          duplicate.name == "Home base" && duplicate.completed, "saving the same marker twice preserves its name, notes and completion");
    check(store.setPreferences(true, true) && reopened.reload() && reopened.hideCompleted() && reopened.showLabels(),
          "map visibility preferences persist");
    const QString backup = folder + "/backup.json";
    MarkerStore imported(folder + "/imported.json");
    int added = -1;
    bool backupOk = store.exportBackup(backup) && imported.importBackup(backup, &added) && added == 3 &&
        imported.find(marker.id).completed && imported.find(marker.id).notes == marker.notes;
    imported.setCompleted(marker.id, false);
    backupOk &= imported.importBackup(backup, &added) && added == 0 && !imported.find(marker.id).completed;
    check(backupOk, "backup/import round trip merges missing markers and keeps existing progress");
    const QByteArray before = bytes(path);
    const QString corrupt = folder + "/broken.json";
    write(corrupt, "{broken");
    check(!store.importBackup(corrupt) && bytes(path) == before,
          "invalid backup cannot partially import or overwrite saved markers");
    MarkerStore damaged(corrupt);
    check(!damaged.lastError().isEmpty() && !damaged.put(second) && bytes(corrupt) == "{broken",
          "damaged marker file is reported and never silently overwritten");
    QLockFile lock(path + ".lock");
    const bool locked = lock.tryLock(0);
    check(locked && !store.setCompleted(marker.id, false) && bytes(path) == before,
          "busy marker file refuses a conflicting save without data loss");
    lock.unlock();
    write(folder + "/not-a-directory", "occupied");
    MarkerStore unwritable(folder + "/not-a-directory/markers.json");
    check(!unwritable.put(second) && unwritable.all().isEmpty(),
          "failed storage write is surfaced and does not claim to save a marker");
    check(store.remove(third.id) && reopened.reload() && reopened.find(third.id).id.isEmpty() && reopened.all().size() == 2,
          "marker deletion persists after reopening");
    MarkerEditor cancelled(&store, marker);
    cancelled.findChild<QLineEdit*>("markerName")->setText("Cancelled edit");
    cancelled.findChild<QDialogButtonBox*>("markerButtons")->button(QDialogButtonBox::Cancel)->click();
    check(store.find(marker.id).name == "Home base", "cancelled marker edits do not change saved data");
}

QImage render(MapView& map)
{
    map.grab(); map.world->waitForIdle();
    QImage image = map.grab().toImage(); map.world->waitForIdle();
    return image;
}

bool nearColor(const QImage& image, const QPoint& center, const QColor& color)
{
    const qreal ratio = image.devicePixelRatio();
    for (int y = -12; y <= 12; y++) for (int x = -12; x <= 12; x++)
    {
        const QPoint pixel(qRound((center.x()+x)*ratio), qRound((center.y()+y)*ratio));
        if (image.rect().contains(pixel) && image.pixelColor(pixel) == color) return true;
    }
    return false;
}

QTreeWidgetItem *findItem(QTreeWidget *table, const QString& id)
{
    for (int i = 0; i < table->topLevelItemCount(); i++)
        if (table->topLevelItem(i)->data(0, Qt::UserRole) == id) return table->topLevelItem(i);
    return nullptr;
}

void interfaceTests(MainWindow& window)
{
    WorldInfo wi; wi.seed = 424242;
    window.setSeed(wi, DIM_OVERWORLD);
    MapView& map = *window.mapView;
    map.resize(384, 256);
    GotoDialog goTo(&map, 0, 0, 4);
    goTo.findChild<QLineEdit*>("lineX")->setText("-256.5");
    goTo.findChild<QLineEdit*>("lineZ")->setText("768.5");
    goTo.findChild<QCheckBox*>("checkAnimate")->setChecked(false);
    goTo.findChild<QCheckBox*>("checkSaveMarker")->setChecked(true);
    goTo.findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Apply)->click();
    MarkerStore *store = window.markerStore;
    check(store->all().size() == 1 && store->all().front().x == -256.5 &&
          store->all().front().z == 768.5, "Go to Save marker checkbox persists the exact destination");
    if (store->all().isEmpty()) return;
    const QString id = store->all().front().id;
    goTo.findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Apply)->click();
    check(store->all().size() == 1, "repeated Go to Apply does not duplicate a saved marker");
    window.markersTab->refresh();
    auto *table = window.markersTab->findChild<QTreeWidget*>("markerTable");
    auto *item = findItem(table, id);
    if (item) item->setCheckState(0, Qt::Checked);
    MarkerStore reopened(store->filePath());
    check(item && reopened.find(id).completed, "Markers list checkbox saves completion immediately");
    auto *status = window.markersTab->findChild<QComboBox*>("markerStatusFilter");
    auto *type = window.markersTab->findChild<QComboBox*>("markerTypeFilter");
    auto *search = window.markersTab->findChild<QLineEdit*>("markerSearch");
    status->setCurrentIndex(1); bool filters = table->topLevelItemCount() == 0;
    status->setCurrentIndex(2); filters &= table->topLevelItemCount() == 1;
    search->setText("missing marker"); filters &= table->topLevelItemCount() == 0;
    search->clear(); filters &= table->topLevelItemCount() == 1;
    type->setCurrentIndex(type->findData(D_VILLAGE)); filters &= table->topLevelItemCount() == 0;
    type->setCurrentIndex(0); status->setCurrentIndex(0);
    check(filters, "Markers list filters progress, type and search text");
    item = findItem(table, id);
    if (item)
    {
        table->setCurrentItem(item);
        map.setView(0, 0, 4);
        QMetaObject::invokeMethod(table, "itemActivated", Qt::DirectConnection,
                                 Q_ARG(QTreeWidgetItem*, item), Q_ARG(int, 0));
    }
    check(item && map.getX() == -256.5 && map.getZ() == 768.5,
          "keyboard activation from the Done column returns to the saved marker");

    map.clearCoordinateMarker();
    render(map); // A hidden main-window layout settles its map size on first render.
    const QPoint center(map.width()/2, map.height()/2);
    const QColor green(36, 158, 82);
    bool colors = nearColor(render(map), center, green);
    store->setPreferences(true, false);
    colors &= !nearColor(render(map), center, green);
    store->setPreferences(false, false);
    colors &= nearColor(render(map), center, green);
    check(colors, "completed saved marker renders a green check and can be hidden/restored");
    {
        QMenu *menu = map.createContextMenu(center);
        QAction *action = menu->findChild<QAction*>("toggleSavedCompleted");
        if (action) action->trigger();
        check(action && !store->find(id).completed, "right-click saved marker reopens completed progress");
        delete menu;
    }
    SavedMarker nether; nether.world = wi; nether.dimension = DIM_NETHER;
    nether.name = "Portal route"; nether.x = -400; nether.z = 1000;
    store->put(nether);
    window.markersTab->refresh();
    auto *dimensions = window.markersTab->findChild<QComboBox*>("markerDimensionFilter");
    const bool currentOnly = table->topLevelItemCount() == 1;
    dimensions->setCurrentIndex(1);
    item = findItem(table, nether.id);
    if (item) { table->setCurrentItem(item); window.markersTab->goToSelected(); }
    check(currentOnly && item && map.world->dim == DIM_NETHER && map.getX() == -400 && map.getZ() == 1000,
          "all-dimensions marker list jumps to the correct dimension and coordinates");
    WorldInfo other = wi; other.seed++;
    window.setSeed(other, DIM_NETHER); bool isolated = table->topLevelItemCount() == 0;
    other = wi; other.mc = MC_1_21_11;
    window.setSeed(other, DIM_NETHER); isolated &= table->topLevelItemCount() == 0;
    other = wi; other.large = true;
    window.setSeed(other, DIM_NETHER); isolated &= table->topLevelItemCount() == 0;
    window.setSeed(wi, DIM_NETHER); isolated &= table->topLevelItemCount() == 2;
    check(isolated && store->all().size() == 2,
          "saved markers stay separate by seed, version and world type without being deleted");
    auto *netherAction = window.findChild<QAction*>("actionNetherTargets");
    auto *overworldAction = window.findChild<QAction*>("actionOverworldTargets");
    netherAction->trigger();
    bool presets = map.world->dim == DIM_NETHER && map.getShow(D_BASTION) && map.getShow(D_FORTESS) && !map.getShow(D_VILLAGE);
    overworldAction->trigger();
    presets &= map.world->dim == DIM_OVERWORLD && map.getShow(D_VILLAGE) && map.getShow(D_STRONGHOLD) && !map.getShow(D_BASTION);
    check(presets, "exploration presets switch dimensions and enable the intended structure overlays");
    window.findChild<QAction*>("actionSavedMarkers")->trigger();
    check(window.ui->tabContainer->currentWidget() == window.markersTab && !window.isVisible(),
          "saved markers menu opens the correct tab without displaying a test window");

    const QString preview = qEnvironmentVariable("CUBIOMES_SELFTEST_WORKFLOW_PREVIEW");
    if (!preview.isEmpty())
    {
        SavedMarker home = store->find(id); home.name = "Home base"; home.completed = true;
        home.notes = "Dock on the east shore"; store->put(home);
        store->setPreferences(false, true);
        window.resize(1280, 800);
        window.markersTab->refresh();
        map.focusSavedMarker(id);
        window.grab(); map.world->waitForIdle(); window.grab().save(preview);
    }
    map.deleteWorld();
}

bool fixturePosition(int kind, const WorldInfo& wi, int dimension, Pos& position)
{
    if (kind == D_STRONGHOLD)
    {
        Generator generator; setupGenerator(&generator, wi.mc, 0);
        applySeed(&generator, DIM_OVERWORLD, wi.seed);
        StrongholdIter iter; initFirstStronghold(&iter, wi.mc, wi.seed);
        nextStronghold(&iter, &generator);
        position = iter.pos;
        return true;
    }
    StructureConfig config;
    if (!getStructureConfig(mapopt2stype(kind), wi.mc, &config)) return false;
    for (int rz = -3; rz <= 3; rz++) for (int rx = -3; rx <= 3; rx++)
    {
        Pos candidate;
        if (!getStructurePos(mapopt2stype(kind), wi.mc, wi.seed, rx, rz, &candidate)) continue;
        std::vector<VarPos> positions;
        getStructs(&positions, config, wi, dimension, candidate.x, candidate.z, candidate.x+1, candidate.z+1);
        if (!positions.empty()) { position = positions.front().p; return true; }
    }
    return false;
}

void structureTests(const QString& folder)
{
    MarkerStore store(folder + "/structure-progress.json");
    MapView map;
    Config config; config.mapThreads = 1; config.showBBoxes = false;
    map.setConfig(config); map.setMarkerStore(&store); map.resize(384, 256);
    const QPoint center(map.width()/2, map.height()/2);
    const int kinds[] = {D_VILLAGE, D_BASTION, D_FORTESS, D_STRONGHOLD};
    const char *names[] = {
        "Village: generated icon -> save -> complete -> hide -> restart",
        "Bastion: generated icon -> save -> complete -> hide -> restart",
        "Fortress: generated icon -> save -> complete -> hide -> restart",
        "Stronghold: generated icon -> save -> complete -> hide -> restart"};
    for (int i = 0; i < 4; i++)
    {
        WorldInfo wi; wi.seed = 12345 + kinds[i];
        const int dim = (kinds[i] == D_BASTION || kinds[i] == D_FORTESS) ? DIM_NETHER : DIM_OVERWORLD;
        Pos position;
        bool passed = fixturePosition(kinds[i], wi, dim, position);
        if (!passed) { check(false, names[i]); continue; }
        for (int opt = 0; opt < D_STRUCT_NUM; opt++) map.setShow(opt, opt == kinds[i]);
        map.setSeed(wi, dim, LayerOpt());
        map.setView(position.x, position.z, 4);
        render(map);
        QMenu *menu = map.createContextMenu(center);
        QAction *save = menu->findChild<QAction*>("saveStructureMarker");
        passed &= save && map.world->selopt == kinds[i];
        if (save) save->trigger();
        delete menu;
        SavedMarker expected; expected.world = wi; expected.dimension = dim;
        expected.kind = kinds[i]; expected.x = position.x; expected.z = position.z;
        SavedMarker saved = store.findPlace(expected);
        passed &= !saved.id.isEmpty() && !saved.completed;
        menu = map.createContextMenu(center);
        QAction *complete = menu->findChild<QAction*>("toggleSavedCompleted");
        if (complete) complete->trigger();
        passed &= complete && store.find(saved.id).completed;
        delete menu;
        passed &= nearColor(render(map), center + QPoint(8, 8), QColor(36, 158, 82));
        store.setPreferences(true, false);
        passed &= map.world->hiddenStructures.contains(SavedMarker::structureKey(kinds[i], position.x, position.z));
        menu = map.createContextMenu(center);
        passed &= map.world->selopt == D_NONE && !menu->findChild<QAction*>("toggleSavedCompleted");
        delete menu;
        MarkerStore reopened(store.filePath());
        passed &= reopened.find(saved.id).completed && reopened.find(saved.id).kind == kinds[i] &&
            reopened.find(saved.id).dimension == dim;
        store.setPreferences(false, false);
        passed &= nearColor(render(map), center + QPoint(8, 8), QColor(36, 158, 82));
        if (!passed) printf("Fixture kind=%d seed=%llu pos=%d,%d selected=%d saved=%d\n",
            kinds[i], (unsigned long long)wi.seed, position.x, position.z, map.world->selopt, !saved.id.isEmpty());
        check(passed, names[i]);
    }
    map.deleteWorld();
}

void savedMapTests(const QString& folder)
{
    const QString path = folder + "/map-markers.json";
    {
        MarkerStore first(path);
        SavedMarker marker; marker.world.seed = 8675309;
        marker.x = -500; marker.z = 700; marker.name = "Saved map anchor";
        first.put(marker);
    }
    MarkerStore reopened(path);
    MapView map; Config config; config.mapThreads = 1;
    map.setConfig(config); map.setMarkerStore(&reopened); map.resize(384, 256);
    WorldInfo wi; wi.seed = 8675309;
    map.setSeed(wi, DIM_OVERWORLD, LayerOpt());
    map.setView(-500, 700, 4);
    const QColor blue(40, 135, 235);
    bool anchored = nearColor(render(map), QPoint(192,128), blue);
    map.setView(-500+192, 700-128, 4);
    anchored &= nearColor(render(map), QPoint(144,160), blue);
    map.zoom(2);
    anchored &= nearColor(render(map), QPoint(96,192), blue);
    map.resize(512, 288);
    anchored &= nearColor(render(map), QPoint(160,208), blue);
    check(anchored, "reopened persistent marker stays anchored through pan, zoom and resize");
    map.setView(-500, 700, 4);
    map.setSeed(wi, DIM_NETHER, LayerOpt());
    bool isolated = !nearColor(render(map), QPoint(256,144), blue);
    map.setSeed(wi, DIM_OVERWORLD, LayerOpt());
    isolated &= nearColor(render(map), QPoint(256,144), blue);
    wi.seed++;
    map.setSeed(wi, DIM_OVERWORLD, LayerOpt());
    isolated &= !nearColor(render(map), QPoint(256,144), blue);
    check(isolated && reopened.all().size() == 1, "persistent map markers hide in other worlds/dimensions and return without data loss");
    map.deleteWorld();
}
}

int runSavedMarkerTests(MainWindow& window, const QString& folder)
{
    failures = 0;
    storageTests(folder);
    interfaceTests(window);
    structureTests(folder);
    savedMapTests(folder);
    return failures;
}
