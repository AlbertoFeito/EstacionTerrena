// Grabacion de vuelos (GrabadorVuelo).
//
// Con una base de datos SQLite temporal: el numero de vuelo se da al empezar
// (la primera muestra ya va al vuelo nuevo), las muestras se guardan sin
// redondear (la posicion con 4 decimales dejaba la traza en escalones) y se
// escriben de una vez al volcar o al terminar.

#include "Vuelos/GrabadorVuelo.h"
#include "Vuelos/ReproductorVuelo.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class TestGrabador : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void vueloNuevoAlEmpezar();
    void muestrasSinRedondear();
    void soloEscribeAlVolcarYAlTerminar();
    void dronQueNoExiste();

    void reproduceConSuPlan();
    void interpolaEntreMuestras();
    void planQueCambiaDuranteElVuelo();
    void baseDeDatosVieja();
    void reproduceATiempoRealPorFactor();

private:
    // Filas de DatosVuelos de un vuelo.
    int filas(int vuelo);
    QTemporaryDir m_dir;
    QString m_db;
};

// Base de datos con la tabla de drones y un dron con 4 vuelos ya grabados.
void TestGrabador::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = m_dir.filePath(QStringLiteral("DronDB.db"));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("prueba"));
    db.setDatabaseName(m_db);
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec("CREATE TABLE DronTable(Id_Dron INTEGER PRIMARY KEY NOT NULL UNIQUE, "
                   "Nombre TEXT, Tipo TEXT, Vuelos INTEGER DEFAULT 0)"));
    QVERIFY(q.exec("INSERT INTO DronTable (Id_Dron, Nombre, Tipo, Vuelos) VALUES (7, 'alafija', 'Ala fija', 4)"));
}

// Filas de DatosVuelos de un vuelo.
int TestGrabador::filas(int vuelo)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("prueba")));
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM DatosVuelos WHERE Id_Vuelos = ?"));
    q.addBindValue(vuelo);
    return q.exec() && q.next() ? q.value(0).toInt() : -1;
}

// Al empezar: vuelo 5 del dron 7, y DronTable ya dice 5.
void TestGrabador::vueloNuevoAlEmpezar()
{
    GrabadorVuelo g(m_db);
    QVERIFY2(g.empezar(QStringLiteral("alafija")), qPrintable(g.error()));
    QCOMPARE(g.dron(), 7);
    QCOMPARE(g.vuelo(), 5);
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("prueba")));
    QVERIFY(q.exec("SELECT Vuelos FROM DronTable WHERE Id_Dron = 7") && q.next());
    QCOMPARE(q.value(0).toInt(), 5);
    g.terminar();
}

// Lo que se anota se lee igual: posicion con todos sus decimales, cabeceo y
// banqueo cada uno en su columna, enteros y fecha con milisegundos.
void TestGrabador::muestrasSinRedondear()
{
    GrabadorVuelo g(m_db);
    QVERIFY(g.empezar(QStringLiteral("alafija")));
    MuestraVuelo m;
    m.lat = 23.0412345678;
    m.lon = -82.4276543210;
    m.cabeceo = 3.25;
    m.banqueo = -17.5;
    m.velocidadKmh = 54.0;
    m.altura = 137.75;
    m.regimen = 2;
    m.satelites = 12;
    m.ganancias[10] = 0.45;
    m.actuadores[2] = 0.83;
    m.convergenciaCabeceo = 0.06;
    m.fecha = QDateTime(QDate(2026, 10, 25), QTime(11, 5, 7, 123));
    g.anotar(m);
    g.terminar();

    QSqlQuery q(QSqlDatabase::database(QStringLiteral("prueba")));
    q.prepare(QStringLiteral("SELECT Lat, Lon, Realimentacion_Cabeceo, Realimentacion_Banqueo, "
                             "Realimentacion_Velocidad, Realimentacion_Altura, Realimentacion_Regimen, "
                             "CantidadSatelites, KIRumbo, Control_Motor, Convergencia_Cabeceo, Fecha, Dron_ID "
                             "FROM DatosVuelos WHERE Id_Vuelos = ?"));
    q.addBindValue(g.vuelo());
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toDouble(), 23.0412345678);
    QCOMPARE(q.value(1).toDouble(), -82.4276543210);
    QCOMPARE(q.value(2).toDouble(), 3.25);
    QCOMPARE(q.value(3).toDouble(), -17.5);
    QCOMPARE(q.value(4).toDouble(), 54.0);
    QCOMPARE(q.value(5).toDouble(), 137.75);
    QCOMPARE(q.value(6).toInt(), 2);
    QCOMPARE(q.value(7).toInt(), 12);
    QCOMPARE(q.value(8).toDouble(), 0.45);
    QCOMPARE(q.value(9).toDouble(), 0.83);
    QCOMPARE(q.value(10).toDouble(), 0.06);
    QCOMPARE(q.value(11).toDateTime(), m.fecha);
    QCOMPARE(q.value(12).toInt(), 7);
}

