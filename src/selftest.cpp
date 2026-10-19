// Small, opt-in integration check. Never shows a window or reads user settings.
#include "mainwindow.h"
#include "util.h"
#include "ui_mainwindow.h"
#include "cubiomes/util.h"

#include <QComboBox>
#include <QTemporaryDir>
#include <cstdio>

int runSelfTests(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir scratch(QCoreApplication::applicationDirPath() + "/selftest-XXXXXX");
    if (!scratch.isValid())
        return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, scratch.path());
    QThreadPool::globalInstance()->setMaxThreadCount(1);

    int failures = 0;
    auto check = [&](bool passed, const char *name) {
        printf("%s %s\n", passed ? "PASS" : "FAIL", name);
        fflush(stdout);
        if (!passed) failures++;
    };

    check(str2mc("26.3") == MC_26_3 && QString(mc2str(MC_26_3)) == "26.3", "26.3 version round trip");
    check(biomeExists(MC_26_3, dappled_forest) && !biomeExists(MC_26_2, dappled_forest), "Dappled Forest version gate");
    check(biomeExists(MC_26_3, sulfur_caves) && !biomeExists(MC_26_1, sulfur_caves), "Sulfur Caves version gate");
    check(getBiomeDisplay(MC_26_3, dappled_forest) == "Dappled Forest" &&
          getBiomeDisplay(MC_26_3, sulfur_caves) == "Sulfur Caves", "new biome labels");
    check(g_biomeColors[dappled_forest][0] == 0xeb && g_biomeColors[sulfur_caves][0] == 0xc8, "new biome colors");

    Generator g;
    setupGenerator(&g, MC_26_3, 0);
    const uint64_t fixtureSeed = UINT64_C(14616932530973368134); // -3829811542736183482
    applySeed(&g, DIM_OVERWORLD, fixtureSeed);
    check(getBiomeAt(&g, 1, 68148, 77, 80990) == dappled_forest, "upstream 26.3 Dappled Forest coordinate fixture");

    bool sulfur = false;
    applySeed(&g, DIM_OVERWORLD, 0);
    for (int z = -4096; z <= 4096 && !sulfur; z += 128)
        for (int x = -4096; x <= 4096 && !sulfur; x += 128)
            sulfur = getBiomeAt(&g, 4, x >> 2, 0, z >> 2) == sulfur_caves;
    check(sulfur, "26.3 underground Sulfur Caves generation smoke check");

    bool maps = true;
    for (int dim : {DIM_OVERWORLD, DIM_NETHER, DIM_END})
    {
        applySeed(&g, dim, fixtureSeed);
        Range r = {4, -6, -6, 12, 12, 16, 1};
        int *cache = allocCache(&g, r);
        maps &= genBiomes(&g, cache, r) == 0;
        for (int z = 0; z < r.sz; z++)
            for (int x = 0; x < r.sx; x++)
                maps &= cache[z*r.sx+x] == getBiomeAt(&g, 4, r.x+x, r.y, r.z+z);
        free(cache);
    }
    check(maps, "all dimensions: tile generation matches coordinate queries");

    const uint64_t ringSeed = UINT64_C(856461443495910397);
    setupGenerator(&g, MC_26_2, 0);
    applySeed(&g, DIM_END, ringSeed);
    check(getBiomeAt(&g, 16, -371136 >> 4, 0, -64 >> 4) == end_barrens, "26.2 legacy End ring fixture");
    setupGenerator(&g, MC_26_3, 0);
    applySeed(&g, DIM_END, ringSeed);
    check(getBiomeAt(&g, 16, -371136 >> 4, 0, -64 >> 4) == end_highlands &&
          getEndSurfaceHeight(MC_26_3, ringSeed, -371136, -64) == 57, "26.3 End ring correction fixture");

    setupGenerator(&g, MC_1_18, 0);
    applySeed(&g, DIM_OVERWORLD, 262);
    check(getBiomeAt(&g, 1, 0, 63, 0) == mushroom_fields, "upstream README 1.18 regression fixture");

    StructureConfig camp;
    check(!getStructureConfig(Abandoned_Camp, MC_26_2, &camp) &&
          getStructureConfig(Abandoned_Camp, MC_26_3, &camp) &&
          camp.salt == 91231127 && camp.regionSize == 37 && camp.chunkRange == 29,
          "camp version gate and final 26.3 vanilla spacing/salt");
    Pos p;
    check(getStructurePos(Abandoned_Camp, MC_26_3, fixtureSeed, 0, -1, &p) && p.x == 224 && p.z == -384,
          "camp position: upstream fixture and independent Java RNG calculation");
    WorldInfo wi;
    wi.seed = fixtureSeed;
    wi.y = 77;
    std::vector<VarPos> structures;
    getStructs(&structures, camp, wi, DIM_OVERWORLD, 224, -384, 225, -383);
    check(structures.size() == 1 && structures[0].type == Abandoned_Camp && structures[0].v.biome == savanna,
          "viewer camp overlay biome viability and variant fixture");
    check(mapopt2stype(D_CAMP) == Abandoned_Camp && str2mapopt(mapopt2str(D_CAMP)) == D_CAMP &&
          !getMapIcon(D_CAMP).isNull(), "camp action, settings and icon wiring");

    Pos normal;
    getStructurePos(Abandoned_Camp, MC_26_3, fixtureSeed, 0, -1, &normal);
    g_extgen.saltOverride = true;
    g_extgen.salts[Abandoned_Camp] = camp.salt + 1;
    getStructurePos(Abandoned_Camp, MC_26_3, fixtureSeed, 0, -1, &p);
    check(p.x != normal.x || p.z != normal.z, "custom salt provider reaches backend placement");
    g_extgen.reset();

    MainWindow window(scratch.path() + "/session.save", "");
    window.autosaveTimer.stop();
    Config config = window.config;
    config.mapThreads = 1;
    window.mapView->setConfig(config);
    check(window.ui->comboBoxMC->findText("26.3") >= 0 && !window.isVisible(), "26.3 selector available without showing a window");
    bool dimensions = true, rendering = true;
    for (int dim : {DIM_OVERWORLD, DIM_NETHER, DIM_END})
    {
        window.setSeed(wi, dim);
        QWorld *world = window.mapView->world;
        dimensions &= world && world->g.mc == MC_26_3 && world->dim == dim;
        if (!world) continue;
        for (int i = 0; i < D_STRUCT_NUM; i++) world->sshow[i] = false;
        QImage image(96, 96, QImage::Format_RGB32);
        image.fill(Qt::magenta);
        {
            QPainter painter(&image);
            world->draw(painter, 96, 96, 68148, 80990, 0.25);
        }
        world->waitForIdle();
        {
            QPainter painter(&image);
            world->draw(painter, 96, 96, 68148, 80990, 0.25);
        }
        bool tile = false;
        for (const Level& level : world->lvb)
            for (const Quad *quad : level.cells)
            {
                QImage *img = quad ? quad->img.loadAcquire() : nullptr;
                tile |= quad && quad->done && img && !img->isNull();
            }
        rendering &= tile && image.pixelColor(48, 48) != QColor(Qt::magenta);
        world->waitForIdle();
    }
    check(dimensions, "viewer preserves 26.3 across all dimension switches");
    check(rendering, "viewer renders biome map tiles offscreen in all dimensions");
    window.mapGoto(128, -256, 4);
    const qreal scale = window.mapView->getScale();
    window.mapZoom(2);
    check(window.mapView->getX() == 128 && window.mapView->getZ() == -256 &&
          window.mapView->getScale() != scale, "pan/coordinate jump and zoom");
    wi.seed = 12345;
    window.setSeed(wi, DIM_NETHER);
    check(window.mapView->world->g.seed == wi.seed && window.mapView->world->wi.mc == MC_26_3, "seed changes reach map backend");
    wi.mc = MC_1_16;
    window.setSeed(wi, DIM_OVERWORLD);
    check(window.mapView->world->g.mc == MC_1_16, "viewer older-version switch regression");
    window.mapView->deleteWorld();
    printf("Self-test complete: %d failure(s).\n", failures);
    return failures ? 1 : 0;
}
