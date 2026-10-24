// Pruebas de MapaMision: que lo que pide la estacion (dron, casa, rumbos, ruta)
// acaba en el mapa de libmapa como se espera, y que arrastrar un punto de la
// ruta con el raton avisa con su nueva posicion. Corre sin pantalla
// (QT_QPA_PLATFORM=offscreen) sobre un paquete de datos minimo que se fabrica
// en una carpeta temporal.

#include "Mapa/MapaMision.h"

#include <libmapa/MapWidget.h>
#include <qcustomplot.h>

#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <QtMath>
#include <cmath>

namespace {
const QGeoCoordinate kHabana(23.10, -82.40);

// Base de teselas XYZ con una imagen gris de 256 px en los zooms 3..12
// alrededor de La Habana.
bool crearBase(const QString &fichero)
{
    QImage img(256, 256, QImage::Format_RGB32);
    img.fill(QColor(200, 200, 190));
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");

    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("crear"));
        db.setDatabaseName(fichero);
        if (db.open()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("CREATE TABLE tiles (x INTEGER, y INTEGER, z INTEGER, "
                                       "s INTEGER, image BLOB, PRIMARY KEY (x, y, z, s))"));
            db.transaction();
            q.prepare(QStringLiteral("INSERT INTO tiles VALUES (?, ?, ?, 0, ?)"));
            for (int z = 3; z <= 12 && ok; ++z) {
                const int n = 1 << z;
                auto col = [n](double lon) { return int((lon + 180.0) / 360.0 * n); };
                auto fila = [n](double lat) {
                    const double r = qDegreesToRadians(lat);
                    return int((1.0 - std::asinh(std::tan(r)) / M_PI) / 2.0 * n);
                };
                for (int x = col(-82.8); x <= col(-82.0); ++x) {
                    for (int y = fila(23.4); y <= fila(22.8); ++y) {
                        q.addBindValue(x);
                        q.addBindValue(y);
                        q.addBindValue(z);
                        q.addBindValue(png);
                        ok = ok && q.exec();
                    }
                }
            }
            db.commit();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("crear"));
    return ok;
}

// Paquete con dos capas ("osm" y "satelital") sobre la misma base.
bool crearPaquete(const QString &dir)
{
    if (!crearBase(dir + "/osm.sqlitedb") || !crearBase(dir + "/satelital.sqlitedb"))
        return false;
    QJsonArray datasets;
    for (const QString &id : {QStringLiteral("osm"), QStringLiteral("satelital")}) {
        datasets.append(QJsonObject{{"id", id}, {"displayName", id}, {"filePath", id + ".sqlitedb"},
                                    {"zFactor", 1}, {"zOffset", 0}, {"scheme", "XYZ"},
                                    {"minZoom", 3}, {"maxZoom", 12}, {"baseZoom", 3},
                                    {"hasSColumn", true}, {"sValue", 0}});
    }
    const QJsonObject paquete{
        {"format", "libmapa-package"}, {"version", 2},
        {"package", QJsonObject{{"id", "prueba"}, {"name", "Prueba"}, {"attribution", "Teselas de prueba"},
                                {"bounds", QJsonObject{{"north", 23.4}, {"west", -82.8},
                                                       {"south", 22.8}, {"east", -82.0}}}}},
        {"start", QJsonObject{{"layer", "osm"}, {"center", QJsonArray{23.1, -82.4}}, {"zoom", 12}}},
        {"datasets", datasets}};
    QFile f(dir + "/mapa.json");
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(paquete).toJson());
    return true;
}

// Tres puntos de ruta al este de La Habana.
QVector<PuntoRutaMapa> rutaDePrueba()
{
    return {{QGeoCoordinate(23.10, -82.40), "P1"},
            {QGeoCoordinate(23.11, -82.38), "P2"},
            {QGeoCoordinate(23.12, -82.36), "P3"}};
}
}

class TestMapaMision : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();

    void abrePaqueteYCambiaCapa();
    void pintaYQuitaLaRuta();
    void radioDeAcercamientoEnMetrosMercator();
    void puntoCumplidoEnVerde();
    void vehiculoConTrazaYRumbos();
    void limpiarVueloDejaLaRuta();
    void arrastrarUnPuntoAvisa();

private:
    QTemporaryDir m_dir;
    QWidget *m_ventana = nullptr;
    MapaMision *m_mision = nullptr;
};

// Fabrica el paquete una vez para todas las pruebas.
void TestMapaMision::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QVERIFY(crearPaquete(m_dir.path()));
}

// Un mapa nuevo por prueba, en una ventana de tamano fijo y visible.
void TestMapaMision::init()
{
    m_ventana = new QWidget;
    m_ventana->resize(800, 600);
    m_mision = new MapaMision(m_dir.path(), m_dir.path() + "/dibujos.db", m_ventana);
    m_mision->mapa()->setGeometry(0, 0, 800, 600);
    m_ventana->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_ventana));
    m_mision->mapa()->setCenter(QGeoCoordinate(23.11, -82.38));
    m_mision->mapa()->setZoom(12);
}

