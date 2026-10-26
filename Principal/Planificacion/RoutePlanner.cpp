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

// Velocidad con la que se generan las rutas: el 65 % de la maxima en el ala
// fija y el 45 % en el cuadroptero, nunca por debajo de la minima.
double RoutePlanner::velocidadPorDefecto() const
{
    const double v = droneChars.maxSpeed * (droneChars.type == DroneType::FIXED_WING ? 0.65 : 0.45);
    return std::max(v, droneChars.minSpeed);
}

// Los 10 puntos por esas posiciones, con altura y velocidad que el dron pueda
// seguir. Cada punto queda \a sobreTerreno m por encima de lo mas alto del
// terreno en los tramos que lo tocan (mirado cada kPasoTerreno m): como los
// dos extremos de cada tramo libran su punto mas alto, la recta entre ellos
// nunca baja de esa altura sobre el terreno. Donde no hay cota se cuenta desde
// el nivel del mar, y \a conTerreno sale false.
QList<E_Punto> RoutePlanner::puntosPorPosiciones(const QVector<QGeoCoordinate> &pos, double sobreTerreno,
                                                 const FuncionTerreno &terreno, bool *conTerreno) const
{
    const int n = pos.size();
    const double velocidad = velocidadPorDefecto();
    // La columna Radio no viaja al dron; se rellena como siempre.
    double radio = 10.0;
    if (droneChars.type == DroneType::FIXED_WING)
        radio = std::max((velocidad * velocidad) / (9.81 * 0.577) * 1.5, droneChars.minTurnRadius * 2.0);

    auto cota = [&terreno](const QGeoCoordinate &c) {
        return terreno ? terreno(c) : std::numeric_limits<double>::quiet_NaN();
    };
    QVector<double> largo(std::max(0, n - 1));
    for (int i = 0; i + 1 < n; ++i)
        largo[i] = pos[i].distanceTo(pos[i + 1]);

    // Lo mas alto bajo cada punto y bajo cada tramo (NaN: sin ningun dato).
    bool todoConCota = true;
    auto mayor = [](double a, double b) { return std::isfinite(a) ? (std::isfinite(b) ? std::max(a, b) : a) : b; };
    QVector<double> bajoPunto(n), bajoTramo(std::max(0, n - 1), std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < n; ++i) {
        bajoPunto[i] = cota(pos[i]);
        todoConCota = todoConCota && std::isfinite(bajoPunto[i]);
    }
    for (int i = 0; i + 1 < n; ++i) {
        const double rumbo = pos[i].azimuthTo(pos[i + 1]);
        const int pasos = std::max(1, int(std::ceil(largo[i] / kPasoTerreno)));
        for (int k = 0; k <= pasos; ++k)
            bajoTramo[i] = mayor(bajoTramo[i], cota(pos[i].atDistanceAndAzimuth(largo[i] * k / pasos, rumbo)));
    }

    QVector<double> alt(n), vel(n, velocidad);
    for (int i = 0; i < n; ++i) {
        double techo = bajoPunto[i];
        if (i > 0)
            techo = mayor(techo, bajoTramo[i - 1]);
        if (i + 1 < n)
            techo = mayor(techo, bajoTramo[i]);
        alt[i] = (std::isfinite(techo) ? techo : 0.0) + sobreTerreno;
    }

    // Que el dron pueda seguirla. En un tramo que sube mas de lo que le da su
    // tasa a esa velocidad (sube al tasa/v), se baja la velocidad de sus dos
    // puntos, hasta la minima del ala fija. Si ni asi le da, se suben los
    // puntos de antes (de atras adelante) para que empiece la subida mas alto.
    // Solo se sube: el terreno queda igual de librado. Las bajadas que no le
    // dan se dejan: llega mas alto, que es seguro.
    if (droneChars.maxClimbRate > 0.0) {
        const double vMinima = droneChars.type == DroneType::FIXED_WING ? droneChars.minSpeed : 1.0;
        for (int i = 0; i + 1 < n; ++i) {
            const double dh = alt[i + 1] - alt[i];
            if (dh <= 0.0)
                continue;
            const double cabe = std::max(vMinima, droneChars.maxClimbRate * largo[i] / dh);
            vel[i] = std::min(vel[i], cabe);
            vel[i + 1] = std::min(vel[i + 1], cabe);
        }
        for (int i = n - 2; i >= 0; --i) {
            const double media = (vel[i] + vel[i + 1]) / 2.0;
            const double puedeSubir = droneChars.maxClimbRate * largo[i] / media;
            if (alt[i + 1] - alt[i] > puedeSubir)
                alt[i] = alt[i + 1] - puedeSubir;
        }
    }

    QList<E_Punto> puntos;
    for (int i = 0; i < n; ++i) {
        E_Punto p;
        p.id = i;
        p.nombre = QString("P%1").arg(i + 1);
        p.modo = 0;
        p.pos = pos[i];
        p.altura = float(alt[i]);
        p.velocidad = float(vel[i]);
        p.radio = float(radio);
        p.distanciaAnterior = i == 0 ? 0.0 : largo[i - 1];
        p.altitudObjetivo = p.altura;
        p.tiempoTransicion = 0;
        p.tasaAscenso = 0.0;
        puntos.append(p);
    }
    if (conTerreno)
        *conTerreno = todoConCota;
    return puntos;
}

