#include "SimuladorDron.h"

#include "Protocolo/ProyeccionMercator.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
// Origen de las coordenadas del cuadroptero: manda metros Mercator RELATIVOS a
// este punto (la estacion se lo suma al recibir, ver
// CControladora::onNewCuadropteros).
const double kOrigenCuadroLat = 23.09759;
const double kOrigenCuadroLon = -82.44113;

// Limita \a v a [-tope, tope].
double acotar(double v, double tope)
{
    return std::max(-tope, std::min(tope, v));
}

// Diferencia de rumbos en (-180, 180].
double diferencia(double a, double b)
{
    double d = std::fmod(a - b, 360.0);
    if (d > 180.0)
        d -= 360.0;
    if (d <= -180.0)
        d += 360.0;
    return d;
}
}

// Arranca en la orbita de casa (al norte, rumbo este: vuelta en sentido
// horario) y prepara el reloj de la telemetria.
SimuladorDron::SimuladorDron(const Opciones &opciones, QObject *parent)
    : QObject(parent), m_op(opciones)
{
    m_casa = m_op.casa;
    m_pos = m_casa.atDistanceAndAzimuth(m_op.radioOrbita, 0.0);
    m_rumbo = 90.0;
    m_rumboDeseado = 90.0;
    m_velocidad = m_op.velocidad;
    m_altura = m_op.altura;
    m_regimen = m_op.regimen;

    m_reloj.setInterval(int(std::max(10.0, 1000.0 / std::max(0.1, m_op.hz))));
    connect(&m_reloj, &QTimer::timeout, this, [this]() {
        const double dt = m_desde.isValid() ? m_desde.restart() / 1000.0 : 1.0 / m_op.hz;
        emit bytesSalida(avanzar(dt));
    });
}

// Empieza a mandar telemetria.
void SimuladorDron::arrancar()
{
    m_desde.start();
    m_reloj.start();
}

// Deja de mandar telemetria.
void SimuladorDron::parar()
{
    m_reloj.stop();
}

// Rumbo en [0, 360).
double SimuladorDron::normalizar(double grados)
{
    double g = std::fmod(grados, 360.0);
    return g < 0.0 ? g + 360.0 : g;
}

// Un paso de simulacion: decide a donde ir (siguiente punto del plan o la
// orbita de casa), gira, acelera, sube o baja y avanza. Devuelve la trama.
QByteArray SimuladorDron::avanzar(double dt)
{
    dt = std::max(0.001, std::min(dt, 1.0));   // un salto largo no teletransporta
    double deseado = m_rumbo;
    double alturaObj = m_op.altura;
    double velocidadObj = m_op.velocidad;

    if (m_regimen == 2 && !m_plan.isEmpty()) {
        // Distancia en metros Mercator, como la mide el autopiloto con su radio.
        auto distanciaMercator = [this](const Punto &p) {
            const QPointF m = ProyeccionMercator::aMetros(m_pos);
            return std::hypot(m.x() - p.este, m.y() - p.norte);
        };
        double d = distanciaMercator(m_plan.at(m_indice));
        if (d <= std::max(m_radio, m_velocidad * dt)) {
            emit aviso(tr("Punto %1 cumplido").arg(m_indice + 1));
            m_indice = (m_indice + 1) % m_plan.size();
            d = distanciaMercator(m_plan.at(m_indice));
        }
        const Punto &p = m_plan.at(m_indice);
        deseado = m_pos.azimuthTo(p.pos);
        if (p.altura > 0.0)
            alturaObj = p.altura;
        if (p.velocidad > 0.0)
            velocidadObj = p.velocidad;
        m_distanciaPunto = d;
    } else {
        // Orbita de casa en sentido horario: tangente, corregida hacia dentro o
        // hacia fuera segun lo lejos que este del radio.
        const double dist = m_casa.distanceTo(m_pos);
        const double radial = m_casa.azimuthTo(m_pos);
        deseado = radial + 90.0 + acotar((dist - m_op.radioOrbita) * 0.5, 80.0);
        m_distanciaPunto = dist;
    }
    m_rumboDeseado = normalizar(deseado);

    // Giro con ritmo maximo; el alabeo, en proporcion al ritmo de giro.
    const double ritmoMax = m_op.tipo == Tipo::AlaFija ? 20.0 : 45.0;   // grados/s
    const double giro = acotar(diferencia(m_rumboDeseado, m_rumbo), ritmoMax * dt);
    m_rumbo = normalizar(m_rumbo + giro);
    m_alabeo = acotar(giro / dt * 1.5, m_op.tipo == Tipo::AlaFija ? 35.0 : 20.0);

    // Velocidad y altura, poco a poco.
    m_velocidad += acotar(velocidadObj - m_velocidad, 2.0 * dt);
    const double vz = acotar((alturaObj - m_altura) * 0.5, 3.0);
    m_altura += vz * dt;
    m_cabeceo = acotar(vz * 4.0, 15.0);

    m_pos = m_pos.atDistanceAndAzimuth(m_velocidad * dt, m_rumbo);
    return trama();
}

