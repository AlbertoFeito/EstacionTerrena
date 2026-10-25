// Estado del enlace (MonitorEnlace) y panel de vuelo (PanelVuelo).
//
// El monitor recibe la hora a mano, asi que se prueban los cortes de 1 s
// (lento) y 3 s (perdido) sin esperar. Del panel, el texto y el color de alerta
// de cada campo.

#include "Protocolo/MonitorEnlace.h"
#include "Widgets/PanelVuelo.h"

#include <QToolButton>
#include <QtTest>

#include <algorithm>

class TestEstado : public QObject
{
    Q_OBJECT
private slots:
    void sinEnlace();
    void esperandoYPerdidoSinTramas();
    void bienLentoPerdidoYRecuperado();
    void tramasPorSegundo();
    void desconectarOlvidaLoAnterior();

    void panelVacioAlEmpezar();
    void panelConDatos();
    void coloresDeAlerta();
    void rutaCumplida();
    void avisoDeEnlacePerdido();
    void detallePlegadoYConDatos();
    void detalleDeCuadroptero();
};

// Sin enlace abierto: gris, "Sin enlace", y las tramas no cuentan.
void TestEstado::sinEnlace()
{
    MonitorEnlace m;
    m.registrarTrama(100);
    QCOMPARE(m.estado(200), MonitorEnlace::Estado::Desconectado);
    QCOMPARE(m.texto(200), QStringLiteral("Sin enlace"));
    QCOMPARE(m.msDesdeUltima(200), qint64(-1));
}

// Conectado sin tramas: esperando y, pasados 3 s, perdido.
void TestEstado::esperandoYPerdidoSinTramas()
{
    MonitorEnlace m;
    m.setConectado(true, QStringLiteral("tcp 127.0.0.1:5760"), 1000);
    QCOMPARE(m.estado(2000), MonitorEnlace::Estado::Esperando);
    QCOMPARE(m.texto(2000), QStringLiteral("tcp 127.0.0.1:5760 · esperando datos…"));
    QCOMPARE(m.estado(4500), MonitorEnlace::Estado::Perdido);
    QVERIFY(m.texto(4500).contains(QStringLiteral("SIN DATOS")));
}

// Con tramas: bien; a los 1,5 s sin ninguna, lento; a los 3,5 s, perdido; con
// otra trama, bien otra vez.
void TestEstado::bienLentoPerdidoYRecuperado()
{
    MonitorEnlace m;
    m.setConectado(true, QStringLiteral("COM5"), 0);
    for (qint64 t = 0; t <= 1000; t += 100)
        m.registrarTrama(t);
    QCOMPARE(m.estado(1100), MonitorEnlace::Estado::Bien);
    QCOMPARE(m.texto(1100), QStringLiteral("COM5 · 6 tramas/s · última hace 0,1 s"));
    QCOMPARE(m.estado(2500), MonitorEnlace::Estado::Lento);
    QCOMPARE(m.estado(4500), MonitorEnlace::Estado::Perdido);
    QCOMPARE(m.texto(6000), QStringLiteral("COM5 · SIN DATOS hace 5 s"));
    m.registrarTrama(6100);
    QCOMPARE(m.estado(6150), MonitorEnlace::Estado::Bien);
}

// Las tramas por segundo se cuentan en los ultimos 2 s.
void TestEstado::tramasPorSegundo()
{
    MonitorEnlace m;
    m.setConectado(true, QStringLiteral("udp 14551"), 0);
    for (qint64 t = 0; t < 5000; t += 100)   // 10 por segundo
        m.registrarTrama(t);
    QVERIFY(qAbs(m.tramasPorSegundo(4950) - 10.0) <= 0.5);
    for (qint64 t = 5000; t < 9000; t += 500)   // 2 por segundo
        m.registrarTrama(t);
    QVERIFY(qAbs(m.tramasPorSegundo(8950) - 2.0) <= 0.5);
}

// Al desconectar y volver a conectar se empieza de cero (esperando).
void TestEstado::desconectarOlvidaLoAnterior()
{
    MonitorEnlace m;
    m.setConectado(true, QStringLiteral("COM5"), 0);
    m.registrarTrama(100);
    m.setConectado(false, QString(), 200);
    QCOMPARE(m.estado(300), MonitorEnlace::Estado::Desconectado);
    m.setConectado(true, QStringLiteral("COM6"), 400);
    QCOMPARE(m.estado(500), MonitorEnlace::Estado::Esperando);
    QVERIFY(!m.hayTramas());
    QCOMPARE(m.descripcion(), QStringLiteral("COM6"));
}

