// Exercise seed editing and version selection using public/reference seed inputs.
// Link with the viewer objects except main.o/selftest.o, plus Qt Core/Gui/Widgets.
#include "src/mainwindow.h"
#include "src/util.h"
#include "ui_mainwindow.h"
#include <QTemporaryDir>
#include <cstdio>

extern "C" int getStructureConfig_override(int type, int mc, StructureConfig *config)
{
    return getStructureConfig_default(type, mc, config);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir temp(QCoreApplication::applicationDirPath() + "/regression-XXXXXX");
    if (!temp.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temp.path());
    initBiomeColors(g_biomeColors);
    initBiomeTypeColors(g_tempsColors);
    setStructureConfigProvider(getStructureConfig_override);
    MainWindow window(temp.path() + "/session.save", "", nullptr, temp.path() + "/markers.json");
    window.autosaveTimer.stop();
    Config config = window.config;
    config.mapThreads = 1;
    window.mapView->setConfig(config);
    WorldInfo wi;
    wi.y = 128;
    window.setSeed(wi, DIM_OVERWORLD);
    int failures = 0;
    for (const char *seed : {"12345", "-3829811542736183482"})
    {
        window.ui->seedEdit->setText(seed);
        // Emit the same signal as committing the seed field with Enter/Tab.
        QMetaObject::invokeMethod(window.ui->seedEdit, "editingFinished", Qt::DirectConnection);
        for (const char *version : {"1.21.11", "26.3", "1.21.11"})
        {
            window.ui->comboBoxMC->setCurrentText(version);
            QWorld *world = window.mapView->world;
            Generator reference;
            setupGenerator(&reference, str2mc(version), 0);
            applySeed(&reference, DIM_OVERWORLD, (uint64_t)QString(seed).toLongLong());
            const int expected = getBiomeAt(&reference, 1, 68148, 128, 80990);
            const int actual = world->getBiome({68148, 80990});
            const bool pass = world->g.mc == str2mc(version) &&
                (int64_t)world->g.seed == QString(seed).toLongLong() && actual == expected;
            printf("%s UI seed=%s selected=%s backend=%s at (68148,128,80990)=%s\n",
                pass ? "PASS" : "FAIL", seed, version, mc2str(world->g.mc), biome2str(world->g.mc, actual));
            failures += !pass;
        }
    }

    // A location known to differ between these versions, proving generation changes.
    window.ui->seedEdit->setText("-3829811542736183482");
    QMetaObject::invokeMethod(window.ui->seedEdit, "editingFinished", Qt::DirectConnection);
    window.ui->comboBoxMC->setCurrentText("1.21.11");
    int oldBiome = window.mapView->world->getBiome({68148, 80990});
    window.ui->comboBoxMC->setCurrentText("26.3");
    int newBiome = window.mapView->world->getBiome({68148, 80990});
    bool changes = oldBiome != newBiome && newBiome == dappled_forest;
    printf("%s UI version switch at (68148,128,80990): %s -> %s\n", changes ? "PASS" : "FAIL",
        biome2str(MC_1_21_11, oldBiome), biome2str(MC_26_3, newBiome));
    failures += !changes;
    window.mapView->deleteWorld();
    printf("%d failures; no windows displayed.\n", failures);
    return failures ? 1 : 0;
}