// Cierra la ventana (y con ella el mapa y MapaMision).
void TestMapaMision::cleanup()
{
    delete m_ventana;
    m_ventana = nullptr;
    m_mision = nullptr;
}

// El paquete abre y se arranca en la satelital, como la libreria antigua.
void TestMapaMision::abrePaqueteYCambiaCapa()
{
    QVERIFY2(m_mision->listo(), qPrintable(m_mision->error()));
    QCOMPARE(m_mision->capaBase(), MapaMision::CapaBase::Satelital);
    QVERIFY(m_mision->ponerCapaBase(MapaMision::CapaBase::OSM));
    QCOMPARE(m_mision->mapa()->baseLayerId(), QStringLiteral("osm"));
}

// Una linea y un punto arrastrable por vertice, cada uno con su anillo.
void TestMapaMision::pintaYQuitaLaRuta()
{
    m_mision->pintarRuta(rutaDePrueba(), true);
    const auto ruta = m_mision->mapa()->featuresInLayer(QStringLiteral("ruta"));
    QCOMPARE(ruta.size(), 4);
    int puntos = 0;
    for (const auto &f : ruta) {
        if (f.kind == libmapa::GeometryKind::Point) {
            ++puntos;
            QVERIFY(f.draggable);
        }
    }
    QCOMPARE(puntos, 3);
    QCOMPARE(m_mision->mapa()->rangeMarks().size(), 3);

    m_mision->pintarRuta(rutaDePrueba(), false);
    QVERIFY(m_mision->mapa()->featuresInLayer(QStringLiteral("ruta")).isEmpty());
    QVERIFY(m_mision->mapa()->rangeMarks().isEmpty());
}

// El radio llega en metros Mercator y se dibuja en metros de terreno: por cos(lat).
void TestMapaMision::radioDeAcercamientoEnMetrosMercator()
{
    m_mision->pintarRuta(rutaDePrueba(), true);
    m_mision->ponerRadioAcercamiento(100.0);
    const auto marcas = m_mision->mapa()->rangeMarks();
    QCOMPARE(marcas.size(), 3);
    const auto anillo = m_mision->mapa()->rangeRings(marcas.first());
    QVERIFY(anillo.has_value());
    QCOMPARE(anillo->count, 1);
    QVERIFY(qAbs(anillo->spacingMeters - 100.0 * std::cos(qDegreesToRadians(23.10))) < 0.01);

    m_mision->ponerRadioAcercamiento(0.0);
    QVERIFY(m_mision->mapa()->rangeMarks().isEmpty());
}

// Un punto cumplido cambia de color, el y su anillo.
void TestMapaMision::puntoCumplidoEnVerde()
{
    m_mision->pintarRuta(rutaDePrueba(), true);
    m_mision->marcarPuntoCumplido(0);
    m_mision->marcarPuntoCumplido(99);   // fuera de rango: no hace nada
    bool hayVerde = false;
    for (qint64 id : m_mision->mapa()->rangeMarks()) {
        const auto anillo = m_mision->mapa()->rangeRings(id);
        if (anillo && anillo->color == QColor(Qt::green))
            hayVerde = true;
    }
    QVERIFY(hayVerde);
    int verdes = 0;
    for (const auto &f : m_mision->mapa()->featuresInLayer(QStringLiteral("ruta")))
        if (f.kind == libmapa::GeometryKind::Point && f.style.fillColor == QColor(Qt::green)) {
            ++verdes;
            QVERIFY(f.draggable);
        }
    QCOMPARE(verdes, 1);
}

// Un solo objetivo para el dron, con su traza; rumbos segun el modo.
void TestMapaMision::vehiculoConTrazaYRumbos()
{
    QPixmap simbolo(20, 20);
    simbolo.fill(Qt::red);
    m_mision->actualizarVehiculo(QGeoCoordinate(23.10, -82.40), 45.0, Qt::red, simbolo, "V: 10");
    m_mision->actualizarVehiculo(QGeoCoordinate(23.105, -82.39), 50.0, Qt::green, simbolo, "V: 12");
    auto *mapa = m_mision->mapa();
    QCOMPARE(mapa->targetCount(), 1);
    const auto dron = mapa->targets().first();
    QCOMPARE(dron.color, QColor(Qt::green));
    QCOMPARE(dron.label, QStringLiteral("V: 12"));
    QCOMPARE(mapa->targetHistory(dron.id).size(), 2);

    m_mision->ponerRumbos(50.0, 60.0, QGeoCoordinate(23.12, -82.36), true);
    QCOMPARE(mapa->rangeMarks().size(), 3);
    for (qint64 id : mapa->rangeMarks()) {
        const auto linea = mapa->bearingLine(id);
        QVERIFY(linea.has_value());
        QCOMPARE(linea->fromTarget, dron.id);
    }

    // Fuera de automatico solo queda el rumbo deseado.
    m_mision->ponerRumbos(50.0, 60.0, QGeoCoordinate(23.12, -82.36), false);
    QCOMPARE(mapa->rangeMarks().size(), 1);
    QCOMPARE(mapa->bearingLine(mapa->rangeMarks().first())->bearingDeg, 60.0);
}

