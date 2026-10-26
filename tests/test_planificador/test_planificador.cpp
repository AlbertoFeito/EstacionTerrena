// RoutePlanner: la revision de una ruta antes de mandarla al dron. Cada
// problema tiene que salir (todos, no solo el primero), en su sitio, con la
// gravedad que toca y con un arreglo que lleve el numero bueno.

#include "Planificacion/InformeRuta.h"
#include "Planificacion/RoutePlanner.h"

#include <QtTest>

namespace {
// Un ala fija tipica para las pruebas.
DroneCharacteristics alaFija()
{
    DroneCharacteristics d;
    d.type = DroneType::FIXED_WING;
    d.maxSpeed = 25.0;
    d.minSpeed = 10.0;
    d.maxAcceleration = 2.0;
    d.maxDeceleration = 3.0;
    d.minTurnRadius = 50.0;
    d.maxWaypointDistance = 1000.0;
    d.minWaypointDistance = 20.0;
    d.minAltitude = 30.0;
    d.maxAltitude = 500.0;
    d.maxClimbRate = 3.0;
    d.maxDescentRate = 4.0;
    d.maxRange = 20000.0;
    d.endurance = 3600.0;
    return d;
}

// Un punto a \a metrosEste del origen (La Habana), a esa altura y velocidad.
E_Punto punto(const QString &nombre, double metrosEste, double altura, double velocidad,
              double radio = 150.0)
{
    E_Punto p;
    p.id = 0;
    p.nombre = nombre;
    p.modo = 0;
    p.pos = QGeoCoordinate(23.1, -82.4).atDistanceAndAzimuth(metrosEste, 90.0);
    p.altura = float(altura);
    p.velocidad = float(velocidad);
    p.radio = float(radio);
    p.distanciaAnterior = 0;
    p.altitudObjetivo = float(altura);
    p.tiempoTransicion = 0;
    p.tasaAscenso = 0;
    return p;
}

// Una ruta recta hacia el este: un punto cada \a paso m, todos a esa altura y velocidad.
QList<E_Punto> recta(int n, double paso, double altura, double velocidad)
{
    QList<E_Punto> ruta;
    for (int i = 0; i < n; ++i)
        ruta.append(punto(QStringLiteral("P%1").arg(i + 1), paso * i, altura, velocidad));
    return ruta;
}

// Los hallazgos con ese codigo.
QVector<Hallazgo> conCodigo(const RevisionRuta &r, const QString &codigo)
{
    QVector<Hallazgo> v;
    for (const Hallazgo &h : r.hallazgos)
        if (h.codigo == codigo)
            v.append(h);
    return v;
}
}

class TestPlanificador : public QObject
{
    Q_OBJECT
private slots:
    void rutaRectaSinProblemas();
    void alturaYVelocidadFueraDeLimites();
    void alturaSobreElTerreno();
    void generarRutaSobreElTerreno();
    void laderaQueElDronNoSube();
    void trayectoriaProbable();
    void proponerRutas();
    void loVeTodoNoSoloElPrimero();
    void separacionEntrePuntos();
    void aceleracionConSuDistancia();
    void subidaLimitadaSeArrastra();
    void terrenoConLaAlturaReal();
    void sinDatosDeTerrenoSeDice();
    void giroCerradoDelAlaFija();
    void anguloGiroGeodesico();
    void radioDeGiroComoUnGiroCoordinado();
    void autonomia();
    void informe();
};

// Una recta de 10 puntos cada 300 m dentro de todos los limites: nada que
// decir. La distancia es la suma de los tramos y el tiempo, a 16 m/s.
void TestPlanificador::rutaRectaSinProblemas()
{
    RoutePlanner rp(alaFija());
    const auto ruta = recta(10, 300, 100, 16);
    const RevisionRuta r = rp.revisar(ruta);
    QVERIFY2(r.hallazgos.isEmpty(), qPrintable(r.hallazgos.value(0).explicacion));
    QVERIFY(r.valida());
    QVERIFY(qAbs(r.distancia - 2700.0) < 1.0);
    QVERIFY(qAbs(r.tiempo - 2700.0 / 16.0) < 0.5);
    QCOMPARE(r.tramos.size(), 9);

    const RevisionRuta uno = rp.revisar(ruta.mid(0, 1));
    QVERIFY(!uno.valida());
    QCOMPARE(uno.hallazgos.first().codigo, QStringLiteral("pocos_puntos"));
}

