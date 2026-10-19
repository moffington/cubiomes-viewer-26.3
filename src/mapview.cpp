#include "mapview.h"

#include "gotodialog.h"
#include "markerstab.h"
#include "util.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QThread>
#include <QThreadPool>
#include <QTime>
#include <QWheelEvent>

#include <math.h>

bool MapOverlay::event(QEvent *e)
{
    if (e->type() == QEvent::MouseMove)
        update();
    return QWidget::event(e);
}

void MapOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QString s = bname + QString::asprintf(" [%d,%d]", pos.x, pos.z);
    QRect r = painter.fontMetrics()
            .boundingRect(0, 0, width(), height(), Qt::AlignRight | Qt::AlignTop, s);

    painter.fillRect(r, QBrush(QColor(0, 0, 0, 128), Qt::SolidPattern));
    painter.setPen(Qt::white);
    painter.drawText(r, s);
}

MapView::MapView(QWidget *parent)
    : QWidget(parent)
    , world()
    , decay(2.0)
    , blocks2pix(1.0/16)
    , focusx(),focusz()
    , prevx(),prevz()
    , velx(), velz()
    , mtime()
    , holding()
    , mstart(),mprev()
    , bstart()
    , measure()
    , updatecounter()
    , lopt()
    , config()
{
    memset(sshow, 0, sizeof(sshow));

    QFont fmono;
    fmono.setFamily(QString::fromUtf8("Monospace"));
    setFont(fmono);

    QPalette pal = palette();
    pal.setColor(QPalette::Dark, Qt::black);
    setAutoFillBackground(true);
    setPalette(pal);

    elapsed1.start();
    frameelapsed.start();
    actelapsed.start();

    overlay = new MapOverlay(this);
    overlay->setMouseTracking(true);
    overlay->setFont(font());
    setMouseTracking(true);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
}

MapView::~MapView()
{
    delete world;
    delete overlay;
}

void MapView::deleteWorld()
{
    if (world)
    {
        disconnect(world);
        delete world;
        world = nullptr;
    }
}

void MapView::refresh()
{
    if (world)
    {
        WorldInfo wi = world->wi;
        int dim = world->dim;
        deleteWorld();
        world = new QWorld(wi, dim, lopt);
        QObject::connect(world, &QWorld::update, this, &MapView::mapUpdate);
        settingsToWorld();
        syncSavedMarkers();
    }
}

void MapView::setSeed(WorldInfo wi, int dim, LayerOpt lopt)
{
    if (markerSet && markerSeed != wi.seed)
        clearCoordinateMarker();
    prevx = focusx = getX();
    prevz = focusz = getZ();
    velx = velz = 0;
    this->lopt = lopt;

    if (world == NULL || !wi.equals(world->wi) ||
        world->lopt.mode == LOPT_STRUCTS || lopt.mode == LOPT_STRUCTS)
    {
        deleteWorld();
        world = new QWorld(wi, dim, lopt);
        QObject::connect(world, &QWorld::update, this, &MapView::mapUpdate);
    }
    else if (world->dim != dim || world->lopt.activeDifference(lopt))
    {
        world->setDim(dim, lopt);
    }
    settingsToWorld();
    syncSavedMarkers();
    emit worldChanged();
    update(2);
}

void MapView::setView(qreal x, qreal z, qreal scale)
{
    anielapsed.invalidate();
    if (scale > 0)
    {
        blocks2pix = 1.0 / scale;
    }
    prevx = focusx = x;
    prevz = focusz = z;
    velx = velz = 0;
    update(2);
}

void MapView::animateView(qreal x_dst, qreal z_dst, qreal s_dst)
{
    if (s_dst <= 0)
        s_dst = 1.0 / blocks2pix;
    this->x_src = prevx = focusx = getX();
    this->z_src = prevz = focusz = getZ();
    this->s_src = 1.0 / blocks2pix;
    this->x_dst = x_dst;
    this->z_dst = z_dst;
    this->s_dst = s_dst;
    qreal dx = x_dst - x_src;
    qreal dz = z_dst - z_src;
    qreal ds = s_dst - s_src;
    qreal d = sqrt(dx*dx + dz*dz + ds*ds*1024);
    this->s_mul = d * 1e-3;
    this->atime = sqrt(d) * 5e7;
    anielapsed.start();
    update(2);
}