// Al desconectar se va el vuelo (dron, rumbos, casa) pero no la ruta.
void TestMapaMision::limpiarVueloDejaLaRuta()
{
    m_mision->pintarRuta(rutaDePrueba(), true);
    m_mision->ponerRadioAcercamiento(0.0);
    m_mision->ponerHome(kHabana);
    m_mision->actualizarVehiculo(kHabana, 0.0, Qt::red, QPixmap(), QString());
    m_mision->ponerRumbos(0.0, 90.0, QGeoCoordinate(23.12, -82.36), true);
    QCOMPARE(m_mision->mapa()->featuresInLayer(QStringLiteral("mision")).size(), 1);

    m_mision->limpiarVuelo();
    QCOMPARE(m_mision->mapa()->targetCount(), 0);
    QVERIFY(m_mision->mapa()->rangeMarks().isEmpty());
    QVERIFY(m_mision->mapa()->featuresInLayer(QStringLiteral("mision")).isEmpty());
    QCOMPARE(m_mision->mapa()->featuresInLayer(QStringLiteral("ruta")).size(), 4);
}

// Arrastrar el punto P2 con el raton emite puntoRutaMovido(1, donde se solto),
// y la linea de la ruta sigue al punto.
void TestMapaMision::arrastrarUnPuntoAvisa()
{
    m_mision->pintarRuta(rutaDePrueba(), true);
    QSignalSpy movido(m_mision, &MapaMision::puntoRutaMovido);

    auto *plot = qobject_cast<QCustomPlot *>(m_mision->mapa()->customPlot());
    QVERIFY(plot);
    plot->replot();
    auto aPixel = [&](const QGeoCoordinate &c) {
        const QPointF ejes = m_mision->mapa()->toAxisCoords(c);
        return QPoint(qRound(plot->xAxis->coordToPixel(ejes.x())),
                      qRound(plot->yAxis->coordToPixel(ejes.y())));
    };
    const QPoint desde = aPixel(rutaDePrueba().at(1).pos);
    const QPoint hasta = desde + QPoint(60, 40);

    QTest::mousePress(plot, Qt::LeftButton, Qt::NoModifier, desde);
    for (int i = 1; i <= 10; ++i) {
        QMouseEvent mover(QEvent::MouseMove, desde + (hasta - desde) * i / 10,
                          plot->mapToGlobal(desde + (hasta - desde) * i / 10),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(plot, &mover);
    }
    QTest::mouseRelease(plot, Qt::LeftButton, Qt::NoModifier, hasta);

    QCOMPARE(movido.count(), 1);
    QCOMPARE(movido.first().at(0).toInt(), 1);
    const auto pos = movido.first().at(1).value<QGeoCoordinate>();
    const QPoint soltado = aPixel(pos);
    QVERIFY2((soltado - hasta).manhattanLength() <= 2, "el punto no quedo donde se solto");

    for (const auto &f : m_mision->mapa()->featuresInLayer(QStringLiteral("ruta")))
        if (f.kind == libmapa::GeometryKind::Polyline)
            QVERIFY(f.geometry.at(1).distanceTo(pos) < 1.0);

    // Para mirar el resultado a ojo:  MAPAMISION_CAPTURA=fichero.png
    const QString captura = qEnvironmentVariable("MAPAMISION_CAPTURA");
    if (!captura.isEmpty()) {
        m_mision->ponerRadioAcercamiento(150.0);
        m_mision->marcarPuntoCumplido(0);
        m_mision->ponerHome(QGeoCoordinate(23.095, -82.41));
        QPixmap avion(32, 32);
        avion.fill(Qt::transparent);
        {
            QPainter p(&avion);
            p.setBrush(Qt::red);
            p.drawPolygon(QPolygon({QPoint(16, 0), QPoint(28, 30), QPoint(16, 22), QPoint(4, 30)}));
        }
        m_mision->actualizarVehiculo(QGeoCoordinate(23.100, -82.405), 40.0, Qt::red, avion, "V: 40 km/h\nH: 120 m");
        m_mision->actualizarVehiculo(QGeoCoordinate(23.104, -82.398), 45.0, Qt::yellow, avion, "V: 42 km/h\nH: 125 m");
        m_mision->ponerRumbos(45.0, 60.0, pos, true);
        QVERIFY(m_mision->mapa()->renderImage().save(captura));
    }
}

QTEST_MAIN(TestMapaMision)
#include "test_mapamision.moc"
