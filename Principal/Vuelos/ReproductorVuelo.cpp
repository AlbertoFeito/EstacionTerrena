#include "ReproductorVuelo.h"

#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include <algorithm>
#include <cmath>

namespace {
// Interpolacion lineal.
double mezcla(double a, double b, double f)
{
    return a + (b - a) * f;
}

// Interpolacion de un rumbo en grados por el lado corto (350 -> 10 pasa por 0).
double mezclaRumbo(double a, double b, double f)
{
    double d = std::fmod(b - a + 540.0, 360.0) - 180.0;
    double r = std::fmod(a + d * f + 360.0, 360.0);
    return r;
}

// Mas de esto entre dos muestras es un corte: no se interpola.
const qint64 kMaxHuecoMs = 2000;
}

// Un fotograma cada 40 ms (25 por segundo). Los tipos de las senales se registran para que viajen entre hilos y en las pruebas.
ReproductorVuelo::ReproductorVuelo(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<MuestraVuelo>();
    qRegisterMetaType<QVector<PuntoPlan>>();
    m_reloj.setInterval(40);
    connect(&m_reloj, &QTimer::timeout, this, [this]() {
        const qint64 ms = posicionMs();
        if (ms >= duracionMs()) {
            emitir(duracionMs());
            m_reloj.stop();
            m_baseMs = duracionMs();
            emit terminado();
            return;
        }
        emitir(ms);
    });
}

// Lee las muestras (por fecha) y los planes del vuelo con su hora.
bool ReproductorVuelo::cargar(QSqlDatabase db, int dron, int vuelo)
{
    QVector<MuestraVuelo> muestras;
    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare(QStringLiteral(
        "SELECT Lat, Lon, Realimentacion_Cabeceo, Realimentacion_Banqueo, Realimentacion_Velocidad, "
        "Realimentacion_Altura, Realimentacion_Curso, Distancia_Chequeo, Curso_Deseado, "
        "Realimentacion_Regimen, Realimentacion_ComandoMision, Realimentacion_CursoIMU, CantidadSatelites, "
        "KPCabeceo, KICabeceo, KPBanqueo, KIBanqueo, KPVelocidad, KIVelocidad, KPAltura, KIAltura, "
        "KPCurso, KPRumbo, KIRumbo, Control_Estabilizadores, Control_Alerones, Control_Motor, "
        "Control_Rumbo, Convergencia_Banqueo, Convergencia_Cabeceo, Fecha, "
        "Punto_Norte, Punto_Este, Punto_Altura "
        "FROM DatosVuelos WHERE Dron_ID = ? AND Id_Vuelos = ? ORDER BY Fecha, rowid"));
    q.addBindValue(dron);
    q.addBindValue(vuelo);
    if (!q.exec()) {
        // Base de datos de antes, sin las columnas del punto a cumplir.
        q.prepare(q.lastQuery().replace(QStringLiteral(", Punto_Norte, Punto_Este, Punto_Altura"),
                                        QString()));
        q.addBindValue(dron);
        q.addBindValue(vuelo);
        if (!q.exec())
            return false;
    }
    const bool conPunto = q.record().count() > 31;
    while (q.next()) {
        MuestraVuelo m;
        int c = 0;
        auto d = [&q, &c]() { return q.value(c++).toDouble(); };
        m.lat = d(); m.lon = d(); m.cabeceo = d(); m.banqueo = d(); m.velocidadKmh = d();
        m.altura = d(); m.curso = d(); m.distanciaChequeo = d(); m.cursoDeseado = d();
        m.regimen = int(d()); m.mision = int(d()); m.cursoImu = d(); m.satelites = int(d());
        for (double &g : m.ganancias) g = d();
        for (double &a : m.actuadores) a = d();
        m.convergenciaBanqueo = d(); m.convergenciaCabeceo = d();
        m.fecha = q.value(c++).toDateTime();
        if (conPunto) {
            m.puntoNorte = d(); m.puntoEste = d(); m.puntoAltura = d();
        }
        muestras.append(m);
    }

    QVector<PlanGrabado> planes;
    QSqlQuery p(db);
    p.setForwardOnly(true);
    p.prepare(QStringLiteral("SELECT Fecha, Nombre, Lat, Lon, Altura, Velocidad, Norte, Este "
                             "FROM PlanesVuelo WHERE Dron_ID = ? AND Id_Vuelos = ? ORDER BY Fecha, Indice"));
    p.addBindValue(dron);
    p.addBindValue(vuelo);
    if (p.exec() && !muestras.isEmpty()) {   // sin la tabla (base de datos vieja), sin planes
        QDateTime fechaPlan;
        while (p.next()) {
            const QDateTime f = p.value(0).toDateTime();
            if (planes.isEmpty() || f != fechaPlan) {
                planes.append(PlanGrabado());
                planes.last().desdeMs = muestras.first().fecha.msecsTo(f);
                fechaPlan = f;
            }
            PuntoPlan pt;
            pt.nombre = p.value(1).toString();
            pt.lat = p.value(2).toDouble(); pt.lon = p.value(3).toDouble();
            pt.altura = p.value(4).toDouble(); pt.velocidad = p.value(5).toDouble();
            pt.norte = p.value(6).toDouble(); pt.este = p.value(7).toDouble();
            planes.last().puntos.append(pt);
        }
    }
    cargar(muestras, planes);
    return !m_muestras.isEmpty();
}