void MapView::zoom(qreal factor)
{
    qreal zoommin = 1.0 / 2048.0, zoommax = 128.0;
    if (factor < 1 && blocks2pix < zoommin) return;
    if (factor > 1 && blocks2pix > zoommax) return;
    blocks2pix *= factor;
    if (factor < 1 && blocks2pix < zoommin) blocks2pix = zoommin;
    if (factor > 1 && blocks2pix > zoommax) blocks2pix = zoommax;
    update();
}

void MapView::setCoordinateMarker(qreal x, qreal z)
{
    if (!world)
        return;
    for (const SavedMarker& saved : savedMarkers)
        if (saved.x == x && saved.z == z && !(saved.completed && markerStore->hideCompleted()))
        { selectedMarker = saved.id; markerSet = false; update(); return; }
    markerPos = QPointF(x, z);
    markerSeed = world->wi.seed;
    markerDim = world->dim;
    markerSet = true;
    update();
}

void MapView::clearCoordinateMarker()
{
    markerSet = false;
    update();
}

void MapView::setMarkerStore(MarkerStore *store)
{
    if (markerStore) disconnect(markerStore, nullptr, this, nullptr);
    markerStore = store;
    if (store) connect(store, &MarkerStore::changed, this, &MapView::syncSavedMarkers);
    syncSavedMarkers();
}

void MapView::syncSavedMarkers()
{
    savedMarkers.clear();
    if (world)
    {
        world->hiddenStructures.clear();
        if (markerStore) for (const SavedMarker& m : markerStore->all())
        {
            if (!m.matchesWorld(world->wi) || m.dimension != world->dim) continue;
            savedMarkers.append(m);
            if (markerSet && markerDim == world->dim && markerPos == QPointF(m.x, m.z) &&
                !(m.completed && markerStore->hideCompleted())) markerSet = false;
            if (m.completed && markerStore->hideCompleted() && m.kind != D_NONE)
                world->hiddenStructures.insert(SavedMarker::structureKey(m.kind, m.x, m.z));
        }
        if (world->hiddenStructures.contains(SavedMarker::structureKey(world->selopt, world->selvp.p.x, world->selvp.p.z)))
            world->selopt = D_NONE;
    }
    update();
}

bool MapView::savePointMarker(qreal x, qreal z)
{
    if (!world || !markerStore) return false;
    SavedMarker marker;
    marker.world = world->wi; marker.dimension = world->dim;
    marker.x = x; marker.z = z;
    marker.name = tr("Marker at %1, %2").arg(SavedMarker::coordinate(x), SavedMarker::coordinate(z));
    if (!markerStore->put(marker))
    {
        QMessageBox::warning(this, tr("Marker was not saved"), markerStore->lastError());
        return false;
    }
    selectedMarker = marker.id;
    return true;
}

void MapView::saveMarkerOrWarn(SavedMarker marker)
{
    if (!markerStore->put(marker))
        QMessageBox::warning(this, tr("Marker was not saved"), markerStore->lastError());
    else { selectedMarker = marker.id; update(); }
}

