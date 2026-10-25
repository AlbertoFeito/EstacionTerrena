#ifndef WIDGET_DRONES_H
#define WIDGET_DRONES_H

#include <QWidget>

class QSqlQueryModel;

namespace Ui {
class Widget_Drones;
}

class Widget_Drones : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Drones(QWidget *parent = nullptr);
    ~Widget_Drones();
    QSqlQueryModel *tableModel() const;

    void actualizaTabla();
    bool createTable();
signals:
    void nuevoDron();

private slots:
    void on_pB_Agregar_clicked();

    void on_pB_Eliminar_clicked();

private:
    Ui::Widget_Drones *ui;

    QSqlQueryModel *_tableModel = nullptr;

    // QWidget

protected:
};

#endif // WIDGET_DRONES_H