// Altura y velocidad fuera de lo que puede el dron: error en ese punto y el
// arreglo con el limite. El cuadroptero puede ir parado; el ala fija, no.
void TestPlanificador::alturaYVelocidadFueraDeLimites()
{
    RoutePlanner rp(alaFija());
    auto ruta = recta(3, 300, 100, 16);
    ruta[1].altura = 20;
    ruta[2].velocidad = 5;
    const RevisionRuta r = rp.revisar(ruta);
    QVERIFY(!r.valida());

    const auto baja = conCodigo(r, "altura_baja");
    QCOMPARE(baja.size(), 1);
    QCOMPARE(baja.first().desde, 1);
    QCOMPARE(baja.first().lugar, QStringLiteral("P2"));
    QCOMPARE(baja.first().gravedad, Hallazgo::Gravedad::Error);
    QVERIFY2(baja.first().solucion.contains("30 m"), qPrintable(baja.first().solucion));

    const auto lenta = conCodigo(r, "velocidad_baja");
    QCOMPARE(lenta.size(), 1);
    QVERIFY(lenta.first().explicacion.contains("pérdida"));
    QVERIFY(lenta.first().solucion.contains("10.0 m/s"));

    ruta = recta(3, 300, 100, 16);
    ruta[0].velocidad = 30;
    QCOMPARE(conCodigo(rp.revisar(ruta), "velocidad_alta").size(), 1);
    ruta[0].altura = 600;
    QCOMPARE(conCodigo(rp.revisar(ruta), "altura_alta").size(), 1);

    DroneCharacteristics cuadro = alaFija();
    cuadro.type = DroneType::QUADCOPTER;
    auto parada = recta(3, 300, 100, 5);
    parada[1].velocidad = 0;
    QVERIFY(conCodigo(RoutePlanner(cuadro).revisar(parada), "velocidad_baja").isEmpty());
}

// Los limites de altura del dron (30 a 500 m) cuentan sobre el terreno. Con
// el terreno a 600 m: un punto a 620 m va a 20 m (error, y el arreglo dice
// 630 m sobre el mar); uno a 500 m esta bajo tierra (como la ruta plana de la
// captura del usuario); uno a 1200 m va a 600 m (error por arriba); a 700 m
// va bien. Antes se comparaba la altura sobre el mar y los cuatro "pasaban".
void TestPlanificador::alturaSobreElTerreno()
{
    RoutePlanner rp(alaFija());
    auto meseta = [](const QGeoCoordinate &) { return 600.0; };
    auto ruta = recta(4, 300, 700, 16);
    ruta[0].altura = 620;
    ruta[1].altura = 500;
    ruta[3].altura = 1200;
    const RevisionRuta r = rp.revisar(ruta, meseta);
    QVERIFY(qAbs(r.cotas.value(0) - 600.0) < 1e-9);
    QVERIFY(qAbs(r.sobreTerrenoMin + 100.0) < 0.5);
    QVERIFY(qAbs(r.sobreTerrenoMax - 600.0) < 0.5);

    const auto bajas = conCodigo(r, "altura_baja");
    QCOMPARE(bajas.size(), 2);
    QVERIFY2(bajas[0].explicacion.contains("20 m sobre el terreno"), qPrintable(bajas[0].explicacion));
    QVERIFY2(bajas[0].solucion.contains("630 m sobre el mar"), qPrintable(bajas[0].solucion));
    QCOMPARE(bajas[1].desde, 1);
    QCOMPARE(conCodigo(r, "altura_alta").size(), 1);
    QCOMPARE(conCodigo(r, "altura_alta").first().desde, 3);
    QVERIFY(!conCodigo(r, "terreno_choca").isEmpty());   // P2 bajo tierra
    for (const Hallazgo &h : r.hallazgos)
        QVERIFY(!(h.codigo.startsWith("altura") && h.desde == 2));   // P3, a 100 m, bien
}

