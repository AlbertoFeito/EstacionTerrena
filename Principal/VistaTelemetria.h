#ifndef VISTATELEMETRIA_H
#define VISTATELEMETRIA_H

#include "Vuelos/MuestraVuelo.h"

#include <QObject>

class MapaMision;
class PanelVuelo;
class Widget_Graficas;
class qfi_EADI;

/*!
 * \brief Muestra lo que dice la telemetria: horizonte artificial, panel de
 *        vuelo (con su detalle tecnico), el dron en el mapa con sus rumbos y
 *        las graficas en vivo.
 *
 * Sale de CControladora, donde esto mismo estaba tres veces (trama de ala
 * fija, de cuadroptero y vuelo reproducido). Recibe una MuestraVuelo
 * (Protocolo/Telemetria convierte las tramas), asi que en vivo y reproducido
 * se ven por el mismo camino. Lo que no es pintar (validar la posicion, fijar
 * la casa, los puntos cumplidos, grabar) sigue en CControladora.
 */
class VistaTelemetria : public QObject
{
    Q_OBJECT
public:
    //! En que punto de la ruta va el dron, para el panel de vuelo.
    struct Ruta
    {
        int puntoActual = -1;   //!< indice del punto al que va, -1 ninguno
        int cumplidos = 0;
        int total = 0;
    };

    VistaTelemetria(qfi_EADI *horizonte, PanelVuelo *panel, MapaMision *mapa,
                    Widget_Graficas *graficas, QObject *padre = nullptr);

    /*!
     * \brief Pone una muestra en el horizonte, el panel y el mapa.
     *
     * \a tipoDron: 0 ala fija, 1 cuadroptero (icono, nombres de actuadores y
     * ganancias). Con \a rumbosComoLibreriaAntigua, las lineas de rumbo
     * actual y deseado se pasan cambiadas, como hacia la libreria antigua con
     * el cuadroptero en vivo (se conserva tal cual hasta saber si es a
     * proposito).
     */
    void mostrar(const MuestraVuelo &m, int tipoDron, const Ruta &ruta,
                 bool rumbosComoLibreriaAntigua = false);

    //! Anade la muestra a las graficas en vivo, en el segundo \a segundos.
    void graficar(const MuestraVuelo &m, int tipoDron, double segundos);

    //! Horizonte a cero (sin tocar el panel).
    void instrumentosACero();
    //! Horizonte a cero y panel de vuelo vacio, con el detalle tecnico sin
    //! datos (al desconectar).
    void ponerACero(int tipoDron);

private:
    qfi_EADI *m_horizonte = nullptr;
    PanelVuelo *m_panel = nullptr;
    MapaMision *m_mapa = nullptr;
    Widget_Graficas *m_graficas = nullptr;
};

#endif // VISTATELEMETRIA_H