// Recien creado: todo a guiones y en gris.
void TestEstado::panelVacioAlEmpezar()
{
    PanelVuelo p;
    for (const char *c : {"regimen", "mision", "punto", "distancia", "rumbo", "satelites"}) {
        QCOMPARE(p.valor(QString::fromLatin1(c)), QStringLiteral("—"));
        QCOMPARE(p.color(QString::fromLatin1(c)), QColor(0x9e, 0x9e, 0x9e));
    }
    QCOMPARE(p.valor(QStringLiteral("posicion")), QStringLiteral("Sin datos del dron"));
}

// Cada campo con su formato y sus unidades.
void TestEstado::panelConDatos()
{
    PanelVuelo p;
    PanelVuelo::Datos d;
    d.regimen = 2;
    d.mision = 3;
    d.puntoActual = 2;
    d.totalPuntos = 10;
    d.puntosCumplidos = 2;
    d.distanciaPunto = 163.4;
    d.rumbo = 94.6;
    d.rumboDeseado = 5.0;
    d.satelites = 12;
    d.posicion = QGeoCoordinate(23.0352, -82.4283);
    p.mostrar(d);
    QCOMPARE(p.valor(QStringLiteral("regimen")), QStringLiteral("Automático"));
    QCOMPARE(p.valor(QStringLiteral("mision")), QStringLiteral("RTL"));
    QCOMPARE(p.valor(QStringLiteral("punto")), QStringLiteral("P3 / 10"));
    QCOMPARE(p.valor(QStringLiteral("distancia")), QStringLiteral("163 m"));
    QCOMPARE(p.valor(QStringLiteral("rumbo")), QStringLiteral("095° → 005°"));
    QCOMPARE(p.valor(QStringLiteral("satelites")), QStringLiteral("12"));
    QCOMPARE(p.valor(QStringLiteral("posicion")), QStringLiteral("Dron: 23.03520, -82.42830"));

    d.distanciaPunto = 2345.0;
    d.puntoActual = -1;   // no va a ningun punto (manual, o volviendo a casa)
    p.mostrar(d);
    QCOMPARE(p.valor(QStringLiteral("distancia")), QStringLiteral("2,35 km"));
    QCOMPARE(p.valor(QStringLiteral("punto")), QStringLiteral("—"));
}

// Regimen con el color del avion; satelites en rojo (<6) o ambar (<8).
void TestEstado::coloresDeAlerta()
{
    PanelVuelo p;
    PanelVuelo::Datos d;
    const QColor rojo(0xff, 0x52, 0x52), ambar(0xff, 0xca, 0x28), verde(0x69, 0xf0, 0xae);
    d.regimen = 0;
    d.satelites = 4;
    p.mostrar(d);
    QCOMPARE(p.valor(QStringLiteral("regimen")), QStringLiteral("Manual"));
    QCOMPARE(p.color(QStringLiteral("regimen")), rojo);
    QCOMPARE(p.color(QStringLiteral("satelites")), rojo);
    d.regimen = 1;
    d.satelites = 7;
    p.mostrar(d);
    QCOMPARE(p.valor(QStringLiteral("regimen")), QStringLiteral("Estabilizado"));
    QCOMPARE(p.color(QStringLiteral("regimen")), ambar);
    QCOMPARE(p.color(QStringLiteral("satelites")), ambar);
    d.regimen = 2;
    d.satelites = 9;
    p.mostrar(d);
    QCOMPARE(p.color(QStringLiteral("regimen")), verde);
    QCOMPARE(p.color(QStringLiteral("satelites")), QColor(Qt::white));
}

// Con todos los puntos cumplidos: "✓ 10/10" en verde.
void TestEstado::rutaCumplida()
{
    PanelVuelo p;
    PanelVuelo::Datos d;
    d.regimen = 2;
    d.totalPuntos = 10;
    d.puntosCumplidos = 10;
    d.puntoActual = -1;
    p.mostrar(d);
    QCOMPARE(p.valor(QStringLiteral("punto")), QStringLiteral("✓ 10/10"));
    QCOMPARE(p.color(QStringLiteral("punto")), QColor(0x69, 0xf0, 0xae));
}

