// Revision de rutas: los limites del dron, lo que hara de verdad en cada tramo
// y giro, y el terreno por debajo. Cada problema sale con su explicacion y su
// arreglo, con numeros.

#include "RoutePlanner.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
using Gravedad = Hallazgo::Gravedad;

// Metros, segundos o m/s sin decimales que no dicen nada.
QString num(double v, int decimales = 0)
{
    return QString::number(v, 'f', decimales);
}

// Velocidad con un decimal (como en la tabla).
QString vel(double v)
{
    return num(v, 1);
}

// Duracion como 4:05 (minutos y segundos).
QString duracion(double segundos)
{
    const int s = int(std::lround(segundos));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

// Orden de los hallazgos: errores, avisos y notas; dentro, por donde estan.
bool antes(const Hallazgo &a, const Hallazgo &b)
{
    if (a.gravedad != b.gravedad)
        return int(a.gravedad) < int(b.gravedad);
    return a.desde < b.desde;
}
}

// Cuantos hallazgos hay de la gravedad \a g.
int RevisionRuta::cuantos(Hallazgo::Gravedad g) const
{
    return int(std::count_if(hallazgos.begin(), hallazgos.end(),
                             [g](const Hallazgo &h) { return h.gravedad == g; }));
}

// Los hallazgos de un punto o de los tramos que empiezan o acaban en el.
QVector<Hallazgo> RevisionRuta::delPunto(int fila) const
{
    QVector<Hallazgo> suyos;
    for (const Hallazgo &h : hallazgos)
        if (h.tocaA(fila))
            suyos.append(h);
    return suyos;
}

// Una linea por error, para mensajes cortos (enviar, la barra de estado).
QString RevisionRuta::resumenErrores() const
{
    QStringList lineas;
    for (const Hallazgo &h : hallazgos)
        if (h.gravedad == Gravedad::Error)
            lineas.append(QStringLiteral("%1: %2").arg(h.lugar, h.titulo));
    return lineas.isEmpty() ? QStringLiteral("Ruta válida") : lineas.join(QLatin1Char('\n'));
}

// Guarda las caracteristicas del dron con las que se revisa.
RoutePlanner::RoutePlanner(const DroneCharacteristics &chars)
    : droneChars(chars)
{
}

// Radio de un giro coordinado a esa velocidad, o el minimo configurado si es mayor.
double RoutePlanner::radioDeGiro(double velocidad) const
{
    const double r = velocidad * velocidad / (g * std::tan(qDegreesToRadians(kAlabeoGrados)));
    return std::max(r, droneChars.minTurnRadius);
}

// Cambio de rumbo en p2 con los acimutes geodesicos (en grados de latitud y
// longitud sin mas, como antes, se deformaba lejos del ecuador).
double RoutePlanner::anguloGiro(const E_Punto &p1, const E_Punto &p2, const E_Punto &p3) const
{
    if (p1.distanciaA(p2) < 0.1 || p2.distanciaA(p3) < 0.1)
        return 0.0;
    double giro = std::fabs(p2.pos.azimuthTo(p3.pos) - p1.pos.azimuthTo(p2.pos));
    if (giro > 180.0)
        giro = 360.0 - giro;
    return giro;
}

// Suma de los tramos.
double RoutePlanner::calcularDistanciaTotal(const QList<E_Punto> &puntos) const
{
    double total = 0.0;
    for (int i = 0; i + 1 < puntos.size(); ++i)
        total += puntos[i].distanciaA(puntos[i + 1]);
    return total;
}

// Cada tramo a la media de las velocidades de sus dos puntos.
double RoutePlanner::calcularTiempoVuelo(const QList<E_Punto> &puntos) const
{
    double total = 0.0;
    for (int i = 0; i + 1 < puntos.size(); ++i) {
        const double media = (puntos[i].velocidad + puntos[i + 1].velocidad) / 2.0;
        if (media >= 0.5)
            total += puntos[i].distanciaA(puntos[i + 1]) / media;
    }
    return total;
}

