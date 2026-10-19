#include "markers.h"
#include "cubiomes/util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>

bool SavedMarker::matchesWorld(const WorldInfo& wi) const
{
    return world.seed == wi.seed && world.mc == wi.mc && world.large == wi.large;
}

bool SavedMarker::samePlace(const SavedMarker& other) const
{
    return matchesWorld(other.world) && dimension == other.dimension &&
            kind == other.kind && x == other.x && z == other.z;
}

QString SavedMarker::typeName() const
{
    return kind == D_NONE ? QObject::tr("Custom marker") : mapopt2display(kind);
}

QString SavedMarker::dimensionName(int dimension)
{
    return dimension == DIM_NETHER ? QObject::tr("Nether") :
           dimension == DIM_END ? QObject::tr("End") : QObject::tr("Overworld");
}

QString SavedMarker::coordinate(qreal value)
{
    return QString::number(value, 'f', value == std::floor(value) ? 0 : 1);
}

QString SavedMarker::structureKey(int kind, qreal x, qreal z)
{
    return QString::number(kind) + ':' + coordinate(x) + ':' + coordinate(z);
}

MarkerStore::MarkerStore(const QString& file, QObject *parent) : QObject(parent)
{
    path = file.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                           .filePath("markers.json") : file;
    reload();
}

bool MarkerStore::fail(const QString& message)
{
    error = message;
    emit statusChanged();
    return false;
}

QByteArray MarkerStore::encode(const QVector<SavedMarker>& records, bool hide, bool labels)
{
    QJsonArray array;
    for (const SavedMarker& m : records)
    {
        array.append(QJsonObject{{"id", m.id}, {"seed", QString::number((qint64)m.world.seed)},
            {"version", mc2str(m.world.mc)}, {"largeBiomes", m.world.large},
            {"dimension", m.dimension}, {"kind", m.kind == D_NONE ? "custom" : mapopt2str(m.kind)},
            {"x", m.x}, {"z", m.z}, {"name", m.name}, {"notes", m.notes}, {"completed", m.completed}});
    }
    return QJsonDocument(QJsonObject{{"format", "cubiomes-viewer-markers"}, {"schema", 1},
        {"hideCompleted", hide}, {"showLabels", labels}, {"markers", array}}).toJson();
}

bool MarkerStore::read(const QString& path, QVector<SavedMarker>& records,
                       bool& hide, bool& labels, QString& error)
{
    QFile file(path);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
    if (file.size() > 16 * 1024 * 1024) { error = tr("Marker file is too large (maximum 16 MB)."); return false; }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse);
    const QJsonObject root = doc.object();
    if (parse.error != QJsonParseError::NoError || !doc.isObject() ||
        root["format"] != "cubiomes-viewer-markers" || root["schema"].toInt() != 1 ||
        !root["markers"].isArray())
    {
        error = tr("This is not a supported marker backup, or the file is damaged. It has not been overwritten.");
        return false;
    }
    QVector<SavedMarker> loaded;
    QSet<QString> ids;
    for (const QJsonValue& value : root["markers"].toArray())
    {
        const QJsonObject obj = value.toObject();
        SavedMarker m;
        bool seedOk;
        m.id = obj["id"].toString();
        m.world.seed = (uint64_t)obj["seed"].toString().toLongLong(&seedOk);
        m.world.mc = str2mc(obj["version"].toString().toUtf8().constData());
        m.world.large = obj["largeBiomes"].toBool();
        m.dimension = obj["dimension"].toInt(DIM_UNDEF);
        const QString kind = obj["kind"].toString();
        m.kind = kind == "custom" ? D_NONE : str2mapopt(kind.toUtf8().constData());
        m.x = obj["x"].toDouble();
        m.z = obj["z"].toDouble();
        m.name = obj["name"].toString();
        m.notes = obj["notes"].toString();
        m.completed = obj["completed"].toBool();
        if (m.id.isEmpty() || ids.contains(m.id) || !seedOk || m.world.mc <= MC_UNDEF ||
            m.world.mc > MC_NEWEST || !obj["largeBiomes"].isBool() || !obj["completed"].isBool() ||
            (m.dimension != DIM_OVERWORLD && m.dimension != DIM_NETHER && m.dimension != DIM_END) ||
            (kind != "custom" && (m.kind < D_DESERT || m.kind >= D_STRUCT_NUM)) ||
            !obj["x"].isDouble() || !obj["z"].isDouble() || !std::isfinite(m.x) || !std::isfinite(m.z) ||
            std::abs(m.x) > 30000000 || std::abs(m.z) > 30000000 || m.name.trimmed().isEmpty() ||
            m.name.size() > 200 || m.notes.size() > 4000)
        {
            error = tr("A marker has invalid or duplicate data. Nothing was imported or overwritten.");
            return false;
        }
        ids.insert(m.id);
        loaded.append(m);
    }
    records = loaded;
    hide = root["hideCompleted"].toBool();
    labels = root["showLabels"].toBool();
    return true;
}