// La ruta por defecto: 10 puntos al este cada (20 + 1000) / 2 = 510 m. Con
// terreno a 100 m y un pico de 200 m entre P3 y P4 (a 1275 m de P1), P3 y P4
// quedan a 250 m (50 sobre el pico) y los demas a 150: la recta entre dos
// puntos no baja de 50 m sobre el terreno. Sin elevacion, a 50 m sobre el mar
// y conTerreno = false.
void TestPlanificador::generarRutaSobreElTerreno()
{
    RoutePlanner rp(alaFija());
    const QGeoCoordinate p1(23.1, -82.4);
    const QGeoCoordinate pico = p1.atDistanceAndAzimuth(1275, 90);
    auto terreno = [pico](const QGeoCoordinate &c) { return c.distanceTo(pico) < 15 ? 200.0 : 100.0; };
    bool conTerreno = false;
    const QList<E_Punto> ruta = rp.generarRuta(p1, 50, terreno, &conTerreno);
    QVERIFY(conTerreno);
    QCOMPARE(ruta.size(), 10);
    QVERIFY(qAbs(ruta[1].distanciaA(ruta[2]) - 510.0) < 1.0);
    for (int i = 0; i < ruta.size(); ++i) {
        const double esperada = (i == 2 || i == 3) ? 250.0 : 150.0;
        QVERIFY2(qAbs(ruta[i].altura - esperada) < 0.5, qPrintable(QStringLiteral("P%1 a %2").arg(i + 1).arg(ruta[i].altura)));
    }
    QVERIFY(qAbs(ruta[0].velocidad - 25.0 * 0.65) < 0.01);
    const RevisionRuta r = rp.revisar(ruta, terreno);
    QVERIFY(conCodigo(r, "terreno_choca").isEmpty() && conCodigo(r, "terreno_roza").isEmpty());

    auto nada = [](const QGeoCoordinate &) { return std::numeric_limits<double>::quiet_NaN(); };
    const QList<E_Punto> sin = rp.generarRuta(p1, 50, nada, &conTerreno);
    QVERIFY(!conTerreno);
    QVERIFY(qAbs(sin[5].altura - 50.0) < 0.01);
}

// Una ladera al 25 % (como la ruta "hhh" del usuario, algo mas empinada) y
// la ruta a 100 m sobre ella a 16 m/s: el dron sube al 3/16 = 19 %, pierde
// unos 20 m por tramo y acaba chocando aunque el plan libre el terreno. El
// error tiene que decir que el plan va bien y que subir los puntos no sirve,
// y proponer lo que si (mas despacio, empezar mas alto, subida mas larga). Un
// aviso de ruta lo dice de entrada. El generador, en la misma ladera, baja la
// velocidad y la ruta sale sin choques ni subidas imposibles.
void TestPlanificador::laderaQueElDronNoSube()
{
    RoutePlanner rp(alaFija());
    const QGeoCoordinate p1(23.1, -82.4);
    auto ladera = [p1](const QGeoCoordinate &c) { return 500.0 + 0.25 * p1.distanceTo(c); };
    QList<E_Punto> ruta;
    for (int i = 0; i < 10; ++i) {
        E_Punto pt = punto(QStringLiteral("P%1").arg(i + 1), 300.0 * i, 0, 16);
        pt.altura = float(ladera(pt.pos) + 100.0);
        ruta.append(pt);
    }
    const RevisionRuta r = rp.revisar(ruta, ladera);
    QVector<Hallazgo> choque = conCodigo(r, "terreno_choca");
    choque += conCodigo(r, "terreno_roza");
    QVERIFY(!choque.isEmpty());
    const Hallazgo &c = choque.first();
    QVERIFY2(c.explicacion.contains("Subir los puntos no lo arregla"), qPrintable(c.explicacion));
    QVERIFY2(c.explicacion.contains("desde P1"), qPrintable(c.explicacion));
    QVERIFY2(c.solucion.contains("m/s o menos"), qPrintable(c.solucion));
    QVERIFY(c.solucion.contains("empieza la subida más alto: P1 a"));
    QVERIFY(!c.solucion.contains("Sube P"));

    const auto empinado = conCodigo(r, "terreno_empinado");
    QCOMPARE(empinado.size(), 1);
    QVERIFY2(empinado.first().explicacion.contains("25 %"), qPrintable(empinado.first().explicacion));
    QVERIFY(empinado.first().explicacion.contains("19 %"));
    QVERIFY2(empinado.first().solucion.contains("12.0 m/s o menos"), qPrintable(empinado.first().solucion));

    // Con un plan bajo (no por la subida), el arreglo sigue siendo subir los puntos.
    auto meseta = [](const QGeoCoordinate &) { return 600.0; };
    const RevisionRuta bajo = rp.revisar(recta(2, 300, 590, 16), meseta);
    QVERIFY(conCodigo(bajo, "terreno_choca").value(0).solucion.startsWith("Sube P1 y P2"));

    // El generador en la misma ladera: mas despacio donde sube, sin choques.
    const QList<E_Punto> generada = rp.generarRuta(p1, 100, ladera);
    QVERIFY(generada[3].velocidad < 16.25f);
    QVERIFY(generada[3].velocidad >= 10.0f);
    const RevisionRuta rg = rp.revisar(generada, ladera);
    QVERIFY(conCodigo(rg, "terreno_choca").isEmpty() && conCodigo(rg, "terreno_roza").isEmpty());
    QVERIFY(conCodigo(rg, "sube_rapido").isEmpty());
    QVERIFY(conCodigo(rg, "terreno_empinado").isEmpty());
}

