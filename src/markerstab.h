#ifndef MARKERSTAB_H
#define MARKERSTAB_H

#include "markers.h"
#include <QDialog>
#include <QWidget>

class MainWindow;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

class MarkerEditor : public QDialog
{
    Q_OBJECT
public:
    MarkerEditor(MarkerStore *store, SavedMarker marker, QWidget *parent = nullptr);
};

class MarkersTab : public QWidget
{
    Q_OBJECT
public:
    explicit MarkersTab(MainWindow *window, MarkerStore *store);
    QString selectedId() const;

public slots:
    void refresh();
    void goToSelected();
    void editSelected();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void showSaveError();
    MainWindow *window;
    MarkerStore *store;
    QTreeWidget *table;
    QLineEdit *search;
    QComboBox *status, *kind, *dimension;
    QCheckBox *hide, *labels;
    QLabel *summary, *context, *storage;
    QPushButton *go, *edit, *remove, *copy;
};

#endif
