// Pruebas de MapaMision: que lo que pide la estacion (dron, casa, rumbos, ruta)
// acaba en el mapa de libmapa como se espera, y que arrastrar un punto de la
// ruta con el raton avisa con su nueva posicion. Corre sin pantalla
// (QT_QPA_PLATFORM=offscreen) sobre un paquete de datos minimo que se fabrica
// en una carpeta temporal.

#include "Mapa/MapaMision.h"
#include "Mapa/PanelCapas.h"

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
#include <QTreeWidget>
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

// MBTiles vectorial minimo (una tesela MVT con una linea en la capa
// "transportation"), para que el paquete declare un mapa hibrido de verdad.
bool crearHibrido(const QString &fichero)
{
    auto varint = [](quint32 n) {
        QByteArray out;
        do {
            quint8 b = n & 0x7f;
            n >>= 7;
            out.append(char(n ? (b | 0x80) : b));
        } while (n);
        return out;
    };
    auto campo = [&](quint8 tag, const QByteArray &datos) {
        return QByteArray(1, char(tag)) + varint(quint32(datos.size())) + datos;
    };
    QByteArray geometria;
    for (quint32 v : {9u, 20u, 20u, 10u, 400u, 400u})   // MoveTo(10,10) LineTo(+200,+200)
        geometria += varint(v);
    const QByteArray entidad = QByteArray("\x08\x01\x18\x02", 4) + campo(0x22, geometria);
    const QByteArray capa = QByteArray("\x78\x02", 2) + campo(0x0a, "transportation")
                          + campo(0x12, entidad) + QByteArray("\x28", 1) + varint(4096);
    const QByteArray tesela = campo(0x1a, capa);

    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("hibrido"));
        db.setDatabaseName(fichero);
        if (db.open()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("CREATE TABLE metadata (name TEXT, value TEXT)"))
              && q.exec(QStringLiteral("CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, "
                                       "tile_row INTEGER, tile_data BLOB)"))
              && q.exec(QStringLiteral("INSERT INTO metadata VALUES ('name', 'calles'), ('format', 'pbf'), "
                                       "('minzoom', '3'), ('maxzoom', '12'), "
                                       "('bounds', '-82.8,22.8,-82.0,23.4')"));
            q.prepare(QStringLiteral("INSERT INTO tiles VALUES (12, 1110, 2312, ?)"));
            q.addBindValue(tesela);
            ok = ok && q.exec();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("hibrido"));
    return ok;
}

// Paquete con dos capas ("osm" y "satelital") sobre la misma base, un mapa
// hibrido encima y una capa fija.
bool crearPaquete(const QString &dir)
{
    if (!crearBase(dir + "/osm.sqlitedb") || !crearBase(dir + "/satelital.sqlitedb")
        || !crearHibrido(dir + "/calles.mbtiles"))
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
        {"datasets", datasets},
        // El mapa hibrido que pone libmapa encima de la base: la estacion lo oculta.
        {"vectorOverlay", QJsonObject{{"filePath", "calles.mbtiles"}}},
        // Una capa fija que el paquete pide VISIBLE: la estacion la esconde.
        {"overlays", QJsonArray{QJsonObject{{"id", "costa"}, {"name", "Costa"},
                                            {"file", "costa.geo"}, {"visible", true}}}}};
    QFile geo(dir + "/costa.geo");
    if (!geo.open(QIODevice::WriteOnly))
        return false;
    geo.write("-82.50,23.15,\n-82.40,23.16,\n-82.30,23.15,\n");
    geo.close();
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
    void soloCapasBaseConEscala();
    void capasDelPaquete();
    void panelDeCapas();
    void panelNoDesplazaElMapa();
    void estadoDeCapasSeRestaura();
    void pintaYQuitaLaRuta();
    void radioDeAcercamientoEnMetrosMercator();
    void puntoCumplidoEnVerde();
    void vehiculoConTrazaYRumbos();
    void rumbosDeLargoFijoEnPantalla();
    void rotulosArrastradosSeQuedan();
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
    m_mision = new MapaMision(m_dir.path() + "/mapa.json", m_ventana);
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

// Como los ejemplos: barra de escala, el mapa hibrido cargado pero oculto y las
// capas fijas del paquete escondidas aunque el mapa.json las pida visibles.
void TestMapaMision::soloCapasBaseConEscala()
{
    // El paquete de prueba declara un hibrido valido: un MapWidget a pelo lo pone.
    {
        libmapa::MapConfig cfg;
        cfg.dataDir = m_dir.path();
        cfg.autosaveFeatures = false;
        libmapa::MapWidget aPelo(cfg);
        QVERIFY(aPelo.hasVectorOverlay());
    }
    auto *mapa = m_mision->mapa();
    QVERIFY(mapa->isScaleBarVisible());
    QVERIFY(mapa->hasVectorOverlay());
    QVERIFY(!mapa->isVectorOverlayVisible());
    bool hayCosta = false;
    for (const auto &capa : mapa->featureLayers()) {
        if (capa.id == QStringLiteral("costa")) {
            hayCosta = true;
            QVERIFY(!capa.visible);
        }
    }
    QVERIFY(hayCosta);
}