// Por donde iria en planta. En recta, por la recta (y lo que mide). En un giro
// de 90° a 20 m/s (radio 70,6 m) con acercamiento de 20 m: cambia de punto a
// 20 m de P2, pero se pasa de la esquina unos 50 m mientras gira. Con P3 a 100 m (dentro
// de su circulo de giro) da vueltas sin alcanzarlo: el camino es mucho mas
// largo que los 400 m del plan. Y en el perfil, la altura real.
void TestPlanificador::trayectoriaProbable()
{
    RoutePlanner rp(alaFija());
    auto largo = [](const QVector<QGeoCoordinate> &c) {
        double m = 0.0;
        for (int i = 1; i < c.size(); ++i)
            m += c[i - 1].distanceTo(c[i]);
        return m;
    };
    auto masCerca = [](const QVector<QGeoCoordinate> &c, const QGeoCoordinate &a) {
        double m = 1e12;
        for (const QGeoCoordinate &q : c)
            m = std::min(m, q.distanceTo(a));
        return m;
    };
    const auto recto = rp.trayectoriaProbable(recta(4, 300, 100, 16), 20);
    QVERIFY(qAbs(largo(recto) - 880.0) < 15.0);   // 900 m menos los 20 del ultimo acercamiento

    auto ruta = recta(2, 300, 100, 20);
    ruta.append(punto("P3", 0, 100, 20));
    ruta[2].pos = ruta[1].pos.atDistanceAndAzimuth(300, 0);
    const auto giro = rp.trayectoriaProbable(ruta, 20);
    // Cambia a 20 m de P2, pero con 70,6 m de radio sigue hacia el este
    // mientras gira: se pasa unos r - 20 = 50 m de la esquina.
    double pasa = 0.0;
    for (const QGeoCoordinate &q : giro) {
        const double az = ruta[1].pos.azimuthTo(q);
        pasa = std::max(pasa, ruta[1].pos.distanceTo(q) * std::sin(qDegreesToRadians(az)));
    }
    QVERIFY2(qAbs(pasa - 50.6) < 6.0, qPrintable(QString::number(pasa)));
    QVERIFY(masCerca(giro, ruta[2].pos) <= 21.0);

    ruta[2].pos = ruta[1].pos.atDistanceAndAzimuth(100, 0);
    const auto vuelta = rp.trayectoriaProbable(ruta, 20);
    // Dentro del circulo de giro no lo alcanza girando al maximo: da vueltas
    // (la simulacion corta tras unas cuantas), mucho mas camino que el plan.
    QVERIFY2(largo(vuelta) > 400.0 + 200.0, qPrintable(QString::number(largo(vuelta))));

    // Perfil real: empieza en P1 y acaba a la altura a la que llega al final.
    QList<E_Punto> sube{punto("P1", 0, 100, 15), punto("P2", 300, 200, 15)};
    const RevisionRuta r = rp.revisar(sube);
    QVERIFY(!r.perfilReal.isEmpty());
    QVERIFY(qAbs(r.perfilReal.first().y() - 100.0) < 0.01);
    QVERIFY(qAbs(r.perfilReal.last().x() - 300.0) < 1.0);
    QVERIFY(qAbs(r.perfilReal.last().y() - 160.0) < 0.5);
}