// Arma la telemetria del estado actual, con los campos que lee la estacion.
QByteArray SimuladorDron::trama() const
{
    const bool automatico = m_regimen == 2 && !m_plan.isEmpty();
    const Punto proximo = automatico ? m_plan.at(m_indice) : Punto();

    if (m_op.tipo == Tipo::AlaFija) {
        sTRAMA1 t1;
        t1.Realimentacion_Cabeceo = float(m_cabeceo);
        t1.Realimentacion_Banqueo = float(m_alabeo);
        t1.Realimentacion_Velocidad = float(m_velocidad);
        t1.Realimentacion_Altura = float(m_altura);
        t1.Realimentacion_Curso = float(m_rumbo);
        t1.Realimentacion_Latitud = float(m_pos.latitude());
        t1.Realimentacion_Longitud = float(m_pos.longitude());
        t1.Realimentacion_CursoIMO = float(m_rumbo);

        sTRAMA2 t2;
        t2.Control_Estabilizadores = float(m_cabeceo / 15.0);
        t2.Control_Alerones = float(m_alabeo / 35.0);
        t2.Control_Motor = float(m_velocidad / 30.0);
        t2.Control_Rumbo = float(diferencia(m_rumboDeseado, m_rumbo) / 180.0);
        t2.Convergencia_Cabeceo = float(std::abs(m_cabeceo) / 15.0);
        t2.Convergencia_Banqueo = float(std::abs(m_alabeo) / 35.0);
        t2.Distancia_Chequeo = float(m_distanciaPunto);
        t2.Curso_Deseado = float(m_rumboDeseado);
        t2.Ganancia_Cabeceo[0] = m_mandos.Ganancias_Cabeceo_Manual1;
        t2.Ganancia_Cabeceo[1] = m_mandos.Ganancias_Cabeceo_Manual2;
        t2.Ganancia_Banqueo[0] = m_mandos.Ganancias_Banqueo_Manual1;
        t2.Ganancia_Banqueo[1] = m_mandos.Ganancias_Banqueo_Manual2;
        t2.Ganancia_Velocidad[0] = m_mandos.Ganancias_Velocidad_Manual1;
        t2.Ganancia_Velocidad[1] = m_mandos.Ganancias_Velocidad_Manual2;
        t2.Ganancia_Altura[0] = m_mandos.Ganancias_Altura_Manual1;
        t2.Ganancia_Altura[1] = m_mandos.Ganancias_Altura_Manual2;
        t2.Ganancia_Curso = m_mandos.Ganancias_Curso_Manual;
        t2.Ganancia_Rumbo[0] = m_mandos.Ganancias_Rumbo_Manual1;
        t2.Ganancia_Rumbo[1] = m_mandos.Ganancias_Rumbo_Manual2;
        t2.Realimentacion_Regimen = char(m_regimen);
        t2.Realimentacion_ComendoMision = char(m_comando);
        // El punto a cumplir vuelve tal cual llego en el plan (norte, este,
        // altura): la estacion lo busca entre los suyos para pintar los cumplidos.
        t2.Cumplir_Punto[0] = float(proximo.norte);
        t2.Cumplir_Punto[1] = float(proximo.este);
        t2.Cumplir_Punto[2] = float(proximo.altura);
        t2.Cantidad_Satelites = 12;
        return Tramas::constante(t1, t2);
    }

    sCUADROPTEROS c;
    // Posicion en metros Mercator relativos al origen del cuadroptero: norte en
    // "Latitud", este en "Longitud".
    const QPointF m = ProyeccionMercator::aMetros(m_pos.latitude() - kOrigenCuadroLat,
                                                  m_pos.longitude() - kOrigenCuadroLon);
    c.Realimentacion_Latitud = float(m.y());
    c.Realimentacion_Longitud = float(m.x());
    c.Realimentacion_Cabeceo = float(m_cabeceo);
    c.Realimentacion_Banqueo = float(m_alabeo);
    c.Realimentacion_Velocidad = float(m_velocidad);
    c.Realimentacion_Altura = float(m_altura);
    c.Realimentacion_Curso = float(m_rumbo);
    c.Realimentacion_CursoIMU = float(m_rumbo);
    const double motor = 0.5 + m_velocidad / 60.0;
    c.ControlM1 = float(motor + m_cabeceo / 100.0 + m_alabeo / 100.0);
    c.ControlM2 = float(motor + m_cabeceo / 100.0 - m_alabeo / 100.0);
    c.ControlM3 = float(motor - m_cabeceo / 100.0 - m_alabeo / 100.0);
    c.ControlM4 = float(motor - m_cabeceo / 100.0 + m_alabeo / 100.0);
    c.Convergencia_Cabeceo = float(std::abs(m_cabeceo) / 15.0);
    c.Convergencia_Banqueo = float(std::abs(m_alabeo) / 20.0);
    c.Distancia_Chequeo = float(m_distanciaPunto);
    c.Curso_Deseado = float(m_rumboDeseado);
    c.Ganancias_Cabeceo[0] = m_mandos.Ganancias_Cabeceo_Manual1;
    c.Ganancias_Cabeceo[1] = m_mandos.Ganancias_Cabeceo_Manual2;
    c.Ganancias_Banqueo[0] = m_mandos.Ganancias_Banqueo_Manual1;
    c.Ganancias_Banqueo[1] = m_mandos.Ganancias_Banqueo_Manual2;
    c.Ganancias_Altura[0] = m_mandos.Ganancias_Altura_Manual1;
    c.Ganancias_Altura[1] = m_mandos.Ganancias_Altura_Manual2;
    c.Ganancias_Curso = m_mandos.Ganancias_Curso_Manual;
    c.Ganancias_Rumbo[0] = m_mandos.Ganancias_Rumbo_Manual1;
    c.Ganancias_Rumbo[1] = m_mandos.Ganancias_Rumbo_Manual2;
    c.Realimentacion_Regimen = char(m_regimen);
    c.Realimentacion_ComandoMision = char(m_comando);
    c.Cumplir_Punto[0] = float(proximo.norte);
    c.Cumplir_Punto[1] = float(proximo.este);
    c.Cumplir_Punto[2] = float(proximo.altura);
    c.Cantidad_Satelites = 12;
    return Tramas::cuadroptero(c);
}