bool MarkerStore::reload()
{
    QVector<SavedMarker> loaded;
    bool newHide = false, newLabels = false;
    QString message;
    if (!read(path, loaded, newHide, newLabels, message)) return fail(message);
    records = loaded;
    hide = newHide;
    labels = newLabels;
    error.clear();
    emit changed();
    emit statusChanged();
    return true;
}

bool MarkerStore::change(const std::function<void(QVector<SavedMarker>&, bool&, bool&)>& edit)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return fail(tr("Cannot create the marker folder. Check its write permissions."));
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(0)) return fail(tr("Another viewer is saving markers. Please try again."));
    QVector<SavedMarker> next;
    bool newHide = false, newLabels = false;
    QString message;
    if (!read(path, next, newHide, newLabels, message)) return fail(message);
    edit(next, newHide, newLabels);
    QSaveFile file(path);
    const QByteArray data = encode(next, newHide, newLabels);
    if (data.size() > 16 * 1024 * 1024) return fail(tr("Marker data exceeds the 16 MB limit. No changes were saved."));
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return fail(tr("Markers could not be saved: %1").arg(file.errorString()));
    records = next;
    hide = newHide;
    labels = newLabels;
    error.clear();
    emit changed();
    emit statusChanged();
    return true;
}

SavedMarker MarkerStore::find(const QString& id) const
{
    for (const SavedMarker& m : records) if (m.id == id) return m;
    return SavedMarker();
}

SavedMarker MarkerStore::findPlace(const SavedMarker& marker) const
{
    for (const SavedMarker& m : records) if (m.samePlace(marker)) return m;
    return SavedMarker();
}

bool MarkerStore::put(SavedMarker& marker)
{
    if (!std::isfinite(marker.x) || !std::isfinite(marker.z) || std::abs(marker.x) > 30000000 ||
        std::abs(marker.z) > 30000000 || marker.name.trimmed().isEmpty() || marker.name.size() > 200 ||
        marker.notes.size() > 4000 || marker.world.mc <= MC_UNDEF || marker.world.mc > MC_NEWEST ||
        (marker.dimension != DIM_OVERWORLD && marker.dimension != DIM_NETHER && marker.dimension != DIM_END) ||
        (marker.kind != D_NONE && (marker.kind < D_DESERT || marker.kind >= D_STRUCT_NUM)))
        return fail(tr("Enter a name (up to 200 characters), notes up to 4000 characters, and valid world coordinates."));
    marker.name = marker.name.trimmed();
    return change([&](QVector<SavedMarker>& list, bool&, bool&) {
        for (SavedMarker& m : list)
        {
            if (!marker.id.isEmpty() && m.id == marker.id) { m = marker; return; }
            if (marker.id.isEmpty() && m.samePlace(marker))
            { m.completed = m.completed || marker.completed; marker = m; return; }
        }
        if (marker.id.isEmpty()) marker.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        list.append(marker);
    });
}

bool MarkerStore::remove(const QString& id)
{
    return change([&](QVector<SavedMarker>& list, bool&, bool&) {
        for (int i = list.size()-1; i >= 0; --i) if (list[i].id == id) list.removeAt(i);
    });
}

bool MarkerStore::setCompleted(const QString& id, bool completed)
{
    return change([&](QVector<SavedMarker>& list, bool&, bool&) {
        for (SavedMarker& m : list) if (m.id == id) m.completed = completed;
    });
}

bool MarkerStore::setPreferences(bool hideCompleted, bool showLabels)
{
    return change([&](QVector<SavedMarker>&, bool& h, bool& l) { h = hideCompleted; l = showLabels; });
}

bool MarkerStore::exportBackup(const QString& destination)
{
    if (QFileInfo(destination).absoluteFilePath() == QFileInfo(path).absoluteFilePath())
        return fail(tr("Choose a separate backup file, rather than the live marker file."));
    if (!reload()) return false;
    QSaveFile file(destination);
    const QByteArray data = encode(records, hide, labels);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return fail(tr("Backup could not be saved: %1").arg(file.errorString()));
    return true;
}

bool MarkerStore::importBackup(const QString& source, int *added)
{
    QVector<SavedMarker> incoming;
    bool unusedHide = false, unusedLabels = false;
    QString message;
    if (!QFileInfo::exists(source)) return fail(tr("Backup file does not exist."));
    if (!read(source, incoming, unusedHide, unusedLabels, message)) return fail(message);
    int count = 0;
    const bool ok = change([&](QVector<SavedMarker>& list, bool&, bool&) {
        for (SavedMarker m : incoming)
        {
            bool exists = false;
            for (const SavedMarker& old : list) exists |= old.id == m.id || old.samePlace(m);
            if (!exists) { list.append(m); count++; }
        }
    });
    if (added) *added = ok ? count : 0;
    return ok;
}
