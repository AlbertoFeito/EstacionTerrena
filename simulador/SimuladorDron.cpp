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
    m_comando = m_op.mision;

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

// Nombre de cada fase, para la consola.
QString SimuladorDron::nombreFase(Fase f)
{
    switch (f) {
    case Fase::VueltasCasa:    return tr("vueltas a casa");
    case Fase::Ruta:           return tr("siguiendo la ruta");
    case Fase::Circular:       return tr("circular sobre el punto");
    case Fase::MisionCumplida: return tr("misión cumplida (vueltas sobre el último punto)");
    case Fase::Regreso:        return tr("RTL: volviendo a casa");
    case Fase::Paracaidas:     return tr("paracaídas");
    }
    return QString();
}

// Cambia de fase y lo cuenta (solo si cambia).
void SimuladorDron::cambiarFase(Fase f)
{
    if (f == m_fase)
        return;
    m_fase = f;
    emit aviso(tr("Ahora: %1").arg(nombreFase(f)));
}

// Rumbo para dar vueltas en sentido horario alrededor de \a centro: la
// tangente, corregida hacia dentro o hacia fuera segun lo lejos que este del
// radio de las vueltas.
double SimuladorDron::rumboOrbita(const QGeoCoordinate &centro) const
{
    const double dist = centro.distanceTo(m_pos);
    const double radial = centro.azimuthTo(m_pos);
    return radial + 90.0 + acotar((dist - m_op.radioOrbita) * 0.5, 80.0);
}

// Distancia en metros MERCATOR de \a a a un punto (norte, este): la que usa el
// autopiloto con su radio de acercamiento.
double SimuladorDron::distanciaMercator(const QGeoCoordinate &a, double norte, double este) const
{
    const QPointF m = ProyeccionMercator::aMetros(a);
    return std::hypot(m.x() - este, m.y() - norte);
}