// Anotar no escribe; volcar escribe todo de una vez; terminar escribe lo que
// quede; despues de terminar no se acepta nada.
void TestGrabador::soloEscribeAlVolcarYAlTerminar()
{
    GrabadorVuelo g(m_db);
    QVERIFY(g.empezar(QStringLiteral("alafija")));
    const int vuelo = g.vuelo();
    MuestraVuelo m;
    for (int i = 0; i < 25; ++i) {
        m.lat = 23.04 + i * 1e-6;
        g.anotar(m);
    }
    QCOMPARE(g.pendientes(), 25);
    QCOMPARE(filas(vuelo), 0);
    QVERIFY2(g.volcar(), qPrintable(g.error()));
    QCOMPARE(filas(vuelo), 25);
    QCOMPARE(g.pendientes(), 0);
    for (int i = 0; i < 5; ++i)
        g.anotar(m);
    g.terminar();
    QCOMPARE(filas(vuelo), 30);
    g.anotar(m);
    QCOMPARE(g.pendientes(), 0);
    QVERIFY(!g.grabando());
}

// Un dron que no esta en DronTable: no empieza y lo dice.
void TestGrabador::dronQueNoExiste()
{
    GrabadorVuelo g(m_db);
    QVERIFY(!g.empezar(QStringLiteral("fantasma")));
    QVERIFY(g.error().contains(QStringLiteral("fantasma")));
    QVERIFY(!g.grabando());
}

// Muestra de prueba en el instante \a ms de un vuelo que empieza a las 11:00.
static MuestraVuelo muestra(qint64 ms, double lat, double curso, int regimen = 2)
{
    MuestraVuelo m;
    m.lat = lat;
    m.lon = -82.43;
    m.curso = curso;
    m.altura = 100.0 + ms / 100.0;
    m.regimen = regimen;
    m.puntoNorte = 2630000.0;
    m.puntoEste = -9176000.0;
    m.fecha = QDateTime(QDate(2026, 10, 25), QTime(11, 0)).addMSecs(ms);
    return m;
}

// Plan de prueba de \a n puntos.
static QVector<PuntoPlan> plan(int n, double desplazamiento = 0.0)
{
    QVector<PuntoPlan> p;
    for (int i = 0; i < n; ++i) {
        PuntoPlan pt;
        pt.nombre = QStringLiteral("P%1").arg(i + 1);
        pt.lat = 23.04 + desplazamiento + i * 0.001;
        pt.lon = -82.43;
        pt.altura = 150;
        pt.norte = 2630000.0 + i;
        pt.este = -9176000.0;
        p.append(pt);
    }
    return p;
}

// Lo grabado (muestras con el punto a cumplir y el plan del principio) se
// lee igual al reproducir.
void TestGrabador::reproduceConSuPlan()
{
    GrabadorVuelo g(m_db);
    QVERIFY(g.empezar(QStringLiteral("alafija")));
    g.anotarPlan(plan(10), QDateTime(QDate(2026, 10, 25), QTime(10, 59, 30)));
    for (int i = 0; i < 3; ++i)
        g.anotar(muestra(i * 100, 23.04 + i * 0.0001, 90.0));
    g.terminar();

    ReproductorVuelo r;
    QVERIFY(r.cargar(QSqlDatabase::database(QStringLiteral("prueba")), g.dron(), g.vuelo()));
    QCOMPARE(r.muestras().size(), 3);
    QCOMPARE(r.duracionMs(), qint64(200));
    QCOMPARE(r.muestras().at(2).lat, 23.0402);
    QCOMPARE(r.muestras().at(1).puntoNorte, 2630000.0);
    QCOMPARE(r.planes().size(), 1);
    QCOMPARE(r.planes().first().desdeMs, qint64(-30000));   // ya lo tenia al empezar
    QCOMPARE(r.planes().first().puntos.size(), 10);
    QCOMPARE(r.planes().first().puntos.at(9).nombre, QStringLiteral("P10"));
    QCOMPARE(r.planEn(0), 0);
}

// A mitad de camino entre dos muestras: posicion y altura a mitad, el rumbo
// por el lado corto (350 -> 10 pasa por 0) y lo discreto de la anterior. Con
// un corte de mas de 2 s no se inventa nada.
void TestGrabador::interpolaEntreMuestras()
{
    ReproductorVuelo r;
    QVector<MuestraVuelo> m = {muestra(0, 23.0400, 350.0, 1), muestra(100, 23.0402, 10.0, 2),
                               muestra(5100, 23.0500, 20.0, 2)};
    r.cargar(m, {});
    const MuestraVuelo a = r.muestraEn(50);
    QCOMPARE(a.lat, 23.0401);
    QCOMPARE(a.altura, 100.5);
    QVERIFY(qAbs(a.curso) < 1e-9 || qAbs(a.curso - 360.0) < 1e-9);
    QCOMPARE(a.regimen, 1);
    const MuestraVuelo b = r.muestraEn(2600);   // en el corte: la de antes tal cual
    QCOMPARE(b.lat, 23.0402);
    QCOMPARE(r.indiceEn(5100), 2);
    QCOMPARE(r.indiceEn(99), 0);
}