// La ruta por defecto desde p1: 10 puntos hacia el este, separados la media
// entre la distancia minima y la maxima del dron (ver puntosPorPosiciones).
QList<E_Punto> RoutePlanner::generarRuta(const QGeoCoordinate &p1, double sobreTerreno, const FuncionTerreno &terreno,
                                         bool *conTerreno) const
{
    const double separacion = (droneChars.minWaypointDistance + droneChars.maxWaypointDistance) / 2.0;
    QVector<QGeoCoordinate> pos{p1};
    for (int i = 1; i < 10; ++i)
        pos.append(pos.last().atDistanceAndAzimuth(separacion, 90.0));
    return puntosPorPosiciones(pos, sobreTerreno, terreno, conTerreno);
}

// Nombre de cada prioridad, como lo ve el operador.
QString RoutePlanner::nombrePrioridad(Prioridad p)
{
    switch (p) {
    case Prioridad::Segura: return QStringLiteral("Segura");
    case Prioridad::Equilibrada: return QStringLiteral("Equilibrada");
    case Prioridad::Rapida: return QStringLiteral("Rápida");
    default: return QStringLiteral("Recta");
    }
}

// Rutas de P1 a P10 por un pasillo de ±anchoPasillo m alrededor de la recta,
// una por prioridad y la recta para comparar.
//
// La recta se parte en 9 tramos iguales y en cada una de las 8 estaciones
// intermedias se prueban desplazamientos a un lado y a otro (21, cada
// anchoPasillo/10 m). El coste de una ruta se reparte por tramos y por cada
// giro (que depende de tres puntos seguidos), asi que la mejor combinacion se
// encuentra con programacion dinamica (Viterbi) sobre parejas de
// desplazamientos, sin probar los millones de combinaciones. Lo que cuenta:
// - el largo (tiempo de vuelo);
// - la subida: lo que sube lo mas alto del terreno de un tramo al siguiente
//   (es lo que tendra que subir el dron) y, mucho mas caro, lo que pase de lo
//   que le da su tasa a su velocidad (ahi se quedaria por debajo);
// - lo accidentado del terreno bajo el tramo (con pendientes fuertes, un error
//   de posicion o de la elevacion pesa mas);
// - los giros, y mucho mas si el punto siguiente queda dentro del circulo de
//   giro del ala fija;
// - pasarse de la altura maxima del dron sobre el terreno en un punto.
// Los tramos fuera de la distancia minima o maxima no se admiten. Despues, la
// altura y la velocidad de cada punto salen de puntosPorPosiciones y cada
// propuesta se revisa entera (revisar) para compararlas.
QVector<PropuestaRuta> RoutePlanner::proponerRutas(const QGeoCoordinate &p1, const QGeoCoordinate &p10,
                                                   double sobreTerreno, const FuncionTerreno &terreno,
                                                   double anchoPasillo) const
{
    QVector<PropuestaRuta> propuestas;
    const int n = 10, tramos = n - 1, opciones = 21, centro = opciones / 2;
    const double largoRecta = p1.distanceTo(p10);
    const double rumbo = p1.azimuthTo(p10);

    auto fallo = [&](const QString &motivo) {
        PropuestaRuta pr;
        pr.prioridad = Prioridad::Recta;
        pr.nombre = nombrePrioridad(Prioridad::Recta);
        pr.motivo = motivo;
        propuestas = {pr};
        return propuestas;
    };
    if (largoRecta > tramos * droneChars.maxWaypointDistance)
        return fallo(QStringLiteral("P1 y P10 están a %1 m. Una ruta son 10 puntos, 9 tramos, y este dron admite "
                                    "%2 m como mucho entre puntos (su «Dist. máx entre waypoints»): caben %3 m. "
                                    "Acerca P10 a P1, o sube esa distancia en los datos del dron si el dron real "
                                    "lo admite.")
                         .arg(largoRecta, 0, 'f', 0).arg(droneChars.maxWaypointDistance, 0, 'f', 0)
                         .arg(tramos * droneChars.maxWaypointDistance, 0, 'f', 0));
    if (largoRecta < 1.0)
        return fallo(QStringLiteral("P1 y P10 están en el mismo sitio."));

    // Posiciones candidatas: estacion k (0..9), desplazamiento m (0..20).
    const double paso = anchoPasillo / centro;
    auto cota = [&terreno](const QGeoCoordinate &c) {
        return terreno ? terreno(c) : std::numeric_limits<double>::quiet_NaN();
    };
    QVector<QVector<QGeoCoordinate>> pos(n, QVector<QGeoCoordinate>(opciones));
    QVector<QVector<double>> suelo(n, QVector<double>(opciones, 0.0));
    bool hayCota = false;
    for (int k = 0; k < n; ++k) {
        const QGeoCoordinate eje = k == 0 ? p1 : (k == n - 1 ? p10 : p1.atDistanceAndAzimuth(largoRecta * k / tramos, rumbo));
        for (int m = 0; m < opciones; ++m) {
            const double lado = (m - centro) * paso;
            pos[k][m] = (k == 0 || k == n - 1 || m == centro)
                    ? eje : eje.atDistanceAndAzimuth(std::fabs(lado), rumbo + (lado > 0 ? 90.0 : -90.0));
            const double c = cota(pos[k][m]);
            hayCota = hayCota || std::isfinite(c);
            suelo[k][m] = std::isfinite(c) ? c : 0.0;
        }
    }
    auto libre = [&](int k, int m) { return (k == 0 || k == n - 1) ? m == centro : true; };

    // Cada tramo posible: largo, lo mas alto y lo mas bajo del terreno debajo.
    struct Tramo { double largo = 0, techo = 0, fondo = 0; bool vale = false; };
    QVector<QVector<QVector<Tramo>>> tramo(tramos, QVector<QVector<Tramo>>(opciones, QVector<Tramo>(opciones)));
    for (int k = 0; k < tramos; ++k)
        for (int m = 0; m < opciones; ++m) {
            if (!libre(k, m))
                continue;
            for (int q = 0; q < opciones; ++q) {
                if (!libre(k + 1, q))
                    continue;
                Tramo &t = tramo[k][m][q];
                t.largo = pos[k][m].distanceTo(pos[k + 1][q]);
                t.vale = t.largo >= droneChars.minWaypointDistance && t.largo <= droneChars.maxWaypointDistance;
                if (!t.vale)
                    continue;
                const double az = pos[k][m].azimuthTo(pos[k + 1][q]);
                const int pasos = std::max(1, int(std::ceil(t.largo / kPasoTerreno)));
                t.techo = -1e9;
                t.fondo = 1e9;
                for (int j = 0; j <= pasos; ++j) {
                    const double c = cota(pos[k][m].atDistanceAndAzimuth(t.largo * j / pasos, az));
                    const double h = std::isfinite(c) ? c : 0.0;
                    t.techo = std::max(t.techo, h);
                    t.fondo = std::min(t.fondo, h);
                }
            }
        }

    const bool alaFija = droneChars.type == DroneType::FIXED_WING;
    const double v = velocidadPorDefecto();
    const double radio = alaFija ? radioDeGiro(v) : std::max(droneChars.minTurnRadius, 1.0);
    const double subeEnTramo = droneChars.maxClimbRate / std::max(v, 0.5);   // m de subida por m de tramo
    const double infinito = std::numeric_limits<double>::infinity();

    // Pesos de cada prioridad (en "metros equivalentes").
    struct Pesos { double largo, subida, exceso, accidentado, giroGrado; };
    auto pesos = [](Prioridad p) {
        switch (p) {
        case Prioridad::Segura: return Pesos{0.5, 10.0, 200.0, 1.0, 2.0};
        case Prioridad::Rapida: return Pesos{1.0, 2.0, 20.0, 0.0, 0.5};
        default: return Pesos{1.0, 5.0, 60.0, 0.3, 1.0};
        }
    };

    auto conPosiciones = [&](Prioridad pr, const QVector<QGeoCoordinate> &camino) {
        PropuestaRuta prop;
        prop.prioridad = pr;
        prop.nombre = nombrePrioridad(pr);
        prop.puntos = puntosPorPosiciones(camino, sobreTerreno, terreno);
        prop.revision = revisar(prop.puntos, terreno);
        prop.ok = true;
        return prop;
    };

    for (Prioridad pr : {Prioridad::Segura, Prioridad::Equilibrada, Prioridad::Rapida}) {
        const Pesos w = pesos(pr);
        // coste[k][a][b]: lo mejor para llegar a la estacion k desde a (en k-1) y estando en b.
        QVector<QVector<QVector<double>>> coste(n, QVector<QVector<double>>(opciones, QVector<double>(opciones, infinito)));
        QVector<QVector<QVector<int>>> vieneDe(n, QVector<QVector<int>>(opciones, QVector<int>(opciones, -1)));
        for (int b = 0; b < opciones; ++b) {
            const Tramo &t = tramo[0][centro][b];
            if (t.vale)
                coste[1][centro][b] = w.largo * t.largo + w.accidentado * (t.techo - t.fondo);
        }
        for (int k = 1; k < tramos; ++k)
            for (int a = 0; a < opciones; ++a)
                for (int b = 0; b < opciones; ++b) {
                    const double base = coste[k][a][b];
                    if (!std::isfinite(base))
                        continue;
                    const Tramo &antes = tramo[k - 1][a][b];
                    for (int c = 0; c < opciones; ++c) {
                        const Tramo &t = tramo[k][b][c];
                        if (!t.vale)
                            continue;
                        double extra = w.largo * t.largo + w.accidentado * (t.techo - t.fondo);
                        const double sube = std::max(0.0, t.techo - antes.techo);
                        extra += w.subida * sube + w.exceso * std::max(0.0, sube - subeEnTramo * t.largo);
                        // Altura del punto b sobre su terreno: por encima de la maxima del dron, muy caro.
                        const double sobre = std::max(antes.techo, t.techo) + sobreTerreno - suelo[k][b];
                        extra += 50.0 * std::max(0.0, sobre - droneChars.maxAltitude);
                        // Giro en b.
                        double giro = std::fabs(pos[k][b].azimuthTo(pos[k + 1][c]) - pos[k - 1][a].azimuthTo(pos[k][b]));
                        if (giro > 180.0)
                            giro = 360.0 - giro;
                        extra += w.giroGrado * giro;
                        if (alaFija) {
                            const double necesita = 2.0 * radio * std::sin(qDegreesToRadians(giro));
                            extra += 50.0 * std::max(0.0, necesita - t.largo);
                        }
                        if (base + extra < coste[k + 1][b][c]) {
                            coste[k + 1][b][c] = base + extra;
                            vieneDe[k + 1][b][c] = a;
                        }
                    }
                }
        // Final: la estacion 9 es P10 (centro).
        int mejorA = -1;
        double mejor = infinito;
        for (int a = 0; a < opciones; ++a)
            if (coste[n - 1][a][centro] < mejor) {
                mejor = coste[n - 1][a][centro];
                mejorA = a;
            }
        if (mejorA < 0) {
            PropuestaRuta prop;
            prop.prioridad = pr;
            prop.nombre = nombrePrioridad(pr);
            prop.motivo = QStringLiteral("No hay combinación de puntos que respete la distancia mínima y máxima "
                                         "entre puntos en este pasillo.");
            propuestas.append(prop);
            continue;
        }
        // Hacia atras: los desplazamientos elegidos.
        QVector<int> elegido(n, centro);
        int b = centro, a = mejorA;
        for (int k = n - 1; k >= 1; --k) {
            elegido[k] = b;
            elegido[k - 1] = a;
            const int antes = vieneDe[k][a][b];
            b = a;
            a = antes < 0 ? centro : antes;
        }
        QVector<QGeoCoordinate> camino;
        for (int k = 0; k < n; ++k)
            camino.append(pos[k][elegido[k]]);
        propuestas.append(conPosiciones(pr, camino));
    }

    // La recta, para comparar.
    QVector<QGeoCoordinate> recta;
    for (int k = 0; k < n; ++k)
        recta.append(pos[k][centro]);
    PropuestaRuta laRecta = conPosiciones(Prioridad::Recta, recta);
    if (!tramo[0][centro][centro].vale)
        laRecta.motivo = QStringLiteral("En recta, los tramos (%1 m) no caben entre la distancia mínima y la "
                                        "máxima del dron.").arg(largoRecta / tramos, 0, 'f', 0);
    propuestas.append(laRecta);
    if (!hayCota)
        for (PropuestaRuta &pr : propuestas)
            pr.motivo = pr.motivo.isEmpty() ? QStringLiteral("Sin datos de elevación: las rutas no han podido "
                                                             "tener en cuenta el terreno.") : pr.motivo;
    return propuestas;
}

