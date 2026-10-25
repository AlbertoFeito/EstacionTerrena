#include "Telemetria.h"

#include "ProyeccionMercator.h"

#include <cmath>

namespace Telemetria {

// Lo mismo que hacia la estacion (fmodf(360 + x, 360)): vale para lo que
// manda el autopiloto, entre -360 y 360.
double rumbo0a360(double grados)
{
    return std::fmod(360.0 + grados, 360.0);
}

// Copia el punto al que va el dron: norte, este y altura, como en el plan.
static void ponerPunto(MuestraVuelo &m, const float (&cumplir)[3])
{
    m.puntoNorte = cumplir[0];
    m.puntoEste = cumplir[1];
    m.puntoAltura = cumplir[2];
}

// Campo a campo, como lo leia CControladora::onNewTramaConstanteReceived.
MuestraVuelo desdeAlaFija(const sTRAMA1 &t1, const sTRAMA2 &t2)
{
    MuestraVuelo m;
    m.lat = t1.Realimentacion_Latitud;
    m.lon = t1.Realimentacion_Longitud;
    m.cabeceo = t1.Realimentacion_Cabeceo;
    m.banqueo = t1.Realimentacion_Banqueo;
    m.velocidadKmh = t1.Realimentacion_Velocidad * 3.6;
    m.altura = t1.Realimentacion_Altura;
    m.curso = t1.Realimentacion_Curso;
    m.cursoImu = t1.Realimentacion_CursoIMO;
    m.distanciaChequeo = t2.Distancia_Chequeo;
    m.cursoDeseado = rumbo0a360(t2.Curso_Deseado);
    m.regimen = t2.Realimentacion_Regimen;
    m.mision = t2.Realimentacion_ComendoMision;
    m.satelites = t2.Cantidad_Satelites;
    const float ganancias[11] = {t2.Ganancia_Cabeceo[0], t2.Ganancia_Cabeceo[1],
                                 t2.Ganancia_Banqueo[0], t2.Ganancia_Banqueo[1],
                                 t2.Ganancia_Velocidad[0], t2.Ganancia_Velocidad[1],
                                 t2.Ganancia_Altura[0], t2.Ganancia_Altura[1],
                                 t2.Ganancia_Curso,
                                 t2.Ganancia_Rumbo[0], t2.Ganancia_Rumbo[1]};
    for (int i = 0; i < 11; ++i)
        m.ganancias[i] = ganancias[i];
    m.actuadores[0] = t2.Control_Estabilizadores;
    m.actuadores[1] = t2.Control_Alerones;
    m.actuadores[2] = t2.Control_Motor;
    m.actuadores[3] = t2.Control_Rumbo;
    m.convergenciaBanqueo = t2.Convergencia_Banqueo;
    m.convergenciaCabeceo = t2.Convergencia_Cabeceo;
    ponerPunto(m, t2.Cumplir_Punto);
    return m;
}

// Como CControladora::onNewCuadropteros: inverse(lat, lon) de la libreria
// antigua (el norte va en la latitud de la trama y el este en la longitud) y
// el mismo desplazamiento fijo que ya sumaba la estacion.
MuestraVuelo desdeCuadroptero(const sCUADROPTEROS &c)
{
    MuestraVuelo m;
    const QGeoCoordinate latlon = ProyeccionMercator::aGeo(
        QPointF(c.Realimentacion_Longitud, c.Realimentacion_Latitud));
    m.lat = latlon.latitude() + 23.09759;
    m.lon = latlon.longitude() - 82.44113;
    m.cabeceo = c.Realimentacion_Cabeceo;
    m.banqueo = c.Realimentacion_Banqueo;
    m.velocidadKmh = c.Realimentacion_Velocidad * 3.6;
    m.altura = c.Realimentacion_Altura;
    m.curso = c.Realimentacion_Curso;
    m.cursoImu = c.Realimentacion_CursoIMU;
    m.distanciaChequeo = c.Distancia_Chequeo;
    m.cursoDeseado = rumbo0a360(c.Curso_Deseado);
    m.regimen = c.Realimentacion_Regimen;
    m.mision = c.Realimentacion_ComandoMision;
    m.satelites = int(c.Cantidad_Satelites);
    m.actuadores[0] = c.ControlM1;
    m.actuadores[1] = c.ControlM2;
    m.actuadores[2] = c.ControlM3;
    m.actuadores[3] = c.ControlM4;
    m.convergenciaBanqueo = c.Convergencia_Banqueo;
    m.convergenciaCabeceo = c.Convergencia_Cabeceo;
    ponerPunto(m, c.Cumplir_Punto);
    return m;
}

// Cumplir_Punto va con el norte primero; aGeo quiere (este, norte).
QGeoCoordinate puntoQueVa(const MuestraVuelo &m)
{
    if (m.puntoNorte == 0.0 && m.puntoEste == 0.0)
        return QGeoCoordinate();
    return ProyeccionMercator::aGeo(QPointF(m.puntoEste, m.puntoNorte));
}

// Ala fija: su motor (tercer actuador); cuadroptero: la media de los cuatro.
double motorParaGrafica(const MuestraVuelo &m, int tipoDron)
{
    if (tipoDron == 0)
        return m.actuadores[2];
    return (m.actuadores[0] + m.actuadores[1] + m.actuadores[2] + m.actuadores[3]) / 4.0;
}

} // namespace Telemetria