// La ruta por defecto desde p1: 10 puntos hacia el este, separados la media
// entre la distancia minima y la maxima del dron, a la velocidad por defecto.
// Cada punto queda \a sobreTerreno m por encima de lo mas alto del terreno en
// los tramos que lo tocan (mirado cada kPasoTerreno m): como los
// dos extremos de cada tramo libran su punto mas alto, la recta entre ellos
// nunca baja de esa altura sobre el terreno. Donde no hay cota se cuenta desde
// el nivel del mar, y \a conTerreno sale false.
QList<E_Punto> RoutePlanner::generarRuta(const QGeoCoordinate &p1, double sobreTerreno, const FuncionTerreno &terreno,
                                         bool *conTerreno) const
{
    const int n = 10;
    const double separacion = (droneChars.minWaypointDistance + droneChars.maxWaypointDistance) / 2.0;
    double velocidad = droneChars.maxSpeed * (droneChars.type == DroneType::FIXED_WING ? 0.65 : 0.45);
    velocidad = std::max(velocidad, droneChars.minSpeed);
    // La columna Radio no viaja al dron; se rellena como siempre.
    double radio = 10.0;
    if (droneChars.type == DroneType::FIXED_WING)
        radio = std::max((velocidad * velocidad) / (9.81 * 0.577) * 1.5, droneChars.minTurnRadius * 2.0);

    auto cota = [&terreno](const QGeoCoordinate &c) {
        return terreno ? terreno(c) : std::numeric_limits<double>::quiet_NaN();
    };
    QVector<QGeoCoordinate> pos;
    pos.append(p1);
    for (int i = 1; i < n; ++i)
        pos.append(pos.last().atDistanceAndAzimuth(separacion, 90.0));

    // Lo mas alto bajo cada punto y bajo cada tramo (NaN: sin ningun dato).
    bool todoConCota = true;
    auto mayor = [](double a, double b) { return std::isfinite(a) ? (std::isfinite(b) ? std::max(a, b) : a) : b; };
    QVector<double> bajoPunto(n), bajoTramo(n - 1, std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < n; ++i) {
        bajoPunto[i] = cota(pos[i]);
        todoConCota = todoConCota && std::isfinite(bajoPunto[i]);
    }
    const int pasos = std::max(1, int(std::ceil(separacion / kPasoTerreno)));
    for (int i = 0; i + 1 < n; ++i)
        for (int k = 0; k <= pasos; ++k)
            bajoTramo[i] = mayor(bajoTramo[i], cota(pos[i].atDistanceAndAzimuth(separacion * k / pasos, 90.0)));

    QList<E_Punto> puntos;
    for (int i = 0; i < n; ++i) {
        double techo = bajoPunto[i];
        if (i > 0)
            techo = mayor(techo, bajoTramo[i - 1]);
        if (i + 1 < n)
            techo = mayor(techo, bajoTramo[i]);
        E_Punto p;
        p.id = i;
        p.nombre = QString("P%1").arg(i + 1);
        p.modo = 0;
        p.pos = pos[i];
        p.altura = float((std::isfinite(techo) ? techo : 0.0) + sobreTerreno);
        p.velocidad = float(velocidad);
        p.radio = float(radio);
        p.distanciaAnterior = i == 0 ? 0.0 : separacion;
        p.altitudObjetivo = p.altura;
        p.tiempoTransicion = 0;
        p.tasaAscenso = 0.0;
        puntos.append(p);
    }
    if (conTerreno)
        *conTerreno = todoConCota;
    return puntos;
}