// Un paso de simulacion: decide la fase segun los mandos (regimen, mision,
// paracaidas) y el plan, elige rumbo, altura y velocidad objetivo, gira,
// acelera, sube o baja y avanza. Devuelve la trama de telemetria.
QByteArray SimuladorDron::avanzar(double dt)
{
    dt = std::max(0.001, std::min(dt, 1.0));   // un salto largo no teletransporta

    // --- Que toca hacer -----------------------------------------------------
    if (m_comando == 2)
        m_cumplida = false;   // "Repetir mision" reanuda una mision terminada
    Fase f;
    if (m_paracaidas)
        f = Fase::Paracaidas;
    else if (m_regimen != 2)
        f = Fase::VueltasCasa;
    else if (m_comando == 3)
        f = Fase::Regreso;
    else if (m_plan.isEmpty())
        f = Fase::VueltasCasa;
    else if (m_comando == 1)
        f = Fase::Circular;
    else if (m_cumplida)
        f = Fase::MisionCumplida;
    else
        f = Fase::Ruta;
    cambiarFase(f);

    double deseado = m_rumbo;
    double alturaObj = m_op.altura;
    double velocidadObj = m_op.velocidad;
    double bajada = 3.0;   // m/s maximos de subida o bajada

    // Lo que trae cada punto del plan (si no, lo de siempre).
    auto delPunto = [&](const Punto &p) {
        if (p.altura > 0.0)
            alturaObj = p.altura;
        if (p.velocidad > 0.0)
            velocidadObj = p.velocidad;
    };

    switch (m_fase) {
    case Fase::VueltasCasa:
        m_objetivo = Punto();
        deseado = rumboOrbita(m_casa);
        m_distanciaPunto = m_casa.distanceTo(m_pos);
        break;

    case Fase::Ruta: {
        const Punto &actual = m_plan.at(m_indice);
        if (distanciaMercator(m_pos, actual.norte, actual.este)
            <= std::max(m_radio, m_velocidad * dt)) {
            emit aviso(tr("Punto %1 de %2 cumplido").arg(m_indice + 1).arg(m_plan.size()));
            if (m_indice + 1 < m_plan.size()) {
                ++m_indice;
            } else if (m_comando == 2) {
                m_indice = 0;
                emit aviso(tr("Ruta completa: se repite desde el punto 1"));
            } else {
                m_cumplida = true;
                cambiarFase(Fase::MisionCumplida);
            }
        }
        m_objetivo = m_plan.at(m_indice);
        delPunto(m_objetivo);
        deseado = m_fase == Fase::Ruta ? m_pos.azimuthTo(m_objetivo.pos)
                                       : rumboOrbita(m_objetivo.pos);
        m_distanciaPunto = distanciaMercator(m_pos, m_objetivo.norte, m_objetivo.este);
        break;
    }

    case Fase::Circular:
    case Fase::MisionCumplida:
        m_objetivo = m_plan.at(m_indice);
        delPunto(m_objetivo);
        deseado = rumboOrbita(m_objetivo.pos);
        m_distanciaPunto = m_objetivo.pos.distanceTo(m_pos);
        break;

    case Fase::Regreso: {
        // A casa a V.RTH y a la altura de casa; ya alli, vueltas sobre ella.
        const QPointF c = ProyeccionMercator::aMetros(m_casa);
        m_objetivo = Punto();
        m_objetivo.pos = m_casa;
        m_objetivo.norte = c.y();
        m_objetivo.este = c.x();
        m_objetivo.altura = m_alturaCasa;
        if (m_velocidadRtl > 0.0)
            velocidadObj = m_velocidadRtl;
        if (m_alturaCasa > 0.0)
            alturaObj = m_alturaCasa;
        m_distanciaPunto = m_casa.distanceTo(m_pos);
        deseado = m_distanciaPunto > m_op.radioOrbita * 1.2 ? m_pos.azimuthTo(m_casa)
                                                             : rumboOrbita(m_casa);
        break;
    }

    case Fase::Paracaidas:
        velocidadObj = 0.0;
        alturaObj = 0.0;
        bajada = 5.0;
        break;
    }
    m_rumboDeseado = normalizar(deseado);

    // --- Movimiento -----------------------------------------------------------
    // Giro con ritmo maximo; el alabeo, en proporcion al ritmo de giro.
    const double ritmoMax = m_op.tipo == Tipo::AlaFija ? 20.0 : 45.0;   // grados/s
    const double giro = m_velocidad > 0.5
        ? acotar(diferencia(m_rumboDeseado, m_rumbo), ritmoMax * dt) : 0.0;
    m_rumbo = normalizar(m_rumbo + giro);
    m_alabeo = acotar(giro / dt * 1.5, m_op.tipo == Tipo::AlaFija ? 35.0 : 20.0);

    // Velocidad y altura, poco a poco (el paracaidas frena mas fuerte).
    m_velocidad += acotar(velocidadObj - m_velocidad,
                          (m_fase == Fase::Paracaidas ? 3.0 : 2.0) * dt);
    // Con paracaidas se baja a ritmo constante; si no, cada vez mas suave al
    // acercarse a la altura pedida.
    const double vz = m_fase == Fase::Paracaidas ? -bajada
                                                 : acotar((alturaObj - m_altura) * 0.5, bajada);
    m_altura = std::max(0.0, m_altura + vz * dt);
    m_cabeceo = acotar(vz * 4.0, 15.0);

    m_pos = m_pos.atDistanceAndAzimuth(m_velocidad * dt, m_rumbo);
    return trama();
}

