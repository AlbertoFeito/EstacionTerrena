#ifndef WIDGET_MANDOS_CUADROPTERO_H
#define WIDGET_MANDOS_CUADROPTERO_H

#include <QWidget>

namespace Ui {
class Widget_Mandos_Cuadroptero;
}

class Widget_Mandos_Cuadroptero : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Mandos_Cuadroptero(QWidget *parent = nullptr);
    ~Widget_Mandos_Cuadroptero();

private:
    Ui::Widget_Mandos_Cuadroptero *ui;
};

#endif // WIDGET_MANDOS_CUADROPTERO_H