// Simula el vuelo en planta: velocidad del tramo, giro con su radio
// (radioDeGiro), rumbo siempre hacia el punto al que va y cambio al siguiente
// al entrar en el radio de acercamiento, que es lo que hace el autopiloto con
// los mandos. En metros locales (este, norte) desde P1; un paso cada 0,5 s.
QVector<QGeoCoordinate> RoutePlanner::trayectoriaProbable(const QList<E_Punto> &p, double radioAcercamiento) const
{
    QVector<QGeoCoordinate> camino;
    const int n = p.size();
    if (n < 2)
        return camino;
    const QGeoCoordinate origen = p[0].pos;
    auto local = [&origen](const QGeoCoordinate &c) {
        const double d = origen.distanceTo(c), az = qDegreesToRadians(origen.azimuthTo(c));
        return QPointF(d * std::sin(az), d * std::cos(az));
    };
    auto geo = [&origen](const QPointF &q) {
        const double d = std::hypot(q.x(), q.y());
        return d < 1e-6 ? origen : origen.atDistanceAndAzimuth(d, qRadiansToDegrees(std::atan2(q.x(), q.y())));
    };
    QVector<QPointF> wp;
    for (const E_Punto &pt : p)
        wp.append(local(pt.pos));

    const double dt = 0.5;
    const double llegada = std::max(radioAcercamiento, 1.0);
    QPointF pos = wp[0];
    double rumbo = std::atan2(wp[1].x() - wp[0].x(), wp[1].y() - wp[0].y());   // desde el norte, horario
    camino.append(p[0].pos);
    for (int objetivo = 1; objetivo < n; ++objetivo) {
        const double v = std::max(0.5, (double(p[objetivo - 1].velocidad) + p[objetivo].velocidad) / 2.0);
        // El ala fija gira con el radio de su velocidad; el cuadroptero, con su
        // radio minimo (casi en el sitio).
        const double radio = droneChars.type == DroneType::FIXED_WING ? radioDeGiro(v)
                                                                      : std::max(droneChars.minTurnRadius, 1.0);
        const double giroMax = v / radio * dt;   // rad por paso
        const QPointF ini = pos;
        const double distancia = std::hypot(wp[objetivo].x() - ini.x(), wp[objetivo].y() - ini.y());
        // Si no lo alcanza (da vueltas dentro de su circulo de giro), se pasa
        // al siguiente tras unas vueltas: la revision ya avisa de ese giro.
        const double limite = 3.0 * distancia / v + 4.0 * M_PI * radio / v + 10.0;
        for (double tiempo = 0.0; tiempo < limite; tiempo += dt) {
            const QPointF falta = wp[objetivo] - pos;
            if (std::hypot(falta.x(), falta.y()) <= llegada)
                break;
            double error = std::atan2(falta.x(), falta.y()) - rumbo;
            while (error > M_PI) error -= 2.0 * M_PI;
            while (error < -M_PI) error += 2.0 * M_PI;
            rumbo += std::clamp(error, -giroMax, giroMax);
            pos += QPointF(std::sin(rumbo), std::cos(rumbo)) * (v * dt);
            camino.append(geo(pos));
        }
    }
    return camino;
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
    int ultimoEnPlan = 0;              // ultimo punto al que llego a la altura del plan
    QVector<double> recorrido(n, 0.0); // distancia por la ruta desde P1 hasta cada punto
    for (int i = 1; i < n; ++i)
        recorrido[i] = recorrido[i - 1] + p[i - 1].distanciaA(p[i]);
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
                   QStringLiteral("De %1 a %2 hay %3 m y este dron admite %4 m como mucho entre puntos (su "
                                  "«Dist. máx entre waypoints»).")
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

        // La altura a la que va de verdad a lo largo del tramo: sube o baja a
        // su tasa desde el principio hasta alcanzar la del punto siguiente.
        auto alturaEn = [&](double s) {
            if (t <= 0.0)
                return alturaReal + dh * s / std::max(d, 1e-9);
            const double cambio = std::min(std::fabs(dh), (tasaMax > 0.0 ? tasaMax : 1e9) * s / media);
            return alturaReal + (sube ? cambio : -cambio);
        };
        {
            const int pasos = std::max(1, int(std::ceil(d / kPasoTerreno)));
            for (int k = (i == 0 ? 0 : 1); k <= pasos; ++k) {
                const double s = d * k / pasos;
                r.perfilReal.append(QPointF(recorrido[i] + s, alturaEn(s)));
            }
        }

        // Terreno: cada kPasoTerreno m, con la altura a la que va (sube o baja
        // a su tasa desde el principio del tramo).
        if (terreno && d > 0.0) {
            const double rumbo = a.pos.azimuthTo(b.pos);
            const int pasos = std::max(1, int(std::ceil(d / kPasoTerreno)));
            double peor = std::numeric_limits<double>::infinity(), dondePeor = 0.0, cotaPeor = 0.0,
                   alturaPeor = 0.0, planPeor = 0.0, cotaMax = -std::numeric_limits<double>::infinity();
            for (int k = 0; k <= pasos; ++k) {
                const double s = d * k / pasos;
                const double cota = terreno(a.pos.atDistanceAndAzimuth(s, rumbo));
                if (!std::isfinite(cota))
                    continue;
                algunTerreno = true;
                const double h = alturaEn(s);
                cotaMax = std::max(cotaMax, cota);
                if (h - cota < peor) {
                    peor = h - cota;
                    dondePeor = s;
                    cotaPeor = cota;
                    alturaPeor = h;
                    planPeor = a.altura + (b.altura - a.altura) * s / d;   // la recta del plan
                }
            }
            if (std::isfinite(peor)) {
                tramo.holguraTerreno = peor;
                if (!std::isfinite(r.holguraTerreno) || peor < r.holguraTerreno)
                    r.holguraTerreno = peor;
                const QString sitio = QStringLiteral("A %1 m de %2, camino de %3, el terreno está a %4 m y el dron "
                                                     "pasa a %5 m")
                                          .arg(num(dondePeor), an, bn, num(cotaPeor), num(alturaPeor));
                // Dos causas con arreglos distintos: el plan va bajo (subir los
                // puntos) o el plan libra el terreno pero el dron no llega a su
                // altura porque no le da para subir (subir los puntos no sirve).
                const bool noLlega = planPeor - cotaPeor >= kHolguraAviso && alturaPeor < planPeor - 1.0;
                QString porque, arreglo;
                if (noLlega) {
                    const double falta = cotaPeor + kHolguraAviso - alturaPeor;   // m que le faltan
                    const QString desde = nombre(ultimoEnPlan);
                    // Subir desde el ultimo punto en que iba a su altura hasta
                    // librar este sitio: a que velocidad (media) le da.
                    const double necesita = cotaPeor + kHolguraAviso - p[ultimoEnPlan].altura;
                    const double recorre = recorrido[i] - recorrido[ultimoEnPlan] + dondePeor;
                    porque = QStringLiteral(" El plan pasa ahí a %1 m sobre el terreno, pero el dron no llega a esa "
                                            "altura: desde %2 no le da para subir lo que pide la ruta (va %3 m por "
                                            "debajo del plan). Subir los puntos no lo arregla.")
                                 .arg(num(planPeor - cotaPeor), desde, num(planPeor - alturaPeor));
                    QStringList opciones;
                    if (necesita > 0.0 && tasaMax > 0.0 && recorre > 0.0) {
                        const double vCabe = tasaMax * recorre / necesita;
                        if (!alaFija || vCabe >= droneChars.minSpeed)
                            opciones << QStringLiteral("vuela más despacio de %1 a %2: %3 m/s o menos (así sube al "
                                                       "%4 %)")
                                            .arg(desde, bn, vel(vCabe), num(tasaMax / vCabe * 100.0));
                    }
                    opciones << QStringLiteral("empieza la subida más alto: %1 a %2 m (%3 m más)")
                                    .arg(desde, num(p[ultimoEnPlan].altura + falta), num(falta));
                    opciones << QStringLiteral("haz la subida más larga (puntos más separados, o en zigzag u otro "
                                               "rumbo en vez de ir de frente contra la ladera)");
                    arreglo = opciones.join(QStringLiteral(", o "));
                    arreglo[0] = arreglo[0].toUpper();
                    arreglo += QLatin1Char('.');
                } else {
                    arreglo = QStringLiteral("Sube %1 y %2 hasta %3 m sobre el mar por lo menos (lo más alto del "
                                             "terreno en el tramo, %4 m, más %5 m), o aparta el tramo de esa zona.")
                                  .arg(an, bn, num(cotaMax + kHolguraAviso), num(cotaMax), num(kHolguraAviso));
                }
                if (peor < 0.0)
                    anadir(Gravedad::Error, QStringLiteral("terreno_choca"), i, i + 1,
                           QStringLiteral("Atraviesa el terreno"),
                           sitio + QStringLiteral(": %1 m por debajo. Chocaría.").arg(num(-peor)) + porque, arreglo);
                else if (peor < kHolguraMinima)
                    anadir(Gravedad::Error, QStringLiteral("terreno_roza"), i, i + 1,
                           QStringLiteral("Pasa rozando el terreno"),
                           sitio + QStringLiteral(": solo %1 m por encima (hacen falta %2 m como mínimo).")
                                       .arg(num(peor), num(kHolguraMinima)) + porque,
                           arreglo);
                else if (peor < kHolguraAviso)
                    anadir(Gravedad::Aviso, QStringLiteral("terreno_cerca"), i, i + 1,
                           QStringLiteral("Poco margen sobre el terreno"),
                           sitio + QStringLiteral(": %1 m por encima. Con viento o un error del GPS o de "
                                                  "la elevación, es poco.")
                                       .arg(num(peor)) + porque,
                           arreglo);
            }
        }

        r.tramos.append(tramo);
        if (std::fabs(llega - b.altura) < 0.5)
            ultimoEnPlan = i + 1;
        if (b.altura > a.altura)
            r.subida += b.altura - a.altura;
        else
            r.bajada += a.altura - b.altura;
        alturaReal = llega;
    }
    for (double c : qAsConst(r.cotas))
        algunTerreno = algunTerreno || std::isfinite(c);
    r.hayTerreno = algunTerreno;
    // El terreno sube mas empinado de lo que puede subir el dron a esa
    // velocidad (tasa / velocidad): por alto que se pongan los puntos, se va
    // quedando por debajo. Se busca el tramo de varios puntos con la subida
    // del terreno mas empinada (mas de 50 m, para no avisar por un bache) en la
    // que el dron, ademas, no llega a la altura del plan.
    if (droneChars.maxClimbRate > 0.0) {
        QVector<double> acumulada(n, 0.0);
        for (int i = 1; i < n; ++i)
            acumulada[i] = acumulada[i - 1] + p[i - 1].distanciaA(p[i]);
        int mejorI = -1, mejorJ = -1;
        double mejorPierde = 0.0, mejorPend = 0.0, mejorV = 0.0;
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(r.cotas[i]))
                continue;
            double sumaV = 0.0;
            for (int j = i; j < n; ++j) {
                sumaV += std::max<double>(p[j].velocidad, 0.0);
                if (j < i + 2 || !std::isfinite(r.cotas[j]) || r.cotas[j] - r.cotas[i] < 50.0)
                    continue;
                const double tramoM = acumulada[j] - acumulada[i];
                const double v = sumaV / (j - i + 1);
                if (tramoM <= 0.0 || v < 0.5)
                    continue;
                const double pend = (r.cotas[j] - r.cotas[i]) / tramoM;
                const double puede = droneChars.maxClimbRate / v;
                // Solo si ahi se queda de verdad por debajo del plan en algun
                // tramo: si la ruta ya va por encima, no hace falta seguir la ladera.
                bool seQueda = false;
                for (int k = i; k < j && k < r.tramos.size(); ++k)
                    seQueda = seQueda || r.tramos[k].alturaLlegada < p[k + 1].altura - 1.0;
                // Se queda con el trozo en que mas metros pierde respecto al
                // terreno (la subida entera, no el pedazo mas empinado), con
                // medio punto de tolerancia en la pendiente.
                const double pierde = (pend - puede) * tramoM;
                if (seQueda && pend - puede > 0.005 && pierde > mejorPierde) {
                    mejorPierde = pierde;
                    mejorI = i;
                    mejorJ = j;
                    mejorPend = pend;
                    mejorV = v;
                }
            }
        }
        if (mejorI >= 0) {
            const double subida = r.cotas[mejorJ] - r.cotas[mejorI];
            const double tramoM = acumulada[mejorJ] - acumulada[mejorI];
            const double vCabe = droneChars.maxClimbRate / mejorPend;
            const QString a = nombre(mejorI), b = nombre(mejorJ);
            QString solucion;
            if (alaFija && vCabe < droneChars.minSpeed)
                solucion = QStringLiteral("Ni a su velocidad mínima (%1 m/s, sube al %2 %) puede seguir esa pendiente: "
                                          "empieza la subida unos %3 m más alto, o hazla más larga (en zigzag u otro "
                                          "rumbo en vez de ir de frente contra la ladera).")
                               .arg(vel(droneChars.minSpeed), num(droneChars.maxClimbRate / droneChars.minSpeed * 100.0),
                                    num(mejorPierde));
            else
                solucion = QStringLiteral("Vuela de %1 a %2 a %3 m/s o menos (así sube al %4 %), empieza la subida unos "
                                          "%5 m más alto, o hazla más larga (en zigzag u otro rumbo en vez de ir de "
                                          "frente contra la ladera).")
                               .arg(a, b, vel(vCabe), num(mejorPend * 100.0), num(mejorPierde));
            anadir(Gravedad::Aviso, QStringLiteral("terreno_empinado"), mejorI, mejorJ,
                   QStringLiteral("El terreno sube más de lo que puede subir el dron"),
                   QStringLiteral("De %1 a %2 el terreno sube %3 m en %4 m (una pendiente del %5 %). A %6 m/s y "
                                  "subiendo a %7 m/s como mucho, el dron sube al %8 %: aunque los puntos estén altos, "
                                  "se irá quedando por debajo de la ruta, unos %9 m al final de ese trozo.")
                       .arg(a, b, num(subida), num(tramoM), num(mejorPend * 100.0), vel(mejorV),
                            vel(droneChars.maxClimbRate), num(droneChars.maxClimbRate / mejorV * 100.0),
                            num(mejorPierde)),
                   solucion);
        }
    }

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
                                  "(alabeo de %5°) y %6 está a %7 m: queda dentro del círculo de giro. "
                                  "Girando al máximo no puede apuntarle, así que se pasará y dará vueltas; "
                                  "puede que no llegue a alcanzarlo (míralo en la trayectoria probable del mapa).")
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
