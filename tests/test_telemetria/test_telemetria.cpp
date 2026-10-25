// Protocolo/Telemetria: cada campo de las tramas del dron acaba en su sitio de
// la MuestraVuelo, con las mismas unidades y arreglos que aplicaba antes
// CControladora (velocidad a km/h, curso deseado en 0..360, desplazamiento
// fijo del cuadroptero, punto al que va en lat/lon).

#include "Protocolo/ProyeccionMercator.h"
#include "Protocolo/Telemetria.h"

#include <QtTest>

class TestTelemetria : public QObject
{
    Q_OBJECT
private slots:
    void alaFijaCampoACampo();
    void cursoDeseadoEn0a360();
    void puntoQueVa();
    void cuadropteroConSuDesplazamiento();
    void motorParaLasGraficas();
};

// Todos los campos del ala fija, con valores distintos para ver que no se
// cruzan (el codigo de antes llego a cambiar cabeceo por banqueo al grabar).
void TestTelemetria::alaFijaCampoACampo()
{
    sTRAMA1 t1;
    t1.Realimentacion_Cabeceo = 1.5f;
    t1.Realimentacion_Banqueo = -2.5f;
    t1.Realimentacion_Velocidad = 15.0f;   // m/s
    t1.Realimentacion_Altura = 120.0f;
    t1.Realimentacion_Curso = 90.0f;
    t1.Realimentacion_Latitud = 23.0f;
    t1.Realimentacion_Longitud = -82.5f;
    t1.Realimentacion_CursoIMO = 91.0f;
    sTRAMA2 t2;
    t2.Control_Estabilizadores = 0.1f;
    t2.Control_Alerones = 0.2f;
    t2.Control_Motor = 0.3f;
    t2.Control_Rumbo = 0.4f;
    t2.Convergencia_Cabeceo = 5.0f;
    t2.Convergencia_Banqueo = 6.0f;
    t2.Distancia_Chequeo = 300.0f;
    t2.Curso_Deseado = 45.0f;
    const float g[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    t2.Ganancia_Cabeceo[0] = g[0]; t2.Ganancia_Cabeceo[1] = g[1];
    t2.Ganancia_Banqueo[0] = g[2]; t2.Ganancia_Banqueo[1] = g[3];
    t2.Ganancia_Velocidad[0] = g[4]; t2.Ganancia_Velocidad[1] = g[5];
    t2.Ganancia_Altura[0] = g[6]; t2.Ganancia_Altura[1] = g[7];
    t2.Ganancia_Curso = g[8];
    t2.Ganancia_Rumbo[0] = g[9]; t2.Ganancia_Rumbo[1] = g[10];
    t2.Realimentacion_Regimen = 2;
    t2.Realimentacion_ComendoMision = 3;
    t2.Cumplir_Punto[0] = 2600000.0f;   // norte
    t2.Cumplir_Punto[1] = -9180000.0f;  // este
    t2.Cumplir_Punto[2] = 150.0f;
    t2.Cantidad_Satelites = 12;

    const MuestraVuelo m = Telemetria::desdeAlaFija(t1, t2);
    QCOMPARE(m.lat, 23.0);
    QCOMPARE(m.lon, -82.5);
    QCOMPARE(m.cabeceo, 1.5);
    QCOMPARE(m.banqueo, -2.5);
    QCOMPARE(m.velocidadKmh, 54.0);
    QCOMPARE(m.altura, 120.0);
    QCOMPARE(m.curso, 90.0);
    QCOMPARE(m.cursoImu, 91.0);
    QCOMPARE(m.distanciaChequeo, 300.0);
    QCOMPARE(m.cursoDeseado, 45.0);
    QCOMPARE(m.regimen, 2);
    QCOMPARE(m.mision, 3);
    QCOMPARE(m.satelites, 12);
    for (int i = 0; i < 11; ++i)
        QCOMPARE(m.ganancias[i], double(g[i]));
    QCOMPARE(m.actuadores[0], double(0.1f));
    QCOMPARE(m.actuadores[1], double(0.2f));
    QCOMPARE(m.actuadores[2], double(0.3f));
    QCOMPARE(m.actuadores[3], double(0.4f));
    QCOMPARE(m.convergenciaBanqueo, 6.0);
    QCOMPARE(m.convergenciaCabeceo, 5.0);
    QCOMPARE(m.puntoNorte, 2600000.0);
    QCOMPARE(m.puntoEste, -9180000.0);
    QCOMPARE(m.puntoAltura, 150.0);
}

// El autopiloto manda el curso deseado entre -180 y 180 (o mas): se pasa a
// 0..360, igual que antes.
void TestTelemetria::cursoDeseadoEn0a360()
{
    QCOMPARE(Telemetria::rumbo0a360(-10.0), 350.0);
    QCOMPARE(Telemetria::rumbo0a360(0.0), 0.0);
    QCOMPARE(Telemetria::rumbo0a360(360.0), 0.0);
    QCOMPARE(Telemetria::rumbo0a360(370.0), 10.0);
    sTRAMA1 t1;
    sTRAMA2 t2;
    t2.Curso_Deseado = -90.0f;
    QCOMPARE(Telemetria::desdeAlaFija(t1, t2).cursoDeseado, 270.0);
}

// Cumplir_Punto (norte, este) en lat/lon con la proyeccion del autopiloto; sin
// punto (0, 0), coordenada invalida (antes caia en el golfo de Guinea).
void TestTelemetria::puntoQueVa()
{
    const QGeoCoordinate habana(23.1, -82.4);
    const QPointF xy = ProyeccionMercator::aMetros(habana);   // x este, y norte
    MuestraVuelo m;
    m.puntoNorte = xy.y();
    m.puntoEste = xy.x();
    const QGeoCoordinate p = Telemetria::puntoQueVa(m);
    QVERIFY(p.isValid());
    QVERIFY(p.distanceTo(habana) < 0.01);
    QVERIFY(!Telemetria::puntoQueVa(MuestraVuelo()).isValid());
}

// Cuadroptero: la posicion en metros (norte en la latitud de la trama) mas el
// desplazamiento fijo de siempre; los actuadores son los cuatro motores y las
// ganancias quedan a 0.
void TestTelemetria::cuadropteroConSuDesplazamiento()
{
    sCUADROPTEROS c;
    c.Realimentacion_Latitud = 0.0f;
    c.Realimentacion_Longitud = 0.0f;
    c.Realimentacion_Velocidad = 5.0f;
    c.Realimentacion_CursoIMU = 33.0f;
    c.ControlM1 = 0.1f;
    c.ControlM2 = 0.2f;
    c.ControlM3 = 0.3f;
    c.ControlM4 = 0.4f;
    c.Curso_Deseado = -45.0f;
    c.Realimentacion_Regimen = 1;
    c.Realimentacion_ComandoMision = 2;
    c.Cantidad_Satelites = 9;
    c.Ganancias_Cabeceo[0] = 7.0f;   // no se usan
    const MuestraVuelo m = Telemetria::desdeCuadroptero(c);
    QVERIFY(qAbs(m.lat - 23.09759) < 1e-9);
    QVERIFY(qAbs(m.lon + 82.44113) < 1e-9);
    QCOMPARE(m.velocidadKmh, 18.0);
    QCOMPARE(m.cursoImu, 33.0);
    QCOMPARE(m.cursoDeseado, 315.0);
    QCOMPARE(m.regimen, 1);
    QCOMPARE(m.mision, 2);
    QCOMPARE(m.satelites, 9);
    QCOMPARE(m.actuadores[0], double(0.1f));
    QCOMPARE(m.actuadores[3], double(0.4f));
    for (double g : m.ganancias)
        QCOMPARE(g, 0.0);

    // 1000 m al norte y 2000 al este del origen, antes del desplazamiento.
    c.Realimentacion_Latitud = 1000.0f;
    c.Realimentacion_Longitud = 2000.0f;
    const QGeoCoordinate sinDesplazar = ProyeccionMercator::aGeo(QPointF(2000.0, 1000.0));
    const MuestraVuelo m2 = Telemetria::desdeCuadroptero(c);
    QVERIFY(qAbs(m2.lat - (sinDesplazar.latitude() + 23.09759)) < 1e-9);
    QVERIFY(qAbs(m2.lon - (sinDesplazar.longitude() - 82.44113)) < 1e-9);
}

// Motor de las graficas: el del ala fija o la media de los cuatro.
void TestTelemetria::motorParaLasGraficas()
{
    MuestraVuelo m;
    m.actuadores[0] = 0.2;
    m.actuadores[1] = 0.4;
    m.actuadores[2] = 0.6;
    m.actuadores[3] = 0.8;
    QCOMPARE(Telemetria::motorParaGrafica(m, 0), 0.6);
    QVERIFY(qAbs(Telemetria::motorParaGrafica(m, 1) - 0.5) < 1e-12);
}

QTEST_GUILESS_MAIN(TestTelemetria)
#include "test_telemetria.moc"
