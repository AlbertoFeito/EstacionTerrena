#ifndef PERFILALTITUDWIDGET_H
#define PERFILALTITUDWIDGET_H

#include <QWidget>
#include <QVector>
#include "graficar/qcustomplot.h"
#include "Estructuras/E_Punto.h"
#include "AlturaWorker.h"

class PerfilAltitudWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PerfilAltitudWidget(AlturaWorker *alturaWorker, QWidget *parent = nullptr);
    ~PerfilAltitudWidget();

    void setRuta(const QList<E_Punto> &puntos);
    void generarPerfil();
    void limpiar();

signals:
    void puntoSeleccionado(int indice); // opcional, para sincronizar con la tabla

private slots:
    void onGraphClicked(QMouseEvent *event);

private:
    void configurarGrafico();
    void dibujarPerfil();
    void muestrearTerreno();

    QCustomPlot *m_plot;
    AlturaWorker *m_alturaWorker;
    QList<E_Punto> m_puntos;
    QVector<double> m_distanciasAcumuladas;
    QVector<double> m_alturasVuelo;
    QVector<double> m_distanciasMuestreo;
    QVector<double> m_alturasTerreno;
    double m_distanciaTotal;
};

#endif // PERFILALTITUDWIDGET_H