// El optimizador. En llano, las tres propuestas son la recta. Con una cresta
// de 300 m que cruza la recta (de 1300 a 1700 m de P1) y un paso a 400-700 m
// al norte, la Segura va por el paso: menos altura y sin avisos de subida,
// aunque sea mas larga; la Rapida no es mas larga que la Segura. Con P10 a
// 10 km no caben 10 puntos (9 tramos de 1000 m como mucho): lo dice.
void TestPlanificador::proponerRutas()
{
    RoutePlanner rp(alaFija());
    const QGeoCoordinate p1(23.1, -82.4);
    const QGeoCoordinate p10 = p1.atDistanceAndAzimuth(2700, 90);
    auto llano = [](const QGeoCoordinate &) { return 100.0; };
    const auto planas = rp.proponerRutas(p1, p10, 50, llano, 1000);
    QCOMPARE(planas.size(), 4);
    for (const PropuestaRuta &pr : planas) {
        QVERIFY2(pr.ok, qPrintable(pr.motivo));
        QCOMPARE(pr.puntos.size(), 10);
        QVERIFY2(qAbs(pr.revision.distancia - 2700.0) < 5.0, qPrintable(pr.nombre));
        QVERIFY(pr.puntos.last().pos.distanceTo(p10) < 1.0);
    }

    auto cresta = [p1](const QGeoCoordinate &c) {
        const double d = p1.distanceTo(c), az = qDegreesToRadians(p1.azimuthTo(c));
        const double este = d * std::sin(az), norte = d * std::cos(az);
        const bool enCresta = este > 1300 && este < 1700;
        const bool enPaso = norte > 400 && norte < 700;
        return enCresta && !enPaso ? 400.0 : 100.0;
    };
    const auto rutas = rp.proponerRutas(p1, p10, 50, cresta, 1000);
    QCOMPARE(rutas.size(), 4);
    const PropuestaRuta &segura = rutas[0], &rapida = rutas[2], &recta = rutas[3];
    QCOMPARE(segura.nombre, QStringLiteral("Segura"));
    QCOMPARE(recta.nombre, QStringLiteral("Recta"));
    QVERIFY2(recta.revision.alturaMax > 400.0, qPrintable(QString::number(recta.revision.alturaMax)));
    QVERIFY2(segura.revision.alturaMax < 200.0, qPrintable(QString::number(segura.revision.alturaMax)));
    QVERIFY(segura.revision.distancia > recta.revision.distancia);
    QVERIFY(segura.revision.valida());
    QVERIFY(conCodigo(segura.revision, "sube_rapido").isEmpty());
    QVERIFY(rapida.revision.distancia <= segura.revision.distancia + 1.0);

    const auto lejos = rp.proponerRutas(p1, p1.atDistanceAndAzimuth(10000, 90), 50, llano, 1000);
    QCOMPARE(lejos.size(), 1);
    QVERIFY(!lejos.first().ok);
    QVERIFY2(lejos.first().motivo.contains("9000 m"), qPrintable(lejos.first().motivo));
}

// Antes la validacion se paraba en el primer problema; ahora salen todos, y
// los errores antes que los avisos.
void TestPlanificador::loVeTodoNoSoloElPrimero()
{
    RoutePlanner rp(alaFija());
    auto ruta = recta(4, 300, 100, 16);
    ruta[0].altura = 20;           // error
    ruta[2].velocidad = 30;        // error
    ruta[3].pos = ruta[2].pos.atDistanceAndAzimuth(10, 90);   // error: muy juntos
    const RevisionRuta r = rp.revisar(ruta);
    QCOMPARE(r.cuantos(Hallazgo::Gravedad::Error), 3);
    QVERIFY(r.cuantos(Hallazgo::Gravedad::Aviso) >= 1);   // de 30 a 16 m/s en 10 m no frena
    QCOMPARE(r.hallazgos.first().gravedad, Hallazgo::Gravedad::Error);
    QCOMPARE(r.hallazgos.last().gravedad, Hallazgo::Gravedad::Aviso);
    QCOMPARE(r.resumenErrores().count('\n'), 2);
}

