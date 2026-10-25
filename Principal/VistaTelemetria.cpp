#include "VistaTelemetria.h"

#include "Iconos/Iconos.h"
#include "Mapa/MapaMision.h"
#include "Protocolo/Telemetria.h"
#include "Widgets/PanelVuelo.h"
#include "Widgets/Widget_Graficas.h"
#include "qfi/qfi_EADI.h"

#include <limits>

// Guarda los widgets donde se pinta; no son suyos.
VistaTelemetria::VistaTelemetria(qfi_EADI *horizonte, PanelVuelo *panel, MapaMision *mapa,
                                 Widget_Graficas *graficas, QObject *padre)
    : QObject(padre), m_horizonte(horizonte), m_panel(panel), m_mapa(mapa), m_graficas(graficas)
{
}

// Color del dron y de su traza segun el regimen: rojo manual, ambar
// estabilizado, verde automatico (rojo si llega otra cosa, como antes).
static QColor colorRegimen(int regimen)
{
    if (regimen == 1)
        return Qt::yellow;
    if (regimen == 2)
        return Qt::green;
    return Qt::red;
}

// Horizonte, detalle tecnico, panel de vuelo y, con posicion, el dron en el
// mapa y sus lineas de rumbo (la morada al punto al que va, solo en
// automatico).
void VistaTelemetria::mostrar(const MuestraVuelo &m, int tipoDron, const Ruta &ruta,
                              bool rumbosComoLibreriaAntigua)
{
    const double cursoDeseado = Telemetria::rumbo0a360(m.cursoDeseado);
    const bool automatico = m.regimen == 2;

    // Detalle: el cuadroptero no manda ganancias, salen con guion.
    PanelVuelo::Detalle detalle;
    detalle.tipoDron = tipoDron;
    detalle.rumboImu = m.cursoImu;
    for (int i = 0; i < 11; ++i)
        detalle.ganancias[i] = tipoDron != 0 ? std::numeric_limits<double>::quiet_NaN() : m.ganancias[i];
    for (int i = 0; i < 4; ++i)
        detalle.actuadores[i] = m.actuadores[i];
    detalle.convergenciaBanqueo = m.convergenciaBanqueo;
    detalle.convergenciaCabeceo = m.convergenciaCabeceo;
    m_panel->mostrarDetalle(detalle);

    // El punto al que va solo tiene sentido en automatico.
    PanelVuelo::Datos d;
    d.regimen = m.regimen;
    d.mision = m.mision;
    d.puntoActual = automatico ? ruta.puntoActual : -1;
    d.totalPuntos = ruta.total;
    d.puntosCumplidos = ruta.cumplidos;
    d.distanciaPunto = m.distanciaChequeo;
    d.rumbo = m.curso;
    d.rumboDeseado = cursoDeseado;
    d.satelites = m.satelites;
    d.posicion = QGeoCoordinate(m.lat, m.lon);
    m_panel->mostrar(d);

    m_horizonte->setPitch(m.cabeceo);
    m_horizonte->setRoll(m.banqueo);
    m_horizonte->setHeading(m.curso);
    m_horizonte->setAirspeed(m.velocidadKmh);
    m_horizonte->setAltitude(m.altura);
    m_horizonte->redraw();

    if (m.lat == 0.0)
        return;   // sin posicion todavia: no hay dron que poner en el mapa
    const QColor color = colorRegimen(m.regimen);
    const QString rotulo = QStringLiteral("V: %1 km/h\nH: %2 m")
                               .arg(m.velocidadKmh, 0, 'f', 2).arg(m.altura, 0, 'f', 2);
    m_mapa->actualizarVehiculo(QGeoCoordinate(m.lat, m.lon), m.curso, color,
                               tipoDron == 0 ? Iconos::avion(color) : Iconos::cuadroptero(color),
                               rotulo);
    if (rumbosComoLibreriaAntigua)
        m_mapa->ponerRumbos(cursoDeseado, m.curso, Telemetria::puntoQueVa(m), automatico);
    else
        m_mapa->ponerRumbos(m.curso, cursoDeseado, Telemetria::puntoQueVa(m), automatico);
}

// Banqueo, cabeceo, motor, altura y velocidad (km/h) en las graficas en vivo.
void VistaTelemetria::graficar(const MuestraVuelo &m, int tipoDron, double segundos)
{
    m_graficas->datosTiempoReal(m.banqueo, m.cabeceo, Telemetria::motorParaGrafica(m, tipoDron),
                                m.altura, m.velocidadKmh, segundos);
}

// Horizonte con todo a cero.
void VistaTelemetria::instrumentosACero()
{
    m_horizonte->setRoll(0);
    m_horizonte->setPitch(0);
    m_horizonte->setHeading(0);
    m_horizonte->setAirspeed(0);
    m_horizonte->setAltitude(0);
    m_horizonte->redraw();
}

// Horizonte a cero, panel vacio y detalle sin datos (guiones).
void VistaTelemetria::ponerACero(int tipoDron)
{
    instrumentosACero();
    PanelVuelo::Detalle detalle;
    detalle.tipoDron = tipoDron;
    m_panel->mostrarDetalle(detalle);
    m_panel->limpiar();
}