// El panel de capas ve solo lo del mapa.json: las dos bases, el hibrido y la
// capa fija (no las capas de la estacion), y cada cambio se avisa.
void TestMapaMision::capasDelPaquete()
{
    const QVector<CapaMapa> bases = m_mision->capasBase();
    QCOMPARE(bases.size(), 2);
    QCOMPARE(m_mision->capaBaseId(), QStringLiteral("satelital"));
    int activas = 0;
    for (const CapaMapa &b : bases)
        activas += b.visible ? 1 : 0;
    QCOMPARE(activas, 1);

    QSignalSpy cambios(m_mision, &MapaMision::capasCambiadas);
    QVERIFY(m_mision->ponerCapaBaseId(QStringLiteral("osm")));
    QCOMPARE(m_mision->mapa()->baseLayerId(), QStringLiteral("osm"));
    QVERIFY(!m_mision->ponerCapaBaseId(QStringLiteral("no_existe")));

    QVERIFY(m_mision->hayHibrido());
    QCOMPARE(m_mision->nombreHibrido(), QStringLiteral("calles"));
    QVERIFY(!m_mision->hibridoVisible());
    m_mision->ponerHibridoVisible(true);
    QVERIFY(m_mision->hibridoVisible());
    QVERIFY(m_mision->mapa()->isVectorOverlayVisible());

    const QVector<CapaMapa> fijas = m_mision->capasFijas();
    QCOMPARE(fijas.size(), 1);
    QCOMPARE(fijas.first().id, QStringLiteral("costa"));
    QCOMPARE(fijas.first().nombre, QStringLiteral("Costa"));
    QVERIFY(m_mision->ponerCapaFijaVisible(QStringLiteral("costa"), true));
    QVERIFY(m_mision->capasFijas().first().visible);
    QVERIFY(!m_mision->ponerCapaFijaVisible(QStringLiteral("ruta"), false));   // de la estacion
    QCOMPARE(cambios.count(), 3);
}

// Un panel que quita sitio al mapa (la ventana no cambia) no desplaza lo que
// se ve: la esquina noroeste se queda y se recorta por abajo y por la derecha.
// Si cambia la ventana, se conserva el centro.
void TestMapaMision::panelNoDesplazaElMapa()
{
    auto *mapa = m_mision->mapa();
    QCoreApplication::processEvents();
    const QGeoCoordinate noAntes = mapa->visibleNorthWest();

    mapa->resize(800, 380);   // el panel de rutas, abajo
    QCoreApplication::processEvents();
    QVERIFY(mapa->visibleNorthWest().distanceTo(noAntes) < 3 * mapa->metersPerPixel());
    mapa->resize(560, 380);   // y el de capas, a la derecha
    QCoreApplication::processEvents();
    QVERIFY(mapa->visibleNorthWest().distanceTo(noAntes) < 3 * mapa->metersPerPixel());
    mapa->resize(800, 600);   // se cierran los dos
    QCoreApplication::processEvents();
    QVERIFY(mapa->visibleNorthWest().distanceTo(noAntes) < 3 * mapa->metersPerPixel());

    const QGeoCoordinate centro = mapa->center();
    m_ventana->resize(1000, 700);   // la ventana: el mapa crece con ella
    mapa->resize(1000, 700);
    QCoreApplication::processEvents();
    QVERIFY(mapa->center().distanceTo(centro) < 3 * mapa->metersPerPixel());
}

// El panel de capas: «Mapa base» con las dos bases (una marcada) y
// «Superpuestas» con el hibrido y la capa fija; marcar casillas cambia el mapa.
void TestMapaMision::panelDeCapas()
{
    PanelCapas panel(m_mision);
    QTreeWidget *arbol = panel.arbol();
    QCOMPARE(arbol->topLevelItemCount(), 2);
    QTreeWidgetItem *bases = arbol->topLevelItem(0);
    QTreeWidgetItem *encima = arbol->topLevelItem(1);
    QCOMPARE(bases->childCount(), 2);
    QCOMPARE(encima->childCount(), 2);   // hibrido y costa

    QTreeWidgetItem *osm = nullptr;
    for (int i = 0; i < bases->childCount(); ++i)
        if (bases->child(i)->text(0) == QStringLiteral("osm"))
            osm = bases->child(i);
    QVERIFY(osm);
    QCOMPARE(osm->checkState(0), Qt::Unchecked);
    osm->setCheckState(0, Qt::Checked);
    QCOMPARE(m_mision->capaBaseId(), QStringLiteral("osm"));

    // El arbol se rehace en cola: se vuelve a buscar el hibrido despues.
    QCoreApplication::processEvents();
    QTreeWidgetItem *hibrido = arbol->topLevelItem(1)->child(0);
    QVERIFY(hibrido->text(0).startsWith(QStringLiteral("Híbrido")));
    hibrido->setCheckState(0, Qt::Checked);
    QVERIFY(m_mision->hibridoVisible());
    QCoreApplication::processEvents();
    QTreeWidgetItem *costa = arbol->topLevelItem(1)->child(1);
    QCOMPARE(costa->text(0), QStringLiteral("Costa"));
    costa->setCheckState(0, Qt::Checked);
    QVERIFY(m_mision->capasFijas().first().visible);
}

