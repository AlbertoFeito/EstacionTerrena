// La estacion y el simulador hablando de verdad: SerialManager (la estacion)
// contra SimuladorDron + EnlacesSimulador por TCP, UDP y, si hay socat (Linux),
// por un par de puertos serie virtuales. Se comprueba que llega la telemetria,
// que el plan y los mandos se contestan con su acuse y que el simulador los
// aplica, y que la posicion del cuadroptero se reconstruye como en
// CControladora.

#include "EnlacesSimulador.h"
#include "Protocolo/ProyeccionMercator.h"
#include "Protocolo/Tramas.h"
#include "SerialPortComboBox/SerialManager.h"
#include "SimuladorDron.h"

#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QtTest>

namespace {
// Tres puntos de ruta y su plan como lo arma CControladora::onEnviaPlan:
// metros Mercator truncados a enteros, norte en x y este en y.
const QVector<QGeoCoordinate> kRuta = {QGeoCoordinate(23.045, -82.425),
                                       QGeoCoordinate(23.050, -82.420),
                                       QGeoCoordinate(23.045, -82.415)};

void armarPlan(sPUNTOXYZ (&puntos)[Tramas::kPuntosPlan])
{
    for (int i = 0; i < kRuta.size(); ++i) {
        const QPointF m = ProyeccionMercator::aMetros(kRuta.at(i));
        const QPoint entero(int(m.y()), int(m.x()));
        puntos[i].x = entero.x();
        puntos[i].y = entero.y();
        puntos[i].z = 150.0f;
        puntos[i].velocidad = 20.0f;
    }
}

// Un puerto UDP libre ahora mismo.
quint16 puertoUdpLibre()
{
    QUdpSocket s;
    s.bind(QHostAddress::LocalHost, 0);
    return s.localPort();
}
}

class TestEnlace : public QObject
{
    Q_OBJECT
private slots:
    void tcpTelemetriaPlanYMandos();
    void udpContestaAQuienHabla();
    void cuadropteroPorTcp();
    void puertoSerieVirtual();

private:
    void comprobarPlanYMandos(SerialManager &estacion, SimuladorDron &dron);
};

// Manda el plan y luego los mandos (automatico); comprueba los acuses, que el
// dron los aplica y que su telemetria apunta al primer punto del plan.
void TestEnlace::comprobarPlanYMandos(SerialManager &estacion, SimuladorDron &dron)
{
    QSignalSpy acusePlan(&estacion, &SerialManager::si_AcuseReciboUVW);
    QSignalSpy acuseMandos(&estacion, &SerialManager::si_AcuseReciboXYZ);
    QSignalSpy telemetria(&estacion, &SerialManager::newTramaConstanteReceived);

    sPUNTOXYZ puntos[Tramas::kPuntosPlan];
    armarPlan(puntos);
    estacion.serieWrite(Tramas::plan(puntos));
    QTRY_COMPARE(acusePlan.count(), 1);
    QCOMPARE(dron.plan().size(), kRuta.size());
    QVERIFY(dron.plan().first().pos.distanceTo(kRuta.first()) < 2.0);

    sMANDOS m;
    m.Regimen = 2;
    m.Radio_Acercamiento = 40.0f;
    estacion.serieWrite(Tramas::mandos(m));
    QTRY_COMPARE(acuseMandos.count(), 1);
    QCOMPARE(dron.regimen(), 2);
    QCOMPARE(dron.radioAcercamiento(), 40.0);

    // En automatico, el punto a cumplir es el primero del plan, tal cual se mando.
    telemetria.clear();
    QTRY_VERIFY(telemetria.count() >= 1);
    const sTRAMA2 t2 = estacion.getS34();
    QCOMPARE(int(t2.Realimentacion_Regimen), 2);
    QCOMPARE(t2.Cumplir_Punto[0], puntos[0].x);
    QCOMPARE(t2.Cumplir_Punto[1], puntos[0].y);
}

// TCP: el simulador es servidor, la estacion cliente.
void TestEnlace::tcpTelemetriaPlanYMandos()
{
    EnlacesSimulador enlaces;
    QString error;
    QVERIFY2(enlaces.abrirTcp(0, &error), qPrintable(error));
    SimuladorDron::Opciones op;
    op.hz = 50.0;
    SimuladorDron dron(op);
    connect(&dron, &SimuladorDron::bytesSalida, &enlaces, &EnlacesSimulador::enviar);
    connect(&enlaces, &EnlacesSimulador::recibido, &dron, &SimuladorDron::recibir);

    SerialManager estacion;
    QSignalSpy conectada(&estacion, &SerialManager::connectionChanged);
    QSignalSpy telemetria(&estacion, &SerialManager::newTramaConstanteReceived);
    QVERIFY(estacion.conectarTcp(QStringLiteral("127.0.0.1"), enlaces.puertoTcp()));
    QTRY_COMPARE(conectada.count(), 1);
    QVERIFY(conectada.first().first().toBool());
    QTRY_COMPARE(enlaces.clientesTcp(), 1);

    dron.arrancar();
    QTRY_VERIFY(telemetria.count() >= 3);
    const sTRAMA1 t1 = estacion.getS28();
    const QGeoCoordinate visto(t1.Realimentacion_Latitud, t1.Realimentacion_Longitud);
    QVERIFY(visto.distanceTo(op.casa) < op.radioOrbita + 50.0);   // dando vueltas a casa

    comprobarPlanYMandos(estacion, dron);

    // Si el dron (o el puente) cierra, la estacion lo dice y queda desconectada
    // una sola vez, sin bucles.
    QSignalSpy errores(&estacion, &SerialManager::errorOccurred);
    conectada.clear();
    dron.parar();
    for (QTcpSocket *s : enlaces.findChildren<QTcpSocket *>())
        s->disconnectFromHost();   // el simulador cuelga a la estacion
    QTRY_VERIFY(!estacion.isConnected());
    QTRY_COMPARE(conectada.count(), 1);
    QVERIFY(!conectada.first().first().toBool());
    QCOMPARE(errores.count(), 1);
}

