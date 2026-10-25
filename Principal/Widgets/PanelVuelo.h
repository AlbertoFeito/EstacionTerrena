#ifndef PANELVUELO_H
#define PANELVUELO_H

#include <QColor>
#include <QFrame>
#include <QGeoCoordinate>
#include <QHash>

#include <limits>

class QFrame;
class QLabel;
class QToolButton;

/*!
 * \brief Panel de vuelo sobre el mapa: lo que importa en vuelo, en grande, con
 *        unidades y colores de alerta.
 *
 * Debajo, plegado, el detalle tecnico (ganancias, actuadores, rumbo de la IMU,
 * convergencias): se abre con su boton o con Estado_Drone -> Detalle tecnico.
 * Antes era otro panel aparte (Widget_Parametros) que tapaba el mapa.
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

    //! Detalle tecnico; lo que no se sepa, NaN (sale un guion).
    struct Detalle {
        int tipoDron = 0;                 //!< 0 ala fija, 1 cuadroptero (nombres de los actuadores)
        //! P e I de cabeceo, banqueo, velocidad y altura; P de curso; P e I de rumbo.
        double ganancias[11];
        double actuadores[4];             //!< estabilizadores, alerones, motor, rumbo (o los 4 motores)
        double rumboImu = std::numeric_limits<double>::quiet_NaN();
        double convergenciaBanqueo = std::numeric_limits<double>::quiet_NaN();
        double convergenciaCabeceo = std::numeric_limits<double>::quiet_NaN();
        Detalle();
    };

    explicit PanelVuelo(QWidget *parent = nullptr);

    //! Pone los datos de una trama.
    void mostrar(const Datos &d);
    //! Vacia el panel (al desconectar).
    void limpiar();
    //! Aviso en rojo en la linea de abajo (p. ej. "SIN DATOS hace 5 s");
    //! vacio lo quita y vuelve a la posicion del dron.
    void setAviso(const QString &aviso);

    //! Pone el detalle tecnico de una trama (aunque este plegado).
    void mostrarDetalle(const Detalle &d);
    //! Abre o pliega el detalle tecnico (sin emitir detalleConmutado).
    void setDetalleVisible(bool visible);
    bool detalleVisible() const;

    //! Texto y color de un campo ("regimen", "mision", "punto", "distancia",
    //! "rumbo", "satelites", "posicion"; del detalle "g_cabeceo", "g_banqueo",
    //! "g_velocidad", "g_altura", "g_curso", "g_rumbo", "act0".."act3",
    //! "rumboImu", "convBanqueo", "convCabeceo"). Para pruebas.
    QString valor(const QString &campo) const;
    QColor color(const QString &campo) const;
    //! Nombre del actuador \a i (cambia con el tipo de dron). Para pruebas.
    QString nombreActuador(int i) const;

signals:
    //! El operador ha abierto o plegado el detalle con su boton.
    void detalleConmutado(bool visible);

private:
    void poner(const QString &campo, const QString &texto, const QColor &color);
    QFrame *crearDetalle();

    QHash<QString, QLabel *> m_valores;
    QFrame *m_detalle = nullptr;
    QToolButton *m_botonDetalle = nullptr;
    QLabel *m_nombresActuador[4] = {};
    QString m_aviso;
    QString m_textoPosicion;
};

#endif // PANELVUELO_H
