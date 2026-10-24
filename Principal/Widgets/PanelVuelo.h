#ifndef PANELVUELO_H
#define PANELVUELO_H

#include <QColor>
#include <QFrame>
#include <QGeoCoordinate>
#include <QHash>

#include <limits>

class QLabel;

/*!
 * \brief Panel de vuelo sobre el mapa: lo que importa en vuelo, en grande, con
 *        unidades y colores de alerta.
 *
 * Sustituye en vuelo al panel tecnico (Widget_Parametros, con ganancias y
 * actuadores), que sigue disponible en Estado_Drone -> Detalle tecnico.
 *
 * Muestra: regimen (con el color del avion: rojo manual, amarillo estabilizado,
 * verde automatico), mision, punto al que va ("P3 / 10", o "✓ 10/10" con la
 * ruta cumplida), distancia al punto, rumbo actual -> deseado y satelites (rojo
 * por debajo de 6, ambar por debajo de 8). Debajo, la posicion del dron.
 */
class PanelVuelo : public QFrame
{
    Q_OBJECT
public:
    //! Lo que se muestra; lo que no se sepa se deja como esta (-1 / NaN).
    struct Datos {
        int regimen = -1;                 //!< 0 manual, 1 estabilizado, 2 automatico
        int mision = -1;                  //!< 0 puntos, 1 circular, 2 repetir, 3 RTL
        int puntoActual = -1;             //!< indice del punto al que va (0..), -1 = ninguno
        int puntosCumplidos = 0;
        int totalPuntos = 0;
        double distanciaPunto = std::numeric_limits<double>::quiet_NaN();   //!< m
        double rumbo = std::numeric_limits<double>::quiet_NaN();            //!< grados
        double rumboDeseado = std::numeric_limits<double>::quiet_NaN();     //!< grados
        int satelites = -1;
        QGeoCoordinate posicion;
    };

    explicit PanelVuelo(QWidget *parent = nullptr);

    //! Pone los datos de una trama.
    void mostrar(const Datos &d);
    //! Vacia el panel (al desconectar).
    void limpiar();
    //! Aviso en rojo en la linea de abajo (p. ej. "SIN DATOS hace 5 s");
    //! vacio lo quita y vuelve a la posicion del dron.
    void setAviso(const QString &aviso);

    //! Texto y color de un campo ("regimen", "mision", "punto", "distancia",
    //! "rumbo", "satelites", "posicion"). Para pruebas.
    QString valor(const QString &campo) const;
    QColor color(const QString &campo) const;

private:
    void poner(const QString &campo, const QString &texto, const QColor &color);

    QHash<QString, QLabel *> m_valores;
    QString m_aviso;
    QString m_textoPosicion;
};

#endif // PANELVUELO_H
