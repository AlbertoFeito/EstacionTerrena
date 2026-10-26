#ifndef VENTANAPROPUESTAS_H
#define VENTANAPROPUESTAS_H

#include "Planificacion/RoutePlanner.h"

#include <QDialog>
#include <QGeoCoordinate>
#include <QVector>

class AlturaWorker;
class PerfilAltitudWidget;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

/*!
 * \brief Las rutas que propone el optimizador (RoutePlanner::proponerRutas),
 *        para que el operador las compare y elija.
 *
 * Una fila por propuesta (Segura, Equilibrada, Rapida y la Recta) con sus
 * cifras; la elegida se ve en el perfil y, en el mapa, como vista previa
 * (vistaPrevia). «Usar esta ruta» la pasa a la tabla de puntos. No es modal.
 */
class VentanaPropuestas : public QDialog
{
    Q_OBJECT
public:
    VentanaPropuestas(AlturaWorker *alturaWorker, QWidget *padre = nullptr);

    //! Pone las propuestas y elige la Equilibrada (o la primera que se pudo).
    void mostrar(const QVector<PropuestaRuta> &propuestas);
    //! Ancho del pasillo a cada lado de la recta, en metros.
    double anchoPasillo() const;

signals:
    //! Hay que volver a calcular (ha cambiado el pasillo).
    void calcular();
    //! La ruta elegida, para verla en el mapa; vacia, quitarla.
    void vistaPrevia(const QVector<QGeoCoordinate> &camino);
    //! El operador se queda con esta ruta.
    void usar(const QList<E_Punto> &puntos);

protected:
    void hideEvent(QHideEvent *event) override;

private:
    void elegir(int fila);

    QSpinBox *m_pasillo = nullptr;
    QTableWidget *m_tabla = nullptr;
    PerfilAltitudWidget *m_perfil = nullptr;
    QLabel *m_detalle = nullptr;
    QPushButton *m_usar = nullptr;
    QVector<PropuestaRuta> m_propuestas;
};

#endif // VENTANAPROPUESTAS_H