// Arma la telemetria del estado actual, con los campos que lee la estacion.
QByteArray SimuladorDron::trama() const
{
    // El punto al que va (o sobre el que da vueltas); a cero si no hay.
    const Punto &proximo = m_objetivo;

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
        t2.Ganancia_Cabeceo[0] = m_ganancias.Ganancias_Cabeceo_Manual1;
        t2.Ganancia_Cabeceo[1] = m_ganancias.Ganancias_Cabeceo_Manual2;
        t2.Ganancia_Banqueo[0] = m_ganancias.Ganancias_Banqueo_Manual1;
        t2.Ganancia_Banqueo[1] = m_ganancias.Ganancias_Banqueo_Manual2;
        t2.Ganancia_Velocidad[0] = m_ganancias.Ganancias_Velocidad_Manual1;
        t2.Ganancia_Velocidad[1] = m_ganancias.Ganancias_Velocidad_Manual2;
        t2.Ganancia_Altura[0] = m_ganancias.Ganancias_Altura_Manual1;
        t2.Ganancia_Altura[1] = m_ganancias.Ganancias_Altura_Manual2;
        t2.Ganancia_Curso = m_ganancias.Ganancias_Curso_Manual;
        t2.Ganancia_Rumbo[0] = m_ganancias.Ganancias_Rumbo_Manual1;
        t2.Ganancia_Rumbo[1] = m_ganancias.Ganancias_Rumbo_Manual2;
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
    c.Ganancias_Cabeceo[0] = m_ganancias.Ganancias_Cabeceo_Manual1;
    c.Ganancias_Cabeceo[1] = m_ganancias.Ganancias_Cabeceo_Manual2;
    c.Ganancias_Banqueo[0] = m_ganancias.Ganancias_Banqueo_Manual1;
    c.Ganancias_Banqueo[1] = m_ganancias.Ganancias_Banqueo_Manual2;
    c.Ganancias_Altura[0] = m_ganancias.Ganancias_Altura_Manual1;
    c.Ganancias_Altura[1] = m_ganancias.Ganancias_Altura_Manual2;
    c.Ganancias_Curso = m_ganancias.Ganancias_Curso_Manual;
    c.Ganancias_Rumbo[0] = m_ganancias.Ganancias_Rumbo_Manual1;
    c.Ganancias_Rumbo[1] = m_ganancias.Ganancias_Rumbo_Manual2;
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

// Mandos, como los arma la pantalla de Mandos (y la de Ganancias): regimen,
// mision, casa y V.RTH, radio de acercamiento, paracaidas y ganancias (solo
// las marcadas con "Actualizar"). Se contesta "xyz1" (o "xyz0" si la suma no
// cuadra, y entonces no se aplica nada).
void SimuladorDron::alRecibirMandos(const sMANDOS &m, bool sumaCorrecta)
{
    if (!sumaCorrecta) {
        emit aviso(tr("Mandos con la suma mal: acuse negativo"));
        emit bytesSalida(Tramas::acuse("xyz", false));
        return;
    }
    m_regimen = std::max(0, std::min(2, int(m.Regimen)));
    m_comando = std::max(0, std::min(3, int(m.Comando_Mision)));
    m_paracaidas = m.Paracaidas != 0;
    if (m.Radio_Acercamiento > 0.0f)
        m_radio = m.Radio_Acercamiento;
    if (m.Velocidad_RTH > 0.0f)
        m_velocidadRtl = m.Velocidad_RTH;
    m_alturaCasa = m.Home[2];
    if (m.Home[0] != 0.0f || m.Home[1] != 0.0f)   // norte, este (Mercator)
        m_casa = ProyeccionMercator::aGeo(QPointF(m.Home[1], m.Home[0]));

    // Ganancias: solo las que vienen marcadas para actualizar.
    if (m.Actualizar_Ganancias_Cabeceo) {
        m_ganancias.Ganancias_Cabeceo_Manual1 = m.Ganancias_Cabeceo_Manual1;
        m_ganancias.Ganancias_Cabeceo_Manual2 = m.Ganancias_Cabeceo_Manual2;
    }
    if (m.Actualizar_Ganancias_Banqueo) {
        m_ganancias.Ganancias_Banqueo_Manual1 = m.Ganancias_Banqueo_Manual1;
        m_ganancias.Ganancias_Banqueo_Manual2 = m.Ganancias_Banqueo_Manual2;
    }
    if (m.Actualizar_Ganancias_Altura) {
        m_ganancias.Ganancias_Altura_Manual1 = m.Ganancias_Altura_Manual1;
        m_ganancias.Ganancias_Altura_Manual2 = m.Ganancias_Altura_Manual2;
    }
    if (m.Actualizar_Ganancias_Velocidad) {
        m_ganancias.Ganancias_Velocidad_Manual1 = m.Ganancias_Velocidad_Manual1;
        m_ganancias.Ganancias_Velocidad_Manual2 = m.Ganancias_Velocidad_Manual2;
    }
    if (m.Actualizar_Ganancias_Curso)
        m_ganancias.Ganancias_Curso_Manual = m.Ganancias_Curso_Manual;
    if (m.Actualizar_Ganancias_Rumbo) {
        m_ganancias.Ganancias_Rumbo_Manual1 = m.Ganancias_Rumbo_Manual1;
        m_ganancias.Ganancias_Rumbo_Manual2 = m.Ganancias_Rumbo_Manual2;
    }

    static const char *regimenes[] = {"Manual", "Estabilizado", "Automático"};
    static const char *misiones[] = {"Puntos", "Circular", "Repetir misión", "RTL"};
    emit aviso(tr("Mandos: régimen %1, misión %2, RPunto %3 m, V.RTH %4 km/h, casa %5 a %6 m%7")
                   .arg(regimenes[m_regimen], misiones[m_comando]).arg(m_radio)
                   .arg(m_velocidadRtl * 3.6, 0, 'f', 0)
                   .arg(m_casa.toString(QGeoCoordinate::Degrees)).arg(m_alturaCasa)
                   .arg(m_paracaidas ? tr(", PARACAÍDAS") : QString()));
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
    m_cumplida = false;
    emit aviso(tr("Plan recibido: %1 puntos%2").arg(m_plan.size())
                   .arg(m_regimen == 2 ? QString() : tr(" (se sigue en régimen automático)")));
    emit bytesSalida(Tramas::acuse("uvw", true));
}
