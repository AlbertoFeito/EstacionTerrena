#ifndef WIDGET_PARAMETROS_H
#define WIDGET_PARAMETROS_H

#include <QtWidgets/QWidget>

namespace Ui {
class Widget_Parametros;
}

class Widget_Parametros : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Parametros(QWidget *parent = nullptr);
    ~Widget_Parametros();

    void updateValues(const QVector<double> &values);
    void updateSatelites(int cantSatelites);
    int tipo_dron() const;
    void setTipo_dron(int newTipo_dron);

private:
    Ui::Widget_Parametros *ui;

     int m_tipo_dron = 0;
};

#endif // WIDGET_PARAMETROS_H
