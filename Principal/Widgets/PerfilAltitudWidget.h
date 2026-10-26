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

    //! Pone la ruta y rehace el perfil. \a trayectoria es la altura a la que
    //! iria de verdad (RevisionRuta::perfilReal: distancia, altura); si se da
    //! y se aparta del plan, se dibuja discontinua y las cruces rojas la usan.
    void setRuta(const QList<E_Punto> &puntos, const QVector<QPointF> &trayectoria = {});
    void generarPerfil();
    void limpiar();

signals:
    void puntoSeleccionado(int indice); // opcional, para sincronizar con la tabla

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onGraphClicked(QMouseEvent *event);

private:
    void configurarGrafico();
    void dibujarPerfil();
    void muestrearTerreno();

    QCustomPlot *m_plot;
    AlturaWorker *m_alturaWorker;
    QList<E_Punto> m_puntos;
    QVector<QPointF> m_trayectoria;   // altura probable por distancia (vacia: la del plan)
    QVector<double> m_distanciasAcumuladas;
    QVector<double> m_alturasVuelo;
    QVector<double> m_distanciasMuestreo;
    QVector<double> m_alturasTerreno;
    double m_distanciaTotal;
};

#endif // PERFILALTITUDWIDGET_H