void MapView::addSavedMarker(qreal x, qreal z)
{
    if (!world || !markerStore) return;
    SavedMarker marker;
    marker.world = world->wi; marker.dimension = world->dim;
    marker.x = x; marker.z = z;
    marker.name = tr("Marker");
    auto *dialog = new MarkerEditor(markerStore, marker, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MapView::addMarkerAtCenter()
{
    addSavedMarker(floor(getX()), floor(getZ()));
}

void MapView::editSavedMarker(const QString& id)
{
    if (!markerStore) return;
    const SavedMarker marker = markerStore->find(id);
    if (marker.id.isEmpty()) return;
    auto *dialog = new MarkerEditor(markerStore, marker, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MapView::focusSavedMarker(const QString& id)
{
    if (!world || !markerStore) return;
    const SavedMarker marker = markerStore->find(id);
    if (marker.id.isEmpty() || !marker.matchesWorld(world->wi) || marker.dimension != world->dim) return;
    selectedMarker = id;
    setView(marker.x, marker.z, std::min(getScale(), qreal(16)));
    if (marker.completed && markerStore->hideCompleted()) setCoordinateMarker(marker.x, marker.z);
    else clearCoordinateMarker();
}

QString MapView::savedMarkerAt(const QPoint& pos)
{
    if (!markerStore) return QString();
    const qreal fx = getX(), fz = getZ();
    qreal best = 18 * 18;
    QString result;
    for (const SavedMarker& m : savedMarkers)
    {
        if (m.completed && markerStore->hideCompleted()) continue;
        const qreal dx = (m.x - fx) * blocks2pix + width()/2.0 - pos.x();
        const qreal dz = (m.z - fz) * blocks2pix + height()/2.0 - pos.y();
        const qreal d = dx*dx + dz*dz;
        if (d < best) { best = d; result = m.id; }
    }
    return result;
}

void MapView::drawSavedMarkers(QPainter& painter, qreal fx, qreal fz)
{
    if (!markerStore) return;
    painter.save();
    painter.setFont(font());
    for (const SavedMarker& m : savedMarkers)
    {
        if (m.completed && markerStore->hideCompleted()) continue;
        const QPointF point((m.x - fx) * blocks2pix + width()/2.0,
                            (m.z - fz) * blocks2pix + height()/2.0);
        if (!QRectF(rect()).adjusted(-24, -24, 24, 24).contains(point)) continue;
        const bool structure = m.kind != D_NONE;
        if (structure)
        {
            const QPixmap& icon = getMapIcon(m.kind);
            painter.drawPixmap(point - QPointF(icon.width()/2.0, icon.height()/2.0), icon);
        }
        const QPointF badge = point + (structure ? QPointF(8, 8) : QPointF());
        painter.setPen(QPen(Qt::black, 4));
        painter.setBrush(m.completed ? QColor(36, 158, 82) : QColor(40, 135, 235));
        painter.drawEllipse(badge, 7, 7);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.drawEllipse(badge, 7, 7);
        painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (m.completed)
        {
            painter.drawLine(badge + QPointF(-4, 0), badge + QPointF(-1, 3));
            painter.drawLine(badge + QPointF(-1, 3), badge + QPointF(4, -3));
        }
        else painter.drawPoint(badge);
        if (m.id == selectedMarker)
        {
            painter.setBrush(Qt::NoBrush); painter.setPen(QPen(Qt::white, 1));
            painter.drawEllipse(point, 14, 14);
        }
        if (markerStore->showLabels() || m.id == selectedMarker || m.id == hoveredMarker)
        {
            const QString text = m.name + (m.completed ? tr(" (completed)") : QString());
            const QFontMetrics fm = painter.fontMetrics();
            const QString elided = fm.elidedText(text, Qt::ElideRight, qMax(80, qMin(300, width()-8)));
            QRectF box(0, 0, fm.horizontalAdvance(elided) + 10, fm.height() + 6);
            box.moveTopLeft(point + QPointF(18, -box.height()-5));
            if (box.right() > width()-3) box.moveRight(width()-3);
            if (box.left() < 3) box.moveLeft(3);
            if (box.top() < 3) box.moveTop(point.y()+18);
            painter.setPen(Qt::NoPen); painter.setBrush(QColor(0, 0, 0, 200));
            painter.drawRoundedRect(box, 3, 3);
            painter.setPen(Qt::white); painter.drawText(box, Qt::AlignCenter, elided);
        }
    }
    painter.restore();
}

void MapView::drawCoordinateMarker(QPainter& painter, qreal fx, qreal fz)
{
    if (!markerSet || !world || markerDim != world->dim)
        return;
    const QPointF point((markerPos.x() - fx) * blocks2pix + width() / 2.0,
                        (markerPos.y() - fz) * blocks2pix + height() / 2.0);
    // Clip before painting: distant world coordinates can exceed Qt's raster range.
    if (!QRectF(rect()).adjusted(-14, -14, 14, 14).contains(point))
        return;

    painter.save();
    const QColor red(255, 64, 80);
    // A fixed-size crosshair with a white outline stays legible over every biome.
    for (const QPen& pen : {QPen(Qt::black, 5), QPen(Qt::white, 3), QPen(red, 1.5)})
    {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(point, 7, 7);
        painter.drawLine(point + QPointF(-12, 0), point + QPointF(12, 0));
        painter.drawLine(point + QPointF(0, -12), point + QPointF(0, 12));
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(red);
    painter.drawEllipse(point, 2, 2);

    auto coordinate = [](qreal value) {
        return QString::number(value, 'f', value == floor(value) ? 0 : 1);
    };
    const QString label = tr("X: %1  Z: %2")
            .arg(coordinate(markerPos.x()), coordinate(markerPos.y()));
    const QFontMetrics fm = painter.fontMetrics();
    QRectF box(0, 0, fm.horizontalAdvance(label) + 10, fm.height() + 6);
    box.moveTopLeft(point + QPointF(16, -box.height() - 5));
    if (box.right() > width() - 3) box.moveRight(width() - 3);
    if (box.left() < 3) box.moveLeft(3);
    if (box.top() < 3) box.moveTop(point.y() + 16);
    painter.setBrush(QColor(0, 0, 0, 200));
    painter.drawRoundedRect(box, 3, 3);
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, label);
    painter.restore();
}

bool MapView::getShow(int stype)
{
    if (stype < 0 || stype >= D_STRUCT_NUM)
        return false;
    return sshow[stype];
}

void MapView::setShow(int stype, bool v)
{
    sshow[stype] = v;
    settingsToWorld();
    update(2);
}

void MapView::setConfig(const Config& c)
{
    config = c;
    settingsToWorld();
    update(2);
}

void MapView::setShapes(const std::vector<Shape>& s)
{
    shapes = s;
    settingsToWorld();
    update(1);
}

void MapView::refreshBiomeColors()
{
    if (world)
    {
        world->refreshBiomeColors();
        update(1);
    }
}

void MapView::settingsToWorld()
{
    if (!world)
        return;
    for (int s = 0; s < D_STRUCT_NUM; s++)
        world->sshow[s] = sshow[s] && world->mconfig.enabled(s);
    world->showBB = config.showBBoxes;
    world->gridspacing = config.gridSpacing;
    world->gridmultiplier = config.gridMultiplier;
    world->memlimit = (uint64_t) config.mapCacheSize * 1024 * 1024;
    world->threadlimit = config.mapThreads;
    world->lopt = lopt;
    world->shapes = shapes;
}

static qreal smoothstep(qreal x)
{
    if (x < 0) return 0;
    if (x > 1) return 1;
    qreal v = x * x * x * (x * (x * 6 - 15) + 10);
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

static qreal smoothstep_integral(qreal x)
{
    if (x < 0) return 0;
    if (x > 1) return 1;
    qreal x2 = x * x;
    qreal v = x2 * x2 * (x * (x - 3) + 2.5);
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

static void smoothmotion(qreal *pos, qreal *vel, qreal x0, qreal x1, qreal t)
{
    // TODO: include the percieved travel distance due to scale changes
    qreal xm = (x0 + x1) / 2;
    qreal p, v;
    if (t < 0.5)
    {
        t = 2 * t;
        v = smoothstep(t);
        qreal u = 2 * smoothstep_integral(t);
        p = x0 + (xm - x0) * u;
    }
    else
    {
        t = 2 * (1 - t);
        v = smoothstep(t);
        qreal u = 1 - 2 * smoothstep_integral(t);
        p = xm + (x1 - xm) * u;
    }
    if (vel) *vel = v;
    if (pos) *pos = p;
}

qreal MapView::getX()
{
    if (anielapsed.isValid())
    {
        qreal t = anielapsed.nsecsElapsed() / atime;
        qreal u, v;
        smoothmotion(&u, 0, x_src, x_dst, t);
        smoothmotion(&v, 0, s_src, s_dst, t);
        blocks2pix = 1.0 / v;
        return focusx = u;
    }

    qreal dt = frameelapsed.nsecsElapsed() * 1e-9;
    qreal fx = focusx;

    if (velx)
    {
        qreal df = 1.0 - exp(-decay*dt);
        fx += velx * df;
        if (df > 0.998)
        {
            focusx = fx;
            velx = 0;
        }
    }
    return fx;
}

qreal MapView::getZ()
{
    qreal dt = frameelapsed.nsecsElapsed() * 1e-9;
    if (anielapsed.isValid())
    {
        qreal t = anielapsed.nsecsElapsed() / atime;
        qreal u, v;
        smoothmotion(&u, 0, z_src, z_dst, t);
        smoothmotion(&v, 0, s_src, s_dst, t);
        blocks2pix = 1.0 / v;
        return focusz = u;
    }

    qreal fz = focusz;
    if (velz)
    {
        qreal df = 1.0 - exp(-decay*dt);
        fz += velz * df;
        if (df > 0.998)
        {
            focusz = fz;
            velz = 0;
        }
    }
    return fz;
}

static qreal clampimax(qreal x, bool *ok = nullptr)
{
    const double imax = INT_MAX - 1024.0;
    if (x < -imax) x = -imax;
    if (x > +imax) x = +imax;
    if (ok) *ok = (x > -imax && x < +imax);
    return x;
}

void MapView::getVisible(int *x0, int *z0, int *x1, int *z1)
{
    qreal x = getX();
    qreal z = getZ();
    qreal uiw = width() / blocks2pix;
    qreal uih = height() / blocks2pix;
    if (x0) *x0 = (int) clampimax( floor(x - uiw/2) );
    if (z0) *z0 = (int) clampimax( floor(z - uih/2) );
    if (x1) *x1 = (int) clampimax( ceil(x + uiw/2) );
    if (z1) *z1 = (int) clampimax( ceil(z + uih/2) );
}

void MapView::update(int cnt)
{
    updatecounter = cnt;
    QWidget::update();
}

VarPos MapView::getActivePos()
{
    if (world && world->selopt != D_NONE)
        return world->selvp;
    return VarPos(overlay->pos, -1);
}

QPixmap MapView::screenshot()
{
    overlay->setVisible(false);
    QPixmap img = grab();
    overlay->setVisible(true);
    return img;
}

void MapView::mapUpdate()
{
    update(1);
}

QMenu *MapView::createContextMenu(const QPoint &pos)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setObjectName("mapContextMenu");
    menu->setFont(font());
    // this is a contextual temporary menu so shortcuts are only indicated here,
    // but will not function - see keyReleaseEvent() for shortcut implementation

    const qreal fx = getX(), fz = getZ();
    setView(fx, fz, getScale());
    const qreal clickedX = floor((pos.x()-width()/2.0)/blocks2pix + fx);
    const qreal clickedZ = floor((pos.y()-height()/2.0)/blocks2pix + fz);
    if (world)
    {
        mstart = pos;
        world->setSelectPos(mstart);
        grab(); // invokes an immediate paint call
    }

    struct _cpy_dat { QString txt, cpy; };
    std::vector<_cpy_dat> cpy_dat;

    VarPos vp = world && world->selopt != D_NONE ? world->selvp : VarPos({(int)clickedX, (int)clickedZ}, -1);
    const QPixmap *icon = 0;

    if (vp.type != -1)
    {   // structure has a known size / location
        int midx, midy, midz;
        if (vp.pieces.size() > 0)
        {
            const Piece& pc = vp.pieces[0];
            midx = (pc.bb0.x + pc.bb1.x) >> 1;
            midy = (pc.bb0.y) + 1;
            midz = (pc.bb0.z + pc.bb1.z) >> 1;
        }
        else
        {
            midx = vp.p.x + vp.v.x + vp.v.sx / 2;
            midy = vp.v.y;
            midz = vp.p.z + vp.v.z + vp.v.sz / 2;
            if (midy >= 320 && world)
            {
                midy = world->estimateSurface(Pos{midx, midz}) + 8;
                if (midy < 63)
                    midy = 63;
            }
        }
        cpy_dat.push_back({ tr("Copy tp:"), QString::asprintf("/tp @p %d %d %d", midx, midy, midz) });
        icon = &getMapIcon(world ? world->selopt : -1, &vp);
    }
    cpy_dat.push_back({ tr("Copy tp:"),     QString::asprintf("/tp @p %d ~ %d", vp.p.x, vp.p.z) });
    cpy_dat.push_back({ tr("Copy coords:"), QString::asprintf("%d %d", vp.p.x, vp.p.z) });
    cpy_dat.push_back({ tr("Copy chunk:"),  QString::asprintf("%d %d", vp.p.x >> 4, vp.p.z >> 4) });
    cpy_dat.push_back({ tr("Copy region:"), QString::asprintf("%d %d", vp.p.x >> 9, vp.p.z >> 9) });

    int wmax = 0;
    for (auto& it : cpy_dat)
    {
        int w = txtWidth(menu->fontMetrics(), it.txt + "  ");
        if (w > wmax)
            wmax = w;
    }

    menu->addAction(tr("Go to coordinates..."), this, &MapView::onGoto, QKeySequence(Qt::CTRL + Qt::Key_G));
    if (markerSet && world && markerDim == world->dim)
        menu->addAction(tr("Clear coordinate marker"), this, &MapView::clearCoordinateMarker);
    if (markerStore && world)
    {
        menu->addSeparator();
        menu->addAction(tr("Add marker here..."), this, [=] { addSavedMarker(clickedX, clickedZ); })
            ->setObjectName("addSavedMarkerHere");
        if (markerSet && markerDim == world->dim)
            menu->addAction(tr("Save Go to coordinate marker..."), this, [=] { addSavedMarker(markerPos.x(), markerPos.y()); });
        const QString id = savedMarkerAt(pos);
        if (!id.isEmpty())
        {
            const SavedMarker marker = markerStore->find(id);
            menu->addAction(marker.completed ? tr("Mark as not completed") : tr("Mark as completed"), this,
                [=] { if (!markerStore->setCompleted(id, !marker.completed))
                    QMessageBox::warning(this, tr("Progress was not saved"), markerStore->lastError()); })
                ->setObjectName("toggleSavedCompleted");
            menu->addAction(tr("Edit saved marker..."), this, [=] { editSavedMarker(id); });
            menu->addAction(tr("Delete saved marker..."), this, [=] {
                if (QMessageBox::question(this, tr("Delete marker"), tr("Delete '%1' and its saved progress?").arg(marker.name)) == QMessageBox::Yes)
                    if (!markerStore->remove(id)) QMessageBox::warning(this, tr("Marker was not deleted"), markerStore->lastError());
            });
        }
        else if (world->selopt != D_NONE)
        {
            SavedMarker marker;
            marker.world = world->wi; marker.dimension = world->dim;
            marker.kind = world->selopt; marker.x = vp.p.x; marker.z = vp.p.z;
            marker.name = marker.typeName();
            menu->addAction(tr("Save %1 marker").arg(marker.typeName()), this, [=] { saveMarkerOrWarn(marker); })
                ->setObjectName("saveStructureMarker");
            marker.completed = true;
            menu->addAction(tr("Mark %1 as completed").arg(marker.typeName()), this, [=] { saveMarkerOrWarn(marker); })
                ->setObjectName("completeStructure");
        }
        menu->addAction(tr("Saved markers..."), this, &MapView::showMarkersRequested);
        menu->addSeparator();
    }
    if (world)
    {
        QString txt = tr("Copy seed:");
        while (txtWidth(menu->fontMetrics(), txt + " ") < wmax)
            txt += " ";
        txt += QString::asprintf("%" PRId64, (int64_t)world->wi.seed);
        menu->addAction(txt, this, &MapView::copySeed, QKeySequence::Copy);
    }
    for (auto& it : cpy_dat)
    {
        QString txt = it.txt;
        while (txtWidth(menu->fontMetrics(), txt + " ") < wmax)
            txt += " ";
        txt += it.cpy;
        if (icon)
            menu->addAction(*icon, txt, [=](){ this->copyText(it.cpy); });
        else
            menu->addAction(txt, [=](){ this->copyText(it.cpy); });
    }
    //menu->addAction(tr("Animation"), this, &MapView::runAni);

    for (QAction *act : menu->actions())
        act->setFont(font());
    return menu;
}

void MapView::showContextMenu(const QPoint& pos)
{
    createContextMenu(pos)->popup(mapToGlobal(pos));
}

void MapView::runAni()
{
}

void MapView::copySeed()
{
    if (world)
    {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(QString::asprintf("%" PRId64, (int64_t)world->wi.seed));
    }
}

void MapView::copyText(QString txt)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(txt);
}

void MapView::onGoto()
{
    GotoDialog *dialog = new GotoDialog(this, getX(), getZ(), getScale());
    dialog->show();
}

void MapView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);
    //painter.setRenderHint(QPainter::HighQualityAntialiasing);

    qreal fx = getX();
    qreal fz = getZ();
    if (anielapsed.isValid() && anielapsed.nsecsElapsed() > atime)
    {
        anielapsed.invalidate();
        focusx = x_dst;
        focusz = z_dst;
        blocks2pix = 1.0 / s_dst;
    }
    bool ok;
    fx = clampimax(fx, &ok);
    if (!ok) focusx = fx;
    fz = clampimax(fz, &ok);
    if (!ok) focusz = fz;

    if (world)
    {
        world->draw(painter, width(), height(), fx, fz, blocks2pix);

        QPoint cur = mapFromGlobal(QCursor::pos());
        qreal bx = (cur.x() -  width()/2.0) / blocks2pix + fx;
        qreal bz = (cur.y() - height()/2.0) / blocks2pix + fz;
        Pos p = {(int)clampimax(floor(bx)), (int)clampimax(floor(bz))};
        overlay->pos = p;
        overlay->bname = world->getBiomeName(p);

        if (measure)
        {
            qreal startx = (bstart.x - fx) * blocks2pix + width() / 2.0;
            qreal startz = (bstart.z - fz) * blocks2pix + height() / 2.0;
            QPointF start = {startx, startz};
            painter.setPen(QPen(QColor(0, 0, 0, 255), 2));
            painter.drawPoint(start);
            painter.setPen(QPen(QColor(255, 255, 128, 255), 1));
            painter.drawLine(start, cur);
            qreal dx = bstart.x - bx;
            qreal dz = bstart.z - bz;
            qreal r = sqrt(dx*dx + dz*dz);
            QString s = QString::asprintf("r=%g", floor(r));
            QRect textrec = painter.fontMetrics()
                    .boundingRect(0, 0, 0, 0, Qt::AlignLeft | Qt::AlignVCenter, s);
            textrec.translate(cur);
            painter.fillRect(textrec, QBrush(QColor(0, 0, 0, 128), Qt::SolidPattern));
            painter.setPen(QPen(QColor(255, 255, 128, 255), 1));
            painter.drawText(textrec, s);
        }

        drawSavedMarkers(painter, fx, fz);
        drawCoordinateMarker(painter, fx, fz);

        bool active = world->isBusy();
        if (active || velx || velz || anielapsed.isValid())
            updatecounter = 2;
        if (updatecounter > 0)
        {
            updatecounter--;
            QWidget::update();

            if (active)
            {   // processing animation
                qreal cyc = actelapsed.nsecsElapsed() * 1e-9;
                qreal ang = 360 * (1.0 - (cyc - (int) cyc));
                int r = 20;
                QRect rec = QRect(r, height() - 2*r, r, r);

                QConicalGradient gradient;
                gradient.setCenter(rec.center());
                gradient.setAngle(ang);
                gradient.setColorAt(0, QColor(0, 0, 0, 192));
                gradient.setColorAt(1, QColor(255, 255, 255, 192));
                QPen pen(QBrush(gradient), 5, Qt::SolidLine, Qt::SquareCap);
                painter.setPen(pen);
                painter.drawRect(rec);
            }
        }
        else
        {
            actelapsed.start();
        }
    }
}

void MapView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    overlay->resize(width(), height());
}

void MapView::wheelEvent(QWheelEvent *e)
{
    const qreal ang = e->angleDelta().y() / 8; // e->delta() / 8;
    zoom(pow(2, ang/100));
}

void MapView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        mprev = mstart = e->pos();
        selectedMarker = savedMarkerAt(e->pos());
        holding = true;
        prevx = focusx = getX();
        prevz = focusz = getZ();
        velx = 0;
        velz = 0;
        mtime = 0;
        elapsed1.start();
        frameelapsed.start();
        if (e->modifiers() & Qt::ShiftModifier)
        {
            if (!measure)
                bstart = overlay->pos;
            measure = true;
        }

        if (world)
        {
            world->setSelectPos(mstart);
            update();
        }
    }
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
    if (!(e->buttons() & Qt::LeftButton) && markerStore)
    {
        const QString id = savedMarkerAt(e->pos());
        if (id != hoveredMarker)
        {
            hoveredMarker = id;
            const SavedMarker marker = markerStore->find(id);
            const QString tooltip = id.isEmpty() ? QString() : marker.name + "\n" +
                (marker.completed ? tr("Completed") : tr("To do")) + "\nX: " + SavedMarker::coordinate(marker.x) +
                "  Z: " + SavedMarker::coordinate(marker.z) + (marker.notes.isEmpty() ? QString() : "\n" + marker.notes);
            overlay->setToolTip(tooltip);
            setToolTip(tooltip);
            update();
        }
    }
    if ((e->buttons() & Qt::LeftButton) && holding)
    {
        QPoint d = e->pos() - mprev;
        qreal dt = elapsed1.nsecsElapsed() * 1e-9;
        if (dt > .005)
        {
            mtime = dt;
            prevx = focusx;
            prevz = focusz;
            focusx = focusx - d.x() / blocks2pix;
            focusz = focusz - d.y() / blocks2pix;
            elapsed1.start();
        }
        mprev = e->pos();
        update();//repaint();
    }
}

