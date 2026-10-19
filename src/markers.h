#ifndef MARKERS_H
#define MARKERS_H

#include "config.h"
#include <QObject>
#include <QVector>
#include <functional>

struct SavedMarker
{
    QString id, name, notes;
    WorldInfo world;
    int dimension = DIM_OVERWORLD;
    int kind = D_NONE;
    qreal x = 0, z = 0;
    bool completed = false;

    bool matchesWorld(const WorldInfo& wi) const;
    bool samePlace(const SavedMarker& other) const;
    QString typeName() const;
    static QString dimensionName(int dimension);
    static QString coordinate(qreal value);
    static QString structureKey(int kind, qreal x, qreal z);
};

// A small atomic JSON file, independent of search/session settings. Every edit
// reloads under a file lock so two viewer instances do not overwrite each other.
class MarkerStore : public QObject
{
    Q_OBJECT
public:
    explicit MarkerStore(const QString& path = QString(), QObject *parent = nullptr);
    QString filePath() const { return path; }
    QString lastError() const { return error; }
    const QVector<SavedMarker>& all() const { return records; }
    bool hideCompleted() const { return hide; }
    bool showLabels() const { return labels; }
    bool reload();
    bool put(SavedMarker& marker);
    bool remove(const QString& id);
    bool setCompleted(const QString& id, bool completed);
    bool setPreferences(bool hideCompleted, bool showLabels);
    bool exportBackup(const QString& destination);
    bool importBackup(const QString& source, int *added = nullptr);
    SavedMarker find(const QString& id) const;
    SavedMarker findPlace(const SavedMarker& marker) const;

signals:
    void changed();
    void statusChanged();

private:
    bool fail(const QString& message);
    bool change(const std::function<void(QVector<SavedMarker>&, bool&, bool&)>& edit);
    static bool read(const QString& path, QVector<SavedMarker>& records,
                     bool& hide, bool& labels, QString& error);
    static QByteArray encode(const QVector<SavedMarker>& records, bool hide, bool labels);
    QString path, error;
    QVector<SavedMarker> records;
    bool hide = false, labels = false;
};

#endif