// Un tramo demasiado largo o demasiado corto es de los dos puntos.
void TestPlanificador::separacionEntrePuntos()
{
    RoutePlanner rp(alaFija());
    auto ruta = recta(2, 1200, 100, 16);
    auto largo = conCodigo(rp.revisar(ruta), "tramo_largo");
    QCOMPARE(largo.size(), 1);
    QCOMPARE(largo.first().lugar, QStringLiteral("P1 → P2"));
    QVERIFY(largo.first().tocaA(0) && largo.first().tocaA(1));
    QVERIFY(largo.first().solucion.contains("1000 m"));

    ruta = recta(2, 10, 100, 16);
    QCOMPARE(conCodigo(rp.revisar(ruta), "tramo_corto").size(), 1);
}

// De 10 a 20 m/s acelerando a 2 m/s²: en 100 m llega (v² = 100 + 2·2·100,
// 22,4 m/s); en 50 m solo a 17,3 m/s y hacen falta 75 m. Es aviso: lo vuela,
// pero llega mas despacio.
void TestPlanificador::aceleracionConSuDistancia()
{
    RoutePlanner rp(alaFija());
    QList<E_Punto> ruta{punto("P1", 0, 100, 10), punto("P2", 100, 100, 20)};
    QVERIFY(rp.revisar(ruta).hallazgos.isEmpty());

    ruta[1] = punto("P2", 50, 100, 20);
    const auto a = conCodigo(rp.revisar(ruta), "acelera_poco");
    QCOMPARE(a.size(), 1);
    QCOMPARE(a.first().gravedad, Hallazgo::Gravedad::Aviso);
    QVERIFY2(a.first().explicacion.contains("17.3"), qPrintable(a.first().explicacion));
    QVERIFY2(a.first().solucion.contains("75 m"), qPrintable(a.first().solucion));

    ruta = {punto("P1", 0, 100, 20), punto("P2", 50, 100, 10)};   // frena a 3: 20² - 2·3·50 = 100 -> 10, llega justo
    QVERIFY(rp.revisar(ruta).hallazgos.isEmpty());
    ruta[1] = punto("P2", 30, 100, 10);
    QCOMPARE(conCodigo(rp.revisar(ruta), "frena_poco").size(), 1);
}

// Subir 100 m en 300 m a 15 m/s (20 s) pide 5 m/s y el dron sube a 3: llega
// a P2 a 160 m. El tramo siguiente empieza ahi, no a 200 m.
void TestPlanificador::subidaLimitadaSeArrastra()
{
    RoutePlanner rp(alaFija());
    QList<E_Punto> ruta{punto("P1", 0, 100, 15), punto("P2", 300, 200, 15), punto("P3", 600, 230, 15)};
    const RevisionRuta r = rp.revisar(ruta);
    QVERIFY(r.valida());   // aviso, no error
    const auto sube = conCodigo(r, "sube_rapido");
    QCOMPARE(sube.size(), 2);
    QVERIFY2(sube[0].explicacion.contains("unos 160 m"), qPrintable(sube[0].explicacion));
    QVERIFY(sube[0].solucion.contains("Baja P2 a 160 m"));
    QVERIFY(sube[0].solucion.contains("500 m"));   // 100 m / 3 m/s · 15 m/s
    // P2 -> P3: de 160 a 230 en 20 s, otra vez no le da (y lo dice).
    QVERIFY2(sube[1].explicacion.contains("ya llega a 160 m"), qPrintable(sube[1].explicacion));
    QVERIFY(qAbs(r.tramos[0].alturaLlegada - 160.0) < 0.5);
    QVERIFY(qAbs(r.tramos[1].alturaLlegada - 220.0) < 0.5);
}

