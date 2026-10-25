// RoutePlanner: las reglas con las que se valida una ruta antes de mandarla al
// dron (distancias, alturas, velocidades, transiciones, radio de giro,
// autonomia). Hasta ahora no tenian ninguna prueba.

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
    p.puntoCritico = false;
    return p;
}
}

class TestPlanificador : public QObject
{
    Q_OBJECT
private slots:
    void distanciaEntrePuntos();
    void alturaYVelocidad();
    void transicionDeVelocidad();
    void transicionDeAltura();
    void pendienteNoSeRedondea();
    void radioDeGiroComoUnGiroCoordinado();
    void rutaValidaYDistanciaTotal();
    void autonomiaExcedida();
};

// Ni mas lejos del maximo ni mas cerca del minimo.
void TestPlanificador::distanciaEntrePuntos()
{
    RoutePlanner rp(alaFija());
    QString error;
    QVERIFY(rp.validarDistanciaWaypoint(punto("P1", 0, 100, 15), punto("P2", 300, 100, 15), error));
    QVERIFY(!rp.validarDistanciaWaypoint(punto("P1", 0, 100, 15), punto("P2", 1200, 100, 15), error));
    QVERIFY(error.contains("excede el máximo"));
    QVERIFY(!rp.validarDistanciaWaypoint(punto("P1", 0, 100, 15), punto("P2", 10, 100, 15), error));
    QVERIFY(error.contains("menor al mínimo"));
}

// Altura entre minimo y maximo; el ala fija no puede ir por debajo de su
// velocidad minima (el cuadroptero si, hasta parado).
void TestPlanificador::alturaYVelocidad()
{
    RoutePlanner rp(alaFija());
    QString error;
    QVERIFY(rp.validarAltitud(punto("P1", 0, 100, 15), error));
    QVERIFY(!rp.validarAltitud(punto("P1", 0, 20, 15), error));
    QVERIFY(!rp.validarAltitud(punto("P1", 0, 600, 15), error));
    QVERIFY(rp.validarVelocidadPunto(punto("P1", 0, 100, 15), error));
    QVERIFY(!rp.validarVelocidadPunto(punto("P1", 0, 100, 30), error));
    QVERIFY(!rp.validarVelocidadPunto(punto("P1", 0, 100, 5), error));
    QVERIFY(error.contains("ala fija"));

    DroneCharacteristics cuadro = alaFija();
    cuadro.type = DroneType::QUADCOPTER;
    RoutePlanner rc(cuadro);
    QVERIFY(rc.validarVelocidadPunto(punto("P1", 0, 100, 0), error));
}

// Pasar de 10 a 20 m/s en 100 m: tiempo 100/15 = 6,7 s, aceleracion 1,5 m/s²
// (cabe en 2); en 50 m serian 3 m/s² (no cabe).
void TestPlanificador::transicionDeVelocidad()
{
    RoutePlanner rp(alaFija());
    QString error;
    QVERIFY(rp.validarTransicionVelocidad(punto("P1", 0, 100, 10), punto("P2", 100, 100, 20), 100.0, error));
    QVERIFY(!rp.validarTransicionVelocidad(punto("P1", 0, 100, 10), punto("P2", 50, 100, 20), 50.0, error));
    QVERIFY(error.contains("aceleración"));
}

// Subir 30 m en 300 m a 15 m/s: 20 s, 1,5 m/s (cabe en 3); subir 100 m en
// 300 m: 5 m/s (no cabe) aunque la pendiente (18°) si.
void TestPlanificador::transicionDeAltura()
{
    RoutePlanner rp(alaFija());
    QString error;
    QVERIFY(rp.validarTransicionAltitud(punto("P1", 0, 100, 15), punto("P2", 300, 130, 15), 300.0, error));
    QVERIFY(!rp.validarTransicionAltitud(punto("P1", 0, 100, 15), punto("P2", 300, 200, 15), 300.0, error));
    QVERIFY(error.contains("ascenso"));
}

// Una pendiente de 30,5° pasa del maximo de 30°: si abs() redondeara a entero
// (int abs(int)) se quedaria en 30 y la daria por buena.
void TestPlanificador::pendienteNoSeRedondea()
{
    RoutePlanner rp(alaFija());
    const double subida = 100.0 * std::tan(qDegreesToRadians(30.5));
    QString error;
    QVERIFY(!rp.validarTransicionAltitud(punto("P1", 0, 100, 15), punto("P2", 100, 100 + subida, 15),
                                         100.0, error));
    QVERIFY2(error.contains("Pendiente"), qPrintable(error));
}

// Radio minimo de un giro coordinado con 30° de alabeo: r = v² / (g·tan 30°);
// a 20 m/s, 70,7 m. El recomendado es 1,5 veces eso.
void TestPlanificador::radioDeGiroComoUnGiroCoordinado()
{
    RoutePlanner rp(alaFija());
    const double esperado = 20.0 * 20.0 / (9.81 * std::tan(qDegreesToRadians(30.0)));
    QVERIFY(qAbs(rp.calcularRadioMinimoTeorico(20.0) - esperado) < 0.1);
    QVERIFY(qAbs(rp.calcularRadioGiroRecomendado(20.0) - 1.5 * esperado) < 0.2);
    QString error;
    QVERIFY(rp.validarRadioGiro(punto("P1", 0, 100, 20, 150), error));
    QVERIFY(!rp.validarRadioGiro(punto("P1", 0, 100, 20, 40), error));   // < minimo configurado
    QVERIFY(!rp.validarRadioGiro(punto("P1", 0, 100, 20, 60), error));   // < teorico
}

// Una ruta recta de 10 puntos cada 300 m, todo dentro de limites: valida, y la
// distancia total es la suma de los tramos (2700 m).
void TestPlanificador::rutaValidaYDistanciaTotal()
{
    RoutePlanner rp(alaFija());
    QList<E_Punto> ruta;
    for (int i = 0; i < 10; ++i)
        ruta.append(punto(QStringLiteral("P%1").arg(i + 1), 300.0 * i, 100, 16));
    const auto v = rp.validarRuta(ruta);
    QVERIFY2(v.first, qPrintable(v.second));
    QVERIFY(qAbs(rp.calcularDistanciaTotal(ruta) - 2700.0) < 1.0);
    // Tiempo: 2700 m a 16 m/s.
    QVERIFY(qAbs(rp.calcularTiempoVuelo(ruta).first - 2700.0 / 16.0) < 0.5);
    QVERIFY(!rp.validarRuta(ruta.mid(0, 1)).first);   // un solo punto
}

// La misma ruta con una autonomia de 2 km no vale.
void TestPlanificador::autonomiaExcedida()
{
    DroneCharacteristics d = alaFija();
    d.maxRange = 2000.0;
    RoutePlanner rp(d);
    QList<E_Punto> ruta;
    for (int i = 0; i < 10; ++i)
        ruta.append(punto(QStringLiteral("P%1").arg(i + 1), 300.0 * i, 100, 16));
    const auto v = rp.validarRuta(ruta);
    QVERIFY(!v.first);
    QVERIFY(v.second.contains("autonomía"));
}

QTEST_GUILESS_MAIN(TestPlanificador)
#include "test_planificador.moc"