// Un plan nuevo a mitad del vuelo: antes, el primero; despues, el nuevo.
void TestGrabador::planQueCambiaDuranteElVuelo()
{
    ReproductorVuelo r;
    PlanGrabado p1{-1000, plan(10)};
    PlanGrabado p2{1500, plan(10, 0.01)};
    r.cargar({muestra(0, 23.04, 0), muestra(3000, 23.05, 0)}, {p1, p2});
    QCOMPARE(r.planEn(0), 0);
    QCOMPARE(r.planEn(1499), 0);
    QCOMPARE(r.planEn(1500), 1);

    QSignalSpy planes(&r, &ReproductorVuelo::planCambiado);
    r.irA(1000);
    r.irA(2000);
    r.irA(2500);   // mismo plan: no se vuelve a emitir
    QCOMPARE(planes.count(), 2);
    QCOMPARE(planes.last().first().value<QVector<PuntoPlan>>().first().lat, 23.05);
}

// Un vuelo grabado antes (sin columnas de punto ni tabla de planes) se
// reproduce igual, sin ruta.
void TestGrabador::baseDeDatosVieja()
{
    QTemporaryDir dir;
    const QString f = dir.filePath(QStringLiteral("vieja.db"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("vieja"));
        db.setDatabaseName(f);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE DatosVuelos(Id_Vuelos INTEGER, Dron_ID INTEGER, Lat REAL, Lon REAL, "
                       "Realimentacion_Cabeceo REAL, Realimentacion_Banqueo REAL, Realimentacion_Velocidad REAL, "
                       "Realimentacion_Altura REAL, Realimentacion_Curso REAL, Distancia_Chequeo REAL, "
                       "Curso_Deseado REAL, Realimentacion_Regimen INTEGER, Realimentacion_ComandoMision INTEGER, "
                       "Realimentacion_CursoIMU REAL, CantidadSatelites INTEGER, KPCabeceo REAL, KICabeceo REAL, "
                       "KPBanqueo REAL, KIBanqueo REAL, KPVelocidad REAL, KIVelocidad REAL, KPAltura REAL, "
                       "KIAltura REAL, KPCurso REAL, KPRumbo REAL, KIRumbo REAL, Control_Estabilizadores REAL, "
                       "Control_Alerones REAL, Control_Motor REAL, Control_Rumbo REAL, Convergencia_Banqueo REAL, "
                       "Convergencia_Cabeceo REAL, Fecha DATETIME)"));
        QVERIFY(q.exec("INSERT INTO DatosVuelos (Id_Vuelos, Dron_ID, Lat, Lon, Fecha) VALUES "
                       "(1, 3, 23.04, -82.43, '2026-10-25T10:00:00.000'), "
                       "(1, 3, 23.05, -82.43, '2026-10-25T10:00:01.000')"));
        ReproductorVuelo r;
        QVERIFY(r.cargar(db, 3, 1));
        QCOMPARE(r.muestras().size(), 2);
        QCOMPARE(r.muestras().first().puntoNorte, 0.0);
        QVERIFY(r.planes().isEmpty());
        QCOMPARE(r.planEn(500), -1);
    }
    QSqlDatabase::removeDatabase(QStringLiteral("vieja"));
}

// A x16, un vuelo de 3,2 s se reproduce en unos 200 ms, con fotogramas por
// el camino, y avisa al acabar en la ultima muestra.
void TestGrabador::reproduceATiempoRealPorFactor()
{
    ReproductorVuelo r;
    QVector<MuestraVuelo> m;
    for (int i = 0; i <= 32; ++i)
        m.append(muestra(i * 100, 23.04 + i * 0.0001, 90.0));
    r.cargar(m, {});
    r.setVelocidad(16.0);
    QSignalSpy fotogramas(&r, &ReproductorVuelo::fotograma);
    QSignalSpy fin(&r, &ReproductorVuelo::terminado);
    QElapsedTimer reloj;
    reloj.start();
    r.reproducir();
    QVERIFY(fin.wait(3000));
    QVERIFY(reloj.elapsed() < 1500);
    QVERIFY(fotogramas.count() >= 3);
    QCOMPARE(fotogramas.last().at(1).toLongLong(), qint64(3200));
    QVERIFY(!r.reproduciendo());
}

QTEST_GUILESS_MAIN(TestGrabador)
#include "test_grabador.moc"