// Una loma de 150 m en medio del tramo: a 100 m choca, a 155 roza (error), a
// 170 hay poco margen (aviso) y a 200 nada. Se compara con la altura a la que
// va de verdad: si el plan la libra pero no le da para subir, choca.
void TestPlanificador::terrenoConLaAlturaReal()
{
    RoutePlanner rp(alaFija());
    const QGeoCoordinate centro = QGeoCoordinate(23.1, -82.4).atDistanceAndAzimuth(500, 90);
    auto loma = [centro](const QGeoCoordinate &c) { return c.distanceTo(centro) < 60 ? 150.0 : 20.0; };

    auto codigos = [&](double altura) {
        QStringList c;
        for (const Hallazgo &h : rp.revisar(recta(2, 1000, altura, 16), loma).hallazgos)
            c << h.codigo;
        return c;
    };
    QCOMPARE(codigos(100), QStringList{"terreno_choca"});
    QCOMPARE(codigos(155), QStringList{"terreno_roza"});
    QCOMPARE(codigos(170), QStringList{"terreno_cerca"});
    QVERIFY(codigos(200).isEmpty());

    const RevisionRuta r = rp.revisar(recta(2, 1000, 100, 16), loma);
    QVERIFY(!r.valida());
    QVERIFY(r.hayTerreno);
    QVERIFY(qAbs(r.holguraTerreno + 50.0) < 0.5);
    const Hallazgo choca = r.hallazgos.first();
    QVERIFY2(choca.solucion.contains("180 m"), qPrintable(choca.solucion));   // 150 + 30

    // Con un dron que sube a 1 m/s: el plan va de 100 a 300 m (al llegar a la
    // loma, a 440 m de P1, ya iria a 188 m y la libraria), pero en esos 27,5 s
    // solo sube 27,5 m: pasa a 127,5 m y choca.
    DroneCharacteristics lento = alaFija();
    lento.maxClimbRate = 1.0;
    QList<E_Punto> subiendo{punto("P1", 0, 100, 16), punto("P2", 1000, 300, 16)};
    const RevisionRuta rs = RoutePlanner(lento).revisar(subiendo, loma);
    QCOMPARE(conCodigo(rs, "terreno_choca").size(), 1);
    QCOMPARE(conCodigo(rs, "sube_rapido").size(), 1);
}

// Sin elevacion para la zona no se puede comprobar el terreno: una nota.
void TestPlanificador::sinDatosDeTerrenoSeDice()
{
    RoutePlanner rp(alaFija());
    auto nada = [](const QGeoCoordinate &) { return std::numeric_limits<double>::quiet_NaN(); };
    const RevisionRuta r = rp.revisar(recta(3, 300, 100, 16), nada);
    QVERIFY(r.valida());
    QVERIFY(!r.hayTerreno);
    QCOMPARE(conCodigo(r, "sin_terreno").size(), 1);
    QCOMPARE(conCodigo(r, "sin_terreno").first().gravedad, Hallazgo::Gravedad::Nota);
    QVERIFY(rp.revisar(recta(3, 300, 100, 16)).hallazgos.isEmpty());   // sin funcion, ni se mira
}

// A 20 m/s el ala fija gira con 70,6 m de radio. Girar 90° en P2 con P3 a
// 100 m: P3 queda dentro del circulo de giro (2·r·sen 90° = 141 m), aviso; el
// arreglo da la velocidad que cabe, √(50·g·tan 30°) = 16,8 m/s. Con P3 a
// 200 m, nada. El cuadroptero gira en el sitio.
void TestPlanificador::giroCerradoDelAlaFija()
{
    RoutePlanner rp(alaFija());
    auto ruta = recta(2, 300, 100, 20);
    ruta.append(punto("P3", 0, 100, 20));
    ruta[2].pos = ruta[1].pos.atDistanceAndAzimuth(100, 0);
    const auto giro = conCodigo(rp.revisar(ruta), "giro_cerrado");
    QCOMPARE(giro.size(), 1);
    QCOMPARE(giro.first().desde, 1);
    QCOMPARE(giro.first().gravedad, Hallazgo::Gravedad::Aviso);
    QVERIFY2(giro.first().solucion.contains("16.8 m/s"), qPrintable(giro.first().solucion));
    QVERIFY(giro.first().solucion.contains("141 m"));

    ruta[2].pos = ruta[1].pos.atDistanceAndAzimuth(200, 0);
    QVERIFY(conCodigo(rp.revisar(ruta), "giro_cerrado").isEmpty());

    DroneCharacteristics cuadro = alaFija();
    cuadro.type = DroneType::QUADCOPTER;
    ruta[2].pos = ruta[1].pos.atDistanceAndAzimuth(100, 0);
    QVERIFY(conCodigo(RoutePlanner(cuadro).revisar(ruta), "giro_cerrado").isEmpty());
}

