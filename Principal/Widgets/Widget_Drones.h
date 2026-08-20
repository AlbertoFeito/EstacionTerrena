#ifndef WIDGET_DRONES_H
#define WIDGET_DRONES_H

#include <QWidget>
#include "Database/ConnectionManager.h"
#include "Database/AsyncQuery.h"
#include "Database/AsynqQueryModel.h"

namespace Ui {
class Widget_Drones;
}

class Widget_Drones : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Drones(QWidget *parent = nullptr);
    ~Widget_Drones();
    Database::AsyncQueryModel *tableModel() const;
    void setTableModel(Database::AsyncQueryModel *tableModel);

    Database::AsyncQuery *aQuery() const;
    void setAQuery(Database::AsyncQuery *aQuery);

    void actualizaTabla();
    bool createTable();
signals:
    void nuevoDron();

private slots:
    void on_pB_Agregar_clicked();

    void on_pB_Eliminar_clicked();

private:
    Ui::Widget_Drones *ui;

    QString query1;
    Database::AsyncQuery *_aQuery;
    Database::AsyncQueryModel *_tableModel;

    // QWidget

protected:
};

#endif // WIDGET_DRONES_H