// UDP sin remoto fijo: la estacion contesta a quien le manda la telemetria.
void TestEnlace::udpContestaAQuienHabla()
{
    const quint16 puertoEstacion = puertoUdpLibre();
    EnlacesSimulador enlaces;
    QString error;
    QVERIFY2(enlaces.abrirUdp(0, QStringLiteral("127.0.0.1"), puertoEstacion, &error),
             qPrintable(error));
    SimuladorDron::Opciones op;
    op.hz = 50.0;
    SimuladorDron dron(op);
    connect(&dron, &SimuladorDron::bytesSalida, &enlaces, &EnlacesSimulador::enviar);
    connect(&enlaces, &EnlacesSimulador::recibido, &dron, &SimuladorDron::recibir);

    SerialManager estacion;
    QSignalSpy telemetria(&estacion, &SerialManager::newTramaConstanteReceived);
    QVERIFY(estacion.conectarUdp(puertoEstacion, QString(), 0));
    QVERIFY(estacion.isConnected());

    dron.arrancar();
    QTRY_VERIFY(telemetria.count() >= 3);
    comprobarPlanYMandos(estacion, dron);
}

// Cuadroptero: la estacion lee la estructura entera y reconstruye la posicion
// como CControladora::onNewCuadropteros (metros relativos + origen).
void TestEnlace::cuadropteroPorTcp()
{
    EnlacesSimulador enlaces;
    QString error;
    QVERIFY2(enlaces.abrirTcp(0, &error), qPrintable(error));
    SimuladorDron::Opciones op;
    op.tipo = SimuladorDron::Tipo::Cuadroptero;
    op.hz = 50.0;
    SimuladorDron dron(op);
    connect(&dron, &SimuladorDron::bytesSalida, &enlaces, &EnlacesSimulador::enviar);

    SerialManager estacion;
    estacion.setTipo_dron(1);
    QSignalSpy cuadro(&estacion, &SerialManager::newCuadropteros);
    QSignalSpy conectada(&estacion, &SerialManager::connectionChanged);
    QVERIFY(estacion.conectarTcp(QStringLiteral("127.0.0.1"), enlaces.puertoTcp()));
    QTRY_COMPARE(conectada.count(), 1);
    QTRY_COMPARE(enlaces.clientesTcp(), 1);

    dron.arrancar();
    QTRY_VERIFY(cuadro.count() >= 3);
    dron.parar();
    QTest::qWait(100);
    const sCUADROPTEROS c = estacion.getCuadropteros();
    const QGeoCoordinate rel = ProyeccionMercator::aGeo(QPointF(c.Realimentacion_Longitud,
                                                                c.Realimentacion_Latitud));
    const QGeoCoordinate reconstruida(rel.latitude() + 23.09759, rel.longitude() - 82.44113);
    QVERIFY2(reconstruida.distanceTo(dron.posicion()) < 1.0,
             qPrintable(QString::number(reconstruida.distanceTo(dron.posicion()))));
}

// Puerto serie: un par virtual con socat (solo donde lo haya).
void TestEnlace::puertoSerieVirtual()
{
    const QString socat = QStandardPaths::findExecutable(QStringLiteral("socat"));
    if (socat.isEmpty())
        QSKIP("Sin socat no hay puertos serie virtuales");
    QTemporaryDir dir;
    const QString a = dir.path() + "/dron", b = dir.path() + "/estacion";
    QProcess par;
    par.start(socat, {QStringLiteral("pty,raw,echo=0,link=") + a,
                      QStringLiteral("pty,raw,echo=0,link=") + b});
    QTRY_VERIFY(QFileInfo::exists(a) && QFileInfo::exists(b));

    EnlacesSimulador enlaces;
    QString error;
    QVERIFY2(enlaces.abrirSerie(a, &error), qPrintable(error));
    SimuladorDron::Opciones op;
    op.hz = 50.0;
    SimuladorDron dron(op);
    connect(&dron, &SimuladorDron::bytesSalida, &enlaces, &EnlacesSimulador::enviar);
    connect(&enlaces, &EnlacesSimulador::recibido, &dron, &SimuladorDron::recibir);

    SerialManager estacion;
    QSignalSpy telemetria(&estacion, &SerialManager::newTramaConstanteReceived);
    QVERIFY(estacion.conectar(b));
    dron.arrancar();
    QTRY_VERIFY(telemetria.count() >= 3);
    comprobarPlanYMandos(estacion, dron);
    estacion.desconectar();
    par.terminate();
    par.waitForFinished(2000);
}

QTEST_GUILESS_MAIN(TestEnlace)
#include "test_enlace.moc"