// Bytes de la estacion: se decodifican y cada trama completa se atiende.
void SimuladorDron::recibir(const QByteArray &bytes)
{
    m_decodificador.agregar(bytes);
    for (const auto &e : m_decodificador.eventos()) {
        if (e.tipo == Tramas::DecodificadorEstacion::Tipo::Mandos)
            alRecibirMandos(e.mandos, e.sumaCorrecta);
        else
            alRecibirPlan(e.puntos, e.sumaCorrecta);
    }
}

// Mandos: regimen, comando, radio de acercamiento, casa y ganancias. Se
// contesta "xyz1" (o "xyz0" si la suma no cuadra, y no se aplican).
void SimuladorDron::alRecibirMandos(const sMANDOS &m, bool sumaCorrecta)
{
    if (!sumaCorrecta) {
        emit aviso(tr("Mandos con la suma mal: acuse negativo"));
        emit bytesSalida(Tramas::acuse("xyz", false));
        return;
    }
    m_mandos = m;
    m_regimen = std::max(0, std::min(2, int(m.Regimen)));
    m_comando = m.Comando_Mision;
    if (m.Radio_Acercamiento > 0.0f)
        m_radio = m.Radio_Acercamiento;
    if (m.Home[0] != 0.0f || m.Home[1] != 0.0f)   // norte, este (Mercator)
        m_casa = ProyeccionMercator::aGeo(QPointF(m.Home[1], m.Home[0]));
    emit aviso(tr("Mandos: régimen %1, comando %2, radio %3 m, casa %4")
                   .arg(m_regimen).arg(m_comando).arg(m_radio)
                   .arg(m_casa.toString(QGeoCoordinate::Degrees)));
    emit bytesSalida(Tramas::acuse("xyz", true));
}

// Plan: hasta 10 puntos en metros Mercator (norte, este), altura y velocidad.
// Los puntos a cero no cuentan. Se contesta "uvw1" (o "uvw0" con la suma mal,
// y entonces la estacion lo vuelve a mandar).
void SimuladorDron::alRecibirPlan(const sPUNTOXYZ *puntos, bool sumaCorrecta)
{
    if (!sumaCorrecta) {
        emit aviso(tr("Plan con la suma mal: acuse negativo"));
        emit bytesSalida(Tramas::acuse("uvw", false));
        return;
    }
    m_plan.clear();
    for (int i = 0; i < Tramas::kPuntosPlan; ++i) {
        const sPUNTOXYZ &s = puntos[i];
        if (s.x == 0.0f && s.y == 0.0f)
            continue;
        Punto p;
        p.norte = s.x;
        p.este = s.y;
        p.altura = s.z;
        p.velocidad = s.velocidad;
        p.pos = ProyeccionMercator::aGeo(QPointF(p.este, p.norte));
        m_plan.append(p);
    }
    m_indice = 0;
    emit aviso(tr("Plan recibido: %1 puntos%2").arg(m_plan.size())
                   .arg(m_regimen == 2 ? QString() : tr(" (se sigue en régimen automático)")));
    emit bytesSalida(Tramas::acuse("uvw", true));
}