// Un angulo recto en el terreno es de 90° tambien a 60° de latitud (con
// grados de latitud y longitud sin mas salian unos 63°).
void TestPlanificador::anguloGiroGeodesico()
{
    RoutePlanner rp(alaFija());
    E_Punto a = punto("A", 0, 100, 15), b = a, c = a;
    a.pos = QGeoCoordinate(60.0, 10.0);
    b.pos = a.pos.atDistanceAndAzimuth(1000, 90);
    c.pos = b.pos.atDistanceAndAzimuth(1000, 0);
    QVERIFY(qAbs(rp.anguloGiro(a, b, c) - 90.0) < 1.0);
    QCOMPARE(rp.anguloGiro(a, b, b), 0.0);
}

// Radio de un giro coordinado con 30° de alabeo, r = v² / (g·tan 30°): a
// 20 m/s, 70,6 m; y nunca menos que el minimo configurado (50 m).
void TestPlanificador::radioDeGiroComoUnGiroCoordinado()
{
    RoutePlanner rp(alaFija());
    const double esperado = 20.0 * 20.0 / (9.81 * std::tan(qDegreesToRadians(30.0)));
    QVERIFY(qAbs(rp.radioDeGiro(20.0) - esperado) < 0.1);
    QCOMPARE(rp.radioDeGiro(10.0), 50.0);
}

// 2,7 km con 2 km de autonomia: error, con lo que sobra. Con 3 km usa el
// 90 %: aviso (hay que ir y volver).
void TestPlanificador::autonomia()
{
    DroneCharacteristics d = alaFija();
    d.maxRange = 2000.0;
    auto e = conCodigo(RoutePlanner(d).revisar(recta(10, 300, 100, 16)), "autonomia_distancia");
    QCOMPARE(e.size(), 1);
    QVERIFY(e.first().solucion.contains("700 m"));
    QCOMPARE(e.first().desde, -1);

    d.maxRange = 3000.0;
    const RevisionRuta r = RoutePlanner(d).revisar(recta(10, 300, 100, 16));
    QVERIFY(r.valida());
    QCOMPARE(conCodigo(r, "autonomia_justa").size(), 1);

    d.maxRange = 20000.0;
    d.endurance = 150.0;   // la ruta dura 169 s
    QCOMPARE(conCodigo(RoutePlanner(d).revisar(recta(10, 300, 100, 16)), "autonomia_tiempo").size(), 1);
}

// El informe: el estado en una frase, cada problema con su arreglo y los
// lugares como enlaces a su punto de la tabla.
void TestPlanificador::informe()
{
    DroneCharacteristics d = alaFija();
    d.maxClimbRate = d.maxDescentRate = 50.0;   // que las subidas no cuenten aqui
    d.maxRange = 1000.0;                        // 900 m: el 90 %, aviso
    RoutePlanner rp(d);
    auto ruta = recta(4, 300, 100, 16);
    QCOMPARE(InformeRuta::estado(rp.revisar(ruta)), QStringLiteral("Válida, con 1 aviso"));

    ruta[2].altura = 20;                         // error en P3
    const RevisionRuta r = rp.revisar(ruta);
    QCOMPARE(InformeRuta::estado(r), QStringLiteral("No válida: 1 error y 1 aviso"));

    const QString html = InformeRuta::html(r, d, QStringLiteral("prueba"), ruta);
    QVERIFY(html.contains("href='punto:2'"));
    QVERIFY(html.contains("Cómo arreglarlo"));
    QVERIFY(html.contains("Cómo se revisa"));
    const QString texto = InformeRuta::texto(r, d, QStringLiteral("prueba"), ruta);
    QVERIFY(texto.contains("[Error] P3: Por debajo de la altura mínima"));
    QVERIFY2(texto.contains("Cómo arreglarlo: Sube P3 a 30 m sobre el mar o más (30 m sobre el terreno)."),
             qPrintable(texto));
}

QTEST_GUILESS_MAIN(TestPlanificador)
#include "test_planificador.moc"
