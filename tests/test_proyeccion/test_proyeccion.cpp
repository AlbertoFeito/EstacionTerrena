// Pruebas de ProyeccionMercator: es lo que manda las coordenadas al
// autopiloto, asi que se comprueba contra la definicion publicada de EPSG:3857
// y contra numeros sacados de la libreria antigua (CGoogleMercator).

#include "Protocolo/ProyeccionMercator.h"

#include <QtTest>

class TestProyeccion : public QObject
{
    Q_OBJECT
private slots:
    void origen();
    void limitesEpsg3857();
    void mismaSalidaQueLaLibreriaAntigua();
    void idaYVuelta();
};

// (0, 0) es el origen de la proyeccion.
void TestProyeccion::origen()
{
    const QPointF p = ProyeccionMercator::aMetros(0.0, 0.0);
    QCOMPARE(p.x(), 0.0);
    QVERIFY(qAbs(p.y()) < 1e-9);
}

// Valores de la definicion de EPSG:3857 (fuente independiente): el meridiano
// 180 cae en x = 20037508.342789244 m y la latitud limite 85.0511287798° en
// y = 20037508.34 m (el mapa es un cuadrado).
void TestProyeccion::limitesEpsg3857()
{
    QCOMPARE(ProyeccionMercator::aMetros(0.0, 180.0).x(), 20037508.342789244);
    QVERIFY(qAbs(ProyeccionMercator::aMetros(85.0511287798066, 0.0).y() - 20037508.342789) < 1e-3);
    QVERIFY(qAbs(ProyeccionMercator::aMetros(-85.0511287798066, 0.0).y() + 20037508.342789) < 1e-3);
}

// La Habana con CGoogleMercator (libreria antigua) daba estos metros; el
// autopiloto los tiene que seguir recibiendo iguales.
void TestProyeccion::mismaSalidaQueLaLibreriaAntigua()
{
    const QPointF p = ProyeccionMercator::aMetros(23.13, -82.38);
    QVERIFY(qAbs(p.x() - (-9170499.651550)) < 1e-6);
    QVERIFY(qAbs(p.y() - 2647747.527495) < 1e-6);
}

// Ida y vuelta por todo Cuba: se recupera la coordenada al micrometro.
void TestProyeccion::idaYVuelta()
{
    for (double lat = 19.5; lat <= 23.5; lat += 0.25) {
        for (double lon = -85.0; lon <= -74.0; lon += 0.5) {
            const QGeoCoordinate c = ProyeccionMercator::aGeo(ProyeccionMercator::aMetros(lat, lon));
            QVERIFY(qAbs(c.latitude() - lat) < 1e-11);
            QVERIFY(qAbs(c.longitude() - lon) < 1e-11);
        }
    }
}

QTEST_APPLESS_MAIN(TestProyeccion)
#include "test_proyeccion.moc"
