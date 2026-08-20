#ifndef WIDGET_ESTADO_CUADROPTERO_H
#define WIDGET_ESTADO_CUADROPTERO_H

#include <QWidget>

namespace Ui {
class Widget_Estado_Cuadroptero;
}

class Widget_Estado_Cuadroptero : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Estado_Cuadroptero(QWidget *parent = nullptr);
    ~Widget_Estado_Cuadroptero();

private:
    Ui::Widget_Estado_Cuadroptero *ui;
};

#endif // WIDGET_ESTADO_CUADROPTERO_H