// Tiempos desde la primera muestra y vuelta al principio.
void ReproductorVuelo::cargar(const QVector<MuestraVuelo> &muestras, const QVector<PlanGrabado> &planes)
{
    m_reloj.stop();
    m_muestras = muestras;
    m_planes = planes;
    m_t.clear();
    m_t.reserve(m_muestras.size());
    for (const MuestraVuelo &m : qAsConst(m_muestras))
        m_t.append(qMax<qint64>(m_t.isEmpty() ? 0 : m_t.last(),
                                m_muestras.first().fecha.msecsTo(m.fecha)));
    m_baseMs = 0;
    m_planEmitido = -2;
}

// Busqueda binaria por tiempo.
int ReproductorVuelo::indiceEn(qint64 ms) const
{
    if (m_t.isEmpty() || ms < 0)
        return m_t.isEmpty() ? -1 : 0;
    const auto it = std::upper_bound(m_t.cbegin(), m_t.cend(), ms);
    return int(it - m_t.cbegin()) - 1;
}

// La muestra anterior y la siguiente, mezcladas; lo que no se puede mezclar
// (regimen, mision, punto a cumplir, ganancias...) es el de la anterior.
MuestraVuelo ReproductorVuelo::muestraEn(qint64 ms) const
{
    const int i = indiceEn(ms);
    if (i < 0)
        return MuestraVuelo();
    MuestraVuelo r = m_muestras.at(i);
    if (i + 1 >= m_muestras.size())
        return r;
    const qint64 hueco = m_t.at(i + 1) - m_t.at(i);
    if (hueco <= 0 || hueco > kMaxHuecoMs)
        return r;
    const double f = double(ms - m_t.at(i)) / double(hueco);
    const MuestraVuelo &b = m_muestras.at(i + 1);
    r.lat = mezcla(r.lat, b.lat, f);
    r.lon = mezcla(r.lon, b.lon, f);
    r.altura = mezcla(r.altura, b.altura, f);
    r.velocidadKmh = mezcla(r.velocidadKmh, b.velocidadKmh, f);
    r.cabeceo = mezcla(r.cabeceo, b.cabeceo, f);
    r.banqueo = mezcla(r.banqueo, b.banqueo, f);
    r.distanciaChequeo = mezcla(r.distanciaChequeo, b.distanciaChequeo, f);
    r.curso = mezclaRumbo(r.curso, b.curso, f);
    r.cursoDeseado = mezclaRumbo(r.cursoDeseado, b.cursoDeseado, f);
    r.cursoImu = mezclaRumbo(r.cursoImu, b.cursoImu, f);
    r.fecha = m_muestras.first().fecha.addMSecs(ms);
    return r;
}

// El ultimo plan que empezo en o antes de \a ms.
int ReproductorVuelo::planEn(qint64 ms) const
{
    int elegido = -1;
    for (int i = 0; i < m_planes.size(); ++i)
        if (m_planes.at(i).desdeMs <= ms)
            elegido = i;
    return elegido;
}

// Posicion actual: la del ultimo cambio mas lo que ha pasado por la velocidad.
qint64 ReproductorVuelo::posicionMs() const
{
    if (!m_reloj.isActive())
        return m_baseMs;
    return qMin(duracionMs(), m_baseMs + qint64(m_desde.elapsed() * m_factor));
}

// Sigue desde donde estaba (desde el principio si habia acabado).
void ReproductorVuelo::reproducir()
{
    if (m_muestras.isEmpty() || m_reloj.isActive())
        return;
    if (m_baseMs >= duracionMs())
        m_baseMs = 0;
    m_desde.start();
    m_reloj.start();
    emitir(m_baseMs);
}

// Se queda donde esta.
void ReproductorVuelo::pausar()
{
    m_baseMs = posicionMs();
    m_reloj.stop();
}

// Al principio, sin emitir nada (quien lo usa limpia el mapa).
void ReproductorVuelo::parar()
{
    m_reloj.stop();
    m_baseMs = 0;
    m_planEmitido = -2;
}

// Salta y muestra ese instante.
void ReproductorVuelo::irA(qint64 ms)
{
    m_baseMs = qBound<qint64>(0, ms, duracionMs());
    m_desde.start();
    emitir(m_baseMs);
}

// Cambia el factor sin saltar: lo que lleva se cuenta con el factor anterior.
void ReproductorVuelo::setVelocidad(double factor)
{
    if (factor <= 0.0)
        return;
    m_baseMs = posicionMs();
    m_desde.start();
    m_factor = factor;
}

// Plan (si cambio) y fotograma.
void ReproductorVuelo::emitir(qint64 ms)
{
    if (m_muestras.isEmpty())
        return;
    const int plan = planEn(ms);
    if (plan != m_planEmitido) {
        m_planEmitido = plan;
        emit planCambiado(plan >= 0 ? m_planes.at(plan).puntos : QVector<PuntoPlan>());
    }
    emit fotograma(muestraEn(ms), ms);
}
