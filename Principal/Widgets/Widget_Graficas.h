#ifndef WIDGET_GRAFICAS_H
#define WIDGET_GRAFICAS_H

#include <QtWidgets>
//#include "qcustomplot.h"
#include <graficar/customplot.h>
namespace Ui {
class Widget_Graficas;
}

class Widget_Graficas : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Graficas(QWidget *parent = nullptr);
    ~Widget_Graficas();

    QVector<double> getBanqueo() const;
    void setBanqueo(const QVector<double> &value);

    QVector<double> getCabeceo() const;
    void setCabeceo(const QVector<double> &value);

    QVector<double> getRPM() const;
    void setRPM(const QVector<double> &value);

    QVector<double> getAltura() const;
    void setAltura(const QVector<double> &value);

    QVector<double> getVelocidad() const;
    void setVelocidad(const QVector<double> &value);

    void pintar();
    void preparaGraficas();

    QVector<double> getTiempo() const;
    void setTiempo(const QVector<double> &value);

    QString getVuelo() const;
    void setVuelo(const QString &value);

    QString getDronActivo() const;
    void setDronActivo(const QString &value);


    float Map(float x, float in_min, float in_max, float out_min, float out_max);

    QLineEdit *getLEdron() const;

    QLineEdit *getLEvuelo() const;

    QPushButton *getMpBCerrar() const;

public slots:
    void datosTiempoReal(double Banq,
    double Cab,
    double rpm,
    double Alt,
    double Vel,
    double tiem);
protected slots:
    void slotMousePress(QMouseEvent *event);
    void slotMouseMove(QMouseEvent *event);


private slots:
    void on_pB_Cerrar_clicked();

private:
    Ui::Widget_Graficas *ui;

    CustomPlot *customPlot;

    QVector<double> Banqueo;
    QVector<double> Cabeceo;
    QVector<double> RPM;
    QVector<double> Altura;
    QVector<double> Velocidad;
    QVector<double> tiempo;
    QString dronActivo;
    QString Vuelo;

    QLineEdit *lEdron;
    QLineEdit *lEvuelo;
    QPushButton *mpBCerrar;
};

#endif // WIDGET_GRAFICAS_H