void MapView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && holding)
    {
        mtime += elapsed1.nsecsElapsed() * 1e-9;
        if (mtime > .0)
        {
            elapsed1.start();
            QPoint d = e->pos() - mprev;
            focusx = focusx - d.x() / blocks2pix;
            focusz = focusz - d.y() / blocks2pix;
            velx = (focusx - prevx) / mtime;
            velz = (focusz - prevz) / mtime;
            frameelapsed.start();
            update();
        }
        if (!config.smoothMotion)
        {   // i.e. without inertia
            velx = velz = 0;
        }

        if (!(e->modifiers() & Qt::ShiftModifier))
            measure = false;
        holding = false;
        mprev = e->pos();

        if (world && e->pos() == mstart)
        {
            world->setSelectPos(mstart);
        }
    }
}

void MapView::keyPressEvent(QKeyEvent *e)
{
    if (e->matches(QKeySequence::Copy))
        copySeed();
    else if (e->matches(QKeySequence::ZoomIn) || e->key() == Qt::Key_Plus)
        zoom(pow(2, +0.125));
    else if (e->matches(QKeySequence::ZoomOut) || e->key() == Qt::Key_Minus)
        zoom(pow(2, -0.125));

    qreal step = 4 / blocks2pix;
    switch (e->key())
    {
    case Qt::Key_0:
        if (e->modifiers() == 0)
            setView(0, 0);
        break;
    case Qt::Key_Up:
        focusz -= step;
        update();
        break;
    case Qt::Key_Down:
        focusz += step;
        update();
        break;
    case Qt::Key_Left:
        focusx -= step;
        update();
        break;
    case Qt::Key_Right:
        focusx += step;
        update();
        break;
    }
    QWidget::keyPressEvent(e);
}