// Revisa la ruta entera: puntos, tramos (simulando la altura a la que va de
// verdad), terreno, giros y autonomia. No se para en el primer problema.
RevisionRuta RoutePlanner::revisar(const QList<E_Punto> &p, const FuncionTerreno &terreno) const
{
    RevisionRuta r;
    const int n = p.size();
    const bool alaFija = droneChars.type == DroneType::FIXED_WING;

    auto nombre = [&](int i) {
        return p[i].nombre.isEmpty() ? QStringLiteral("P%1").arg(i + 1) : p[i].nombre;
    };
    auto anadir = [&](Gravedad g, const QString &codigo, int desde, int hasta, const QString &titulo,
                      const QString &explicacion, const QString &solucion) {
        Hallazgo h;
        h.gravedad = g;
        h.codigo = codigo;
        h.desde = desde;
        h.hasta = hasta;
        h.lugar = desde < 0 ? QStringLiteral("Ruta")
                  : hasta < 0 ? nombre(desde)
                              : QStringLiteral("%1 → %2").arg(nombre(desde), nombre(hasta));
        h.titulo = titulo;
        h.explicacion = explicacion;
        h.solucion = solucion;
        r.hallazgos.append(h);
    };

    if (n < 2) {
        anadir(Gravedad::Error, QStringLiteral("pocos_puntos"), -1, -1,
               QStringLiteral("La ruta no tiene tramos"),
               QStringLiteral("Hacen falta al menos dos puntos para tener una ruta."),
               QStringLiteral("Añade puntos (clic derecho en el mapa para generar la ruta)."));
        return r;
    }

    // --- Cifras de la ruta --------------------------------------------------
    r.velocidadMin = r.velocidadMax = p[0].velocidad;
    r.alturaMin = r.alturaMax = p[0].altura;
    for (const E_Punto &pt : p) {
        r.velocidadMin = std::min<double>(r.velocidadMin, pt.velocidad);
        r.velocidadMax = std::max<double>(r.velocidadMax, pt.velocidad);
        r.alturaMin = std::min<double>(r.alturaMin, pt.altura);
        r.alturaMax = std::max<double>(r.alturaMax, pt.altura);
    }
    r.distancia = calcularDistanciaTotal(p);
    r.tiempo = calcularTiempoVuelo(p);
    r.usoDistancia = droneChars.maxRange > 0 ? r.distancia / droneChars.maxRange : 0.0;
    r.usoTiempo = droneChars.endurance > 0 ? r.tiempo / droneChars.endurance : 0.0;

    // --- El terreno bajo cada punto (NaN si no hay dato) ---------------------
    r.cotas.fill(std::numeric_limits<double>::quiet_NaN(), n);
    if (terreno)
        for (int i = 0; i < n; ++i)
            r.cotas[i] = terreno(p[i].pos);

    // --- Cada punto: altura y velocidad dentro de lo que puede el dron -------
    // Los limites de altura del dron son sobre el terreno: la altura de cada
    // punto (sobre el mar) menos la cota de debajo. Sin cota, se cuenta desde
    // el nivel del mar y se dice.
    for (int i = 0; i < n; ++i) {
        const QString pn = nombre(i);
        const double h = p[i].altura, v = p[i].velocidad;
        const double cota = r.cotas[i];
        const bool hayCota = std::isfinite(cota);
        const double sobre = hayCota ? h - cota : h;
        const QString donde = hayCota
                ? QStringLiteral("%1 va a %2 m sobre el terreno (el terreno está a %3 m y el punto a %4 m sobre "
                                 "el mar)").arg(pn, num(sobre), num(cota), num(h))
                : QStringLiteral("%1 va a %2 m (sin datos del terreno, contados desde el nivel del mar)")
                      .arg(pn, num(h));
        if (hayCota) {
            r.sobreTerrenoMin = std::isfinite(r.sobreTerrenoMin) ? std::min(r.sobreTerrenoMin, sobre) : sobre;
            r.sobreTerrenoMax = std::isfinite(r.sobreTerrenoMax) ? std::max(r.sobreTerrenoMax, sobre) : sobre;
        }
        const double base = hayCota ? cota : 0.0;
        // Medio metro de tolerancia: las alturas se guardan en float y un
        // punto puesto justo en el limite no debe salir como error.
        if (sobre < droneChars.minAltitude - 0.5)
            anadir(Gravedad::Error, QStringLiteral("altura_baja"), i, -1,
                   QStringLiteral("Por debajo de la altura mínima"),
                   QStringLiteral("%1, y este dron vuela a %2 m sobre el terreno como mínimo.")
                       .arg(donde, num(droneChars.minAltitude)),
                   QStringLiteral("Sube %1 a %2 m sobre el mar o más (%3 m sobre el terreno).")
                       .arg(pn, num(base + droneChars.minAltitude), num(droneChars.minAltitude)));
        if (sobre > droneChars.maxAltitude + 0.5)
            anadir(Gravedad::Error, QStringLiteral("altura_alta"), i, -1,
                   QStringLiteral("Por encima de la altura máxima"),
                   QStringLiteral("%1, y este dron vuela a %2 m sobre el terreno como mucho.")
                       .arg(donde, num(droneChars.maxAltitude)),
                   QStringLiteral("Baja %1 a %2 m sobre el mar o menos (%3 m sobre el terreno).")
                       .arg(pn, num(base + droneChars.maxAltitude), num(droneChars.maxAltitude)));
        if (v > droneChars.maxSpeed)
            anadir(Gravedad::Error, QStringLiteral("velocidad_alta"), i, -1,
                   QStringLiteral("Más rápido de lo que puede el dron"),
                   QStringLiteral("%1 pide %2 m/s y el dron llega a %3 m/s como mucho.")
                       .arg(pn, vel(v), vel(droneChars.maxSpeed)),
                   QStringLiteral("Baja la velocidad de %1 a %2 m/s o menos.").arg(pn, vel(droneChars.maxSpeed)));
        if (v < 0.0)
            anadir(Gravedad::Error, QStringLiteral("velocidad_negativa"), i, -1,
                   QStringLiteral("Velocidad negativa"),
                   QStringLiteral("%1 tiene %2 m/s.").arg(pn, vel(v)),
                   QStringLiteral("Pon en %1 una velocidad de 0 m/s o más.").arg(pn));
        else if (alaFija && v < droneChars.minSpeed)
            anadir(Gravedad::Error, QStringLiteral("velocidad_baja"), i, -1,
                   QStringLiteral("Por debajo de la velocidad mínima"),
                   QStringLiteral("%1 pide %2 m/s. Por debajo de %3 m/s el ala fija entra en pérdida "
                                  "(el ala deja de sostenerlo).")
                       .arg(pn, vel(v), vel(droneChars.minSpeed)),
                   QStringLiteral("Sube la velocidad de %1 a %2 m/s o más.").arg(pn, vel(droneChars.minSpeed)));
    }

    // --- Cada tramo: separacion, velocidad, subida y terreno ----------------
    double alturaReal = p[0].altura;   // a la que va el dron de verdad
    bool algunTerreno = false;
    for (int i = 0; i + 1 < n; ++i) {
        const E_Punto &a = p[i], &b = p[i + 1];
        const QString an = nombre(i), bn = nombre(i + 1);
        const double d = a.distanciaA(b);
        const double media = (a.velocidad + b.velocidad) / 2.0;
        const double t = media >= 0.5 ? d / media : 0.0;

        TramoRuta tramo;
        tramo.desde = i;
        tramo.distancia = d;
        tramo.tiempo = t;
        tramo.tasaVertical = t > 0.0 ? (b.altura - a.altura) / t : 0.0;

        if (d > droneChars.maxWaypointDistance)
            anadir(Gravedad::Error, QStringLiteral("tramo_largo"), i, i + 1,
                   QStringLiteral("Puntos demasiado separados"),
                   QStringLiteral("De %1 a %2 hay %3 m y este dron admite %4 m como mucho entre puntos.")
                       .arg(an, bn, num(d), num(droneChars.maxWaypointDistance)),
                   QStringLiteral("Acerca %1 y %2 hasta que estén a menos de %3 m.")
                       .arg(an, bn, num(droneChars.maxWaypointDistance)));
        if (d < droneChars.minWaypointDistance)
            anadir(Gravedad::Error, QStringLiteral("tramo_corto"), i, i + 1,
                   QStringLiteral("Puntos demasiado juntos"),
                   QStringLiteral("De %1 a %2 hay %3 m y este dron necesita %4 m como mínimo entre puntos.")
                       .arg(an, bn, num(d), num(droneChars.minWaypointDistance)),
                   QStringLiteral("Separa %1 y %2 al menos %3 m.").arg(an, bn, num(droneChars.minWaypointDistance)));
        if (t <= 0.0 && d > 0.0)
            anadir(Gravedad::Aviso, QStringLiteral("tramo_parado"), i, i + 1,
                   QStringLiteral("Tramo a velocidad cero"),
                   QStringLiteral("%1 y %2 tienen velocidad 0: no se sabe cuánto tarda el tramo "
                                  "y no cuenta en el tiempo de la ruta.").arg(an, bn),
                   QStringLiteral("Pon una velocidad en %1 o en %2.").arg(an, bn));

        // Velocidad: con su aceleracion maxima, que velocidad puede tener al
        // llegar (v² = v0² ± 2·a·d).
        const double v0 = std::max<double>(0.0, a.velocidad), v1 = std::max<double>(0.0, b.velocidad);
        if (v1 > v0 && droneChars.maxAcceleration > 0.0) {
            const double alcanza = std::sqrt(v0 * v0 + 2.0 * droneChars.maxAcceleration * d);
            if (v1 > alcanza + 0.05) {
                const double hace = (v1 * v1 - v0 * v0) / (2.0 * droneChars.maxAcceleration);
                anadir(Gravedad::Aviso, QStringLiteral("acelera_poco"), i, i + 1,
                       QStringLiteral("No le da tiempo a acelerar"),
                       QStringLiteral("De %1 (%2 m/s) a %3 (%4 m/s) hay %5 m. Acelerando a %6 m/s², "
                                      "al llegar a %3 solo va a %7 m/s.")
                           .arg(an, vel(v0), bn, vel(v1), num(d), vel(droneChars.maxAcceleration), vel(alcanza)),
                       QStringLiteral("Pon en %1 %2 m/s o menos, o separa los puntos hasta %3 m.")
                           .arg(bn, vel(alcanza), num(hace)));
            }
        } else if (v1 < v0 && droneChars.maxDeceleration > 0.0) {
            const double alcanza = std::sqrt(std::max(0.0, v0 * v0 - 2.0 * droneChars.maxDeceleration * d));
            if (v1 < alcanza - 0.05) {
                const double hace = (v0 * v0 - v1 * v1) / (2.0 * droneChars.maxDeceleration);
                anadir(Gravedad::Aviso, QStringLiteral("frena_poco"), i, i + 1,
                       QStringLiteral("No le da tiempo a frenar"),
                       QStringLiteral("De %1 (%2 m/s) a %3 (%4 m/s) hay %5 m. Frenando a %6 m/s², "
                                      "al llegar a %3 aún va a %7 m/s.")
                           .arg(an, vel(v0), bn, vel(v1), num(d), vel(droneChars.maxDeceleration), vel(alcanza)),
                       QStringLiteral("Pon en %1 %2 m/s o más, o separa los puntos hasta %3 m.")
                           .arg(bn, vel(alcanza), num(hace)));
            }
        }

        // Subida o bajada desde la altura que lleva de verdad: si no le da,
        // llega mas bajo (o mas alto) y el tramo siguiente empieza ahi.
        const double dh = b.altura - alturaReal;
        const bool sube = dh > 0.0;
        const double tasaMax = sube ? droneChars.maxClimbRate : droneChars.maxDescentRate;
        double llega = b.altura;
        if (t > 0.0 && tasaMax > 0.0 && std::fabs(dh) / t > tasaMax + 1e-6) {
            llega = alturaReal + (sube ? 1.0 : -1.0) * tasaMax * t;
            const double hace = std::fabs(dh) / tasaMax * media;   // metros de tramo necesarios
            const double vNecesaria = d * tasaMax / std::fabs(dh);  // velocidad media que le daria tiempo
            QString arreglo = QStringLiteral("%1 %2 a %3 m, o separa los puntos hasta %4 m")
                                  .arg(sube ? QStringLiteral("Baja") : QStringLiteral("Sube"), bn, num(llega), num(hace));
            if (!alaFija || vNecesaria >= droneChars.minSpeed)
                arreglo += QStringLiteral(", o vuela el tramo más despacio (%1 m/s de media)").arg(vel(vNecesaria));
            const QString desdeDonde = std::fabs(alturaReal - a.altura) > 1.0
                    ? QStringLiteral(" (a %1 ya llega a %2 m, no a %3 m)").arg(an, num(alturaReal), num(a.altura))
                    : QString();
            anadir(Gravedad::Aviso, sube ? QStringLiteral("sube_rapido") : QStringLiteral("baja_rapido"), i, i + 1,
                   sube ? QStringLiteral("Sube más rápido de lo que puede")
                        : QStringLiteral("Baja más rápido de lo que puede"),
                   QStringLiteral("Para pasar de %1 m a %2 m en %3 s%4 tendría que %5 a %6 m/s, y el dron "
                                  "%7 a %8 m/s como mucho: llega a %9 a unos %10 m.")
                       .arg(num(alturaReal), num(b.altura), num(t), desdeDonde,
                            sube ? QStringLiteral("subir") : QStringLiteral("bajar"),
                            vel(std::fabs(dh) / t), sube ? QStringLiteral("sube") : QStringLiteral("baja"),
                            vel(tasaMax), bn)
                       .arg(num(llega)),
                   arreglo + QLatin1Char('.'));
        }
        tramo.alturaLlegada = llega;

        // Terreno: cada kPasoTerreno m, con la altura a la que va (sube o baja
        // a su tasa desde el principio del tramo).
        if (terreno && d > 0.0) {
            const double rumbo = a.pos.azimuthTo(b.pos);
            const int pasos = std::max(1, int(std::ceil(d / kPasoTerreno)));
            double peor = std::numeric_limits<double>::infinity(), dondePeor = 0.0, cotaPeor = 0.0,
                   alturaPeor = 0.0, cotaMax = -std::numeric_limits<double>::infinity();
            for (int k = 0; k <= pasos; ++k) {
                const double s = d * k / pasos;
                const double cota = terreno(a.pos.atDistanceAndAzimuth(s, rumbo));
                if (!std::isfinite(cota))
                    continue;
                algunTerreno = true;
                double h;
                if (t > 0.0) {
                    const double cambio = std::min(std::fabs(dh), (tasaMax > 0.0 ? tasaMax : 1e9) * s / media);
                    h = alturaReal + (sube ? cambio : -cambio);
                } else {
                    h = alturaReal + dh * s / d;
                }
                cotaMax = std::max(cotaMax, cota);
                if (h - cota < peor) {
                    peor = h - cota;
                    dondePeor = s;
                    cotaPeor = cota;
                    alturaPeor = h;
                }
            }
            if (std::isfinite(peor)) {
                tramo.holguraTerreno = peor;
                if (!std::isfinite(r.holguraTerreno) || peor < r.holguraTerreno)
                    r.holguraTerreno = peor;
                const QString sitio = QStringLiteral("A %1 m de %2, camino de %3, el terreno está a %4 m y el dron "
                                                     "pasa a %5 m")
                                          .arg(num(dondePeor), an, bn, num(cotaPeor), num(alturaPeor));
                const QString arreglo = QStringLiteral("Sube %1 y %2 hasta %3 m sobre el mar por lo menos (lo más alto "
                                                       "del terreno en el tramo, %4 m, más %5 m), o aparta el tramo de "
                                                       "esa zona.")
                                            .arg(an, bn, num(cotaMax + kHolguraAviso), num(cotaMax), num(kHolguraAviso));
                if (peor < 0.0)
                    anadir(Gravedad::Error, QStringLiteral("terreno_choca"), i, i + 1,
                           QStringLiteral("Atraviesa el terreno"),
                           sitio + QStringLiteral(": %1 m por debajo. Chocaría.").arg(num(-peor)), arreglo);
                else if (peor < kHolguraMinima)
                    anadir(Gravedad::Error, QStringLiteral("terreno_roza"), i, i + 1,
                           QStringLiteral("Pasa rozando el terreno"),
                           sitio + QStringLiteral(": solo %1 m por encima (hacen falta %2 m como mínimo).")
                                       .arg(num(peor), num(kHolguraMinima)),
                           arreglo);
                else if (peor < kHolguraAviso)
                    anadir(Gravedad::Aviso, QStringLiteral("terreno_cerca"), i, i + 1,
                           QStringLiteral("Poco margen sobre el terreno"),
                           sitio + QStringLiteral(": %1 m por encima. Con viento o un error del GPS o de "
                                                  "la elevación, es poco.")
                                       .arg(num(peor)),
                           arreglo);
            }
        }

        r.tramos.append(tramo);
        if (b.altura > a.altura)
            r.subida += b.altura - a.altura;
        else
            r.bajada += a.altura - b.altura;
        alturaReal = llega;
    }
    for (double c : qAsConst(r.cotas))
        algunTerreno = algunTerreno || std::isfinite(c);
    r.hayTerreno = algunTerreno;
    if (terreno && !algunTerreno)
        anadir(Gravedad::Nota, QStringLiteral("sin_terreno"), -1, -1,
               QStringLiteral("Sin datos de terreno"),
               QStringLiteral("No hay elevación para esta zona: no se ha comprobado que la ruta pase por "
                              "encima del terreno."),
               QStringLiteral("Revisa las alturas a ojo, o añade la elevación al paquete del mapa."));

    // --- Giros del ala fija: el siguiente punto, fuera del circulo de giro ---
    // Al llegar a un punto gira con un radio r (por la velocidad). Si el
    // siguiente queda dentro de su circulo de giro (d < 2·r·sen θ, con θ lo que
    // gira), no puede ir directo: se pasa y da una vuelta para alcanzarlo.
    if (alaFija) {
        const double tanAlabeo = std::tan(qDegreesToRadians(kAlabeoGrados));
        for (int i = 1; i + 1 < n; ++i) {
            const double giro = anguloGiro(p[i - 1], p[i], p[i + 1]);
            if (giro < 5.0)
                continue;
            const double v = std::max<double>(p[i].velocidad, 0.0);
            const double radio = radioDeGiro(v);
            const double d = p[i].distanciaA(p[i + 1]);
            const double sen = std::sin(qDegreesToRadians(giro));
            const double necesita = 2.0 * radio * sen;
            if (d >= necesita)
                continue;
            const QString pn = nombre(i), sig = nombre(i + 1);
            QStringList arreglos;
            const double radioCabe = d / (2.0 * sen);
            const double vCabe = std::sqrt(radioCabe * g * tanAlabeo);
            if (radioCabe >= droneChars.minTurnRadius && vCabe >= droneChars.minSpeed)
                arreglos << QStringLiteral("baja la velocidad en %1 a %2 m/s o menos").arg(pn, vel(vCabe));
            arreglos << QStringLiteral("aleja %1 hasta %2 m de %3").arg(sig, num(necesita), pn);
            if (giro <= 90.0 && d < 2.0 * radio)
                arreglos << QStringLiteral("abre el giro a menos de %1°")
                                .arg(num(qRadiansToDegrees(std::asin(d / (2.0 * radio)))));
            QString solucion = arreglos.join(QStringLiteral(", o "));
            solucion[0] = solucion[0].toUpper();
            anadir(Gravedad::Aviso, QStringLiteral("giro_cerrado"), i, i + 1,
                   QStringLiteral("Giro demasiado cerrado para su velocidad"),
                   QStringLiteral("En %1 gira %2°. A %3 m/s el ala fija gira con un radio de unos %4 m "
                                  "(alabeo de %5°) y %6 está a %7 m: queda dentro del círculo de giro, así "
                                  "que se pasará y dará una vuelta para alcanzarlo.")
                       .arg(pn, num(giro), vel(v), num(radio), num(kAlabeoGrados), sig, num(d)),
                   solucion + QLatin1Char('.'));
        }
    }

    // --- La ruta entera: autonomia en distancia y en tiempo ------------------
    if (droneChars.maxRange > 0) {
        if (r.distancia > droneChars.maxRange)
            anadir(Gravedad::Error, QStringLiteral("autonomia_distancia"), -1, -1,
                   QStringLiteral("Más larga que la autonomía"),
                   QStringLiteral("La ruta mide %1 m y el dron tiene %2 m de autonomía.")
                       .arg(num(r.distancia), num(droneChars.maxRange)),
                   QStringLiteral("Acorta la ruta al menos %1 m acercando los puntos.")
                       .arg(num(r.distancia - droneChars.maxRange)));
        else if (r.usoDistancia > kUsoAutonomiaAviso)
            anadir(Gravedad::Aviso, QStringLiteral("autonomia_justa"), -1, -1,
                   QStringLiteral("Poco margen de autonomía"),
                   QStringLiteral("La ruta usa el %1 % de la autonomía (%2 de %3 m), y además hay que ir "
                                  "hasta %4 y volver.")
                       .arg(num(r.usoDistancia * 100.0), num(r.distancia), num(droneChars.maxRange), nombre(0)),
                   QStringLiteral("Acorta la ruta o comprueba que le llega para ir y volver."));
    }
    if (droneChars.endurance > 0) {
        if (r.tiempo > droneChars.endurance)
            anadir(Gravedad::Error, QStringLiteral("autonomia_tiempo"), -1, -1,
                   QStringLiteral("Dura más que la batería"),
                   QStringLiteral("La ruta dura unos %1 (min:s) y el dron vuela %2 como mucho.")
                       .arg(duracion(r.tiempo), duracion(droneChars.endurance)),
                   QStringLiteral("Acorta la ruta o súbele la velocidad."));
        else if (r.usoTiempo > kUsoAutonomiaAviso)
            anadir(Gravedad::Aviso, QStringLiteral("tiempo_justo"), -1, -1,
                   QStringLiteral("Poco margen de batería"),
                   QStringLiteral("La ruta dura unos %1 (min:s), el %2 % de lo que vuela el dron (%3), y además "
                                  "hay que ir hasta %4 y volver.")
                       .arg(duracion(r.tiempo), num(r.usoTiempo * 100.0), duracion(droneChars.endurance), nombre(0)),
                   QStringLiteral("Acorta la ruta o comprueba que le llega para ir y volver."));
    }

    std::stable_sort(r.hallazgos.begin(), r.hallazgos.end(), antes);
    return r;
}