// Lo que se guarda en conf.ini y se restaura al arrancar: base, hibrido, sus
// grupos ocultos y las capas fijas visibles. Lo que el paquete ya no trae se
// ignora.
void TestMapaMision::estadoDeCapasSeRestaura()
{
    m_mision->ponerCapaBaseId(QStringLiteral("osm"));
    m_mision->ponerHibridoVisible(true);
    m_mision->ponerCapaFijaVisible(QStringLiteral("costa"), true);
    const QVariantMap estado = m_mision->estadoCapas();

    delete m_ventana;
    init();
    QCOMPARE(m_mision->capaBaseId(), QStringLiteral("satelital"));
    QVariantMap conBasura = estado;
    conBasura.insert(QStringLiteral("fijas"), QStringList{QStringLiteral("costa"), QStringLiteral("ya_no_esta")});
    m_mision->restaurarCapas(conBasura);
    QCOMPARE(m_mision->capaBaseId(), QStringLiteral("osm"));
    QVERIFY(m_mision->hibridoVisible());
    QVERIFY(m_mision->capasFijas().first().visible);
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

// Los rumbos actual y deseado miden 150 px en pantalla a cualquier zoom; la
// linea al proximo punto no se pinta si el dron no tiene punto (llega 0,0).
void TestMapaMision::rumbosDeLargoFijoEnPantalla()
{
    auto *mapa = m_mision->mapa();
    m_mision->actualizarVehiculo(kHabana, 90.0, Qt::red, QPixmap(), QString());
    for (int zoom : {12, 16}) {
        mapa->setZoom(zoom);
        m_mision->ponerRumbos(90.0, 95.0, QGeoCoordinate(23.12, -82.36), true);
        int cortas = 0;
        for (qint64 id : mapa->rangeMarks()) {
            const auto l = mapa->bearingLine(id);
            if (l->to.isValid())
                continue;
            ++cortas;
            QVERIFY(qAbs(l->lengthMeters / mapa->metersPerPixel() - 150.0) < 1.0);
        }
        QCOMPARE(cortas, 2);
    }

    // Sin plan el dron manda Cumplir_Punto = 0,0: lat 0, lon 0.
    m_mision->ponerRumbos(90.0, 95.0, QGeoCoordinate(0.0, 0.0), true);
    QCOMPARE(mapa->rangeMarks().size(), 2);
    for (qint64 id : mapa->rangeMarks())
        QVERIFY(!mapa->bearingLine(id)->to.isValid());
}

// Los rotulos que el operador arrastra (el del dron y el de la linea al proximo
// punto, «095° · 149 m») se quedan donde los deja aunque lleguen mas tramas.
void TestMapaMision::rotulosArrastradosSeQuedan()
{
    auto *mapa = m_mision->mapa();
    m_mision->actualizarVehiculo(kHabana, 90.0, Qt::red, QPixmap(), "V: 10");
    m_mision->ponerRumbos(90.0, 95.0, QGeoCoordinate(23.12, -82.36), true);
    qint64 lineaProximo = -1;
    for (qint64 id : mapa->rangeMarks()) {
        const auto l = mapa->bearingLine(id);
        if (l && l->to.isValid())
            lineaProximo = id;
    }
    QVERIFY(lineaProximo >= 0);

    // Lo que hace el raton al arrastrar: cambiar el desplazamiento.
    QVERIFY(mapa->setBearingLabelOffset(lineaProximo, QPointF(40, -60)));
    QVERIFY(mapa->setTargetLabelOffset(mapa->targets().first().id, QPointF(-30, 25)));

    // Mas tramas.
    for (int i = 0; i < 3; ++i) {
        m_mision->actualizarVehiculo(QGeoCoordinate(23.10 + i * 0.001, -82.40), 90.0, Qt::red,
                                     QPixmap(), "V: 11");
        m_mision->ponerRumbos(90.0, 95.0, QGeoCoordinate(23.12, -82.36), true);
    }
    QCOMPARE(mapa->bearingLine(lineaProximo)->labelOffsetPx, QPointF(40, -60));
    QCOMPARE(mapa->targets().first().labelOffsetPx, QPointF(-30, 25));
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