// El aviso de enlace perdido sustituye a la posicion, en rojo; al quitarlo
// vuelve la posicion, y los datos que lleguen mientras no lo pisan.
void TestEstado::avisoDeEnlacePerdido()
{
    PanelVuelo p;
    PanelVuelo::Datos d;
    d.posicion = QGeoCoordinate(23.0, -82.0);
    p.mostrar(d);
    p.setAviso(QStringLiteral("SIN DATOS DEL DRON hace 4 s"));
    QCOMPARE(p.valor(QStringLiteral("posicion")), QStringLiteral("⚠ SIN DATOS DEL DRON hace 4 s"));
    QCOMPARE(p.color(QStringLiteral("posicion")), QColor(0xff, 0x52, 0x52));
    d.posicion = QGeoCoordinate(23.1, -82.1);
    p.mostrar(d);
    QVERIFY(p.valor(QStringLiteral("posicion")).startsWith(QStringLiteral("⚠")));
    p.setAviso(QString());
    QCOMPARE(p.valor(QStringLiteral("posicion")), QStringLiteral("Dron: 23.10000, -82.10000"));
}

// El detalle tecnico nace plegado; su boton lo abre (y avisa) y los datos
// llegan aunque este plegado: ganancias "P / I" con coma, actuadores, rumbo
// de la IMU y convergencias.
void TestEstado::detallePlegadoYConDatos()
{
    PanelVuelo p;
    QVERIFY(!p.detalleVisible());
    QCOMPARE(p.valor(QStringLiteral("g_cabeceo")), QStringLiteral("—"));

    PanelVuelo::Detalle d;
    const double g[11] = {0.5, 0.1, 1.25, 0.2, 2, 0, 3, 0.3, 0.75, 4, 0.4};
    std::copy(g, g + 11, d.ganancias);
    const double a[4] = {12.5, -3, 70, 0};
    std::copy(a, a + 4, d.actuadores);
    d.rumboImu = 94.6;
    d.convergenciaBanqueo = 0.1;
    d.convergenciaCabeceo = 0.25;
    p.mostrarDetalle(d);
    QCOMPARE(p.valor(QStringLiteral("g_cabeceo")), QStringLiteral("0,50 / 0,10"));
    QCOMPARE(p.valor(QStringLiteral("g_curso")), QStringLiteral("0,75"));
    QCOMPARE(p.valor(QStringLiteral("g_rumbo")), QStringLiteral("4,00 / 0,40"));
    QCOMPARE(p.valor(QStringLiteral("act1")), QStringLiteral("-3,00"));
    QCOMPARE(p.nombreActuador(0), QStringLiteral("Estabilizadores"));
    QCOMPARE(p.valor(QStringLiteral("rumboImu")), QStringLiteral("095°"));
    QCOMPARE(p.valor(QStringLiteral("convCabeceo")), QStringLiteral("0,25"));

    QSignalSpy conmutado(&p, &PanelVuelo::detalleConmutado);
    p.setDetalleVisible(true);   // por programa: no avisa
    QVERIFY(p.detalleVisible());
    QCOMPARE(conmutado.count(), 0);
    auto *boton = p.findChild<QToolButton *>();
    QVERIFY(boton);
    boton->click();              // el operador lo pliega: avisa
    QVERIFY(!p.detalleVisible());
    QCOMPARE(conmutado.count(), 1);
    QCOMPARE(conmutado.first().first().toBool(), false);
}

// Cuadroptero: los actuadores son los cuatro motores y no hay ganancias.
void TestEstado::detalleDeCuadroptero()
{
    PanelVuelo p;
    PanelVuelo::Detalle d;
    d.tipoDron = 1;
    d.actuadores[3] = 55;
    p.mostrarDetalle(d);
    QCOMPARE(p.nombreActuador(3), QStringLiteral("Motor DT"));
    QCOMPARE(p.valor(QStringLiteral("act3")), QStringLiteral("55,00"));
    QCOMPARE(p.valor(QStringLiteral("g_altura")), QStringLiteral("— / —"));
}

QTEST_MAIN(TestEstado)
#include "test_estado.moc"
