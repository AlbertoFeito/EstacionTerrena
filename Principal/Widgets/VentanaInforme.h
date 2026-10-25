#ifndef VENTANAINFORME_H
#define VENTANAINFORME_H

#include "Planificacion/RoutePlanner.h"

#include <QDialog>

class QTextBrowser;

/*!
 * \brief La ventana del analisis de la ruta: el informe de InformeRuta.
 *
 * No es modal: se puede dejar abierta mientras se corrige la ruta, y se
 * rehace sola (Widget_Puntos le pasa la revision nueva con cada cambio). Los
 * lugares son enlaces que llevan a su punto de la tabla.
 */
class VentanaInforme : public QDialog
{
    Q_OBJECT
public:
    explicit VentanaInforme(QWidget *padre = nullptr);

    //! Pone el informe de \a r (sin mover la barra si ya se estaba leyendo).
    void mostrar(const RevisionRuta &r, const DroneCharacteristics &dron, const QString &ruta,
                 const QList<E_Punto> &puntos);

signals:
    //! El operador ha pinchado en el lugar de un problema o en un tramo.
    void puntoElegido(int fila);

private:
    void exportar();

    QTextBrowser *m_texto = nullptr;
    QString m_plano;     // el mismo informe en texto, para exportar
    QString m_ruta;
};

#endif // VENTANAINFORME_H
