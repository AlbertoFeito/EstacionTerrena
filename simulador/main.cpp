// simulador_dron - un dron de mentira para probar la estacion sin dron.
//
// Habla el protocolo de siempre (Principal/Protocolo/Tramas.h) por TCP, UDP y,
// si se pide, puerto serie. Sin opciones:
//   - servidor TCP en el puerto 5760         (estacion: TCP 127.0.0.1:5760)
//   - UDP: escucha en 14550, manda a 127.0.0.1:14551
//                                              (estacion: UDP escuchar 14551,
//                                               enviar a 127.0.0.1:14550)
//
//   simulador_dron                       ala fija, dando vueltas a casa
//   simulador_dron --tipo cuadro         cuadroptero
//   simulador_dron --regimen 2           automatico: sigue el plan en cuanto llega
//   simulador_dron --serie COM6          tambien por puerto serie
//   simulador_dron --casa 22.98,-82.46 --velocidad 70 --hz 20
//
// En la estacion: elegir el dron (del tipo que se simula) en Configuracion,
// el enlace (TCP o UDP) y Conectar.

#include "EnlacesSimulador.h"
#include "SimuladorDron.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTime>
#include <QTimer>

namespace {
// Escribe una linea en la consola con la hora delante.
void escribir(const QString &texto)
{
    static QTextStream out(stdout);
    out << QTime::currentTime().toString("hh:mm:ss") << "  " << texto << Qt::endl;
}

// "host:puerto" -> host y puerto. false si no tiene esa forma.
bool hostPuerto(const QString &texto, QString *host, quint16 *puerto)
{
    const int dos = texto.lastIndexOf(':');
    if (dos <= 0)
        return false;
    bool ok = false;
    const uint p = texto.mid(dos + 1).toUInt(&ok);
    if (!ok || p == 0 || p > 65535)
        return false;
    *host = texto.left(dos);
    *puerto = quint16(p);
    return true;
}
}

// Lee las opciones, abre las vias, une el simulador con ellas y escribe cada
// dos segundos como va el vuelo.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("simulador_dron");

    QCommandLineParser p;
    p.setApplicationDescription("Dron simulado para EstacionTerrena (mismo protocolo que el real).");
    p.addHelpOption();
    const QCommandLineOption oTipo("tipo", "ala (ala fija, por defecto) o cuadro (cuadróptero).", "tipo", "ala");
    const QCommandLineOption oTcp("tcp", "Puerto del servidor TCP (5760).", "puerto", "5760");
    const QCommandLineOption oSinTcp("sin-tcp", "No abrir TCP.");
    const QCommandLineOption oUdp("udp", "Puerto UDP en el que escucha (14550).", "puerto", "14550");
    const QCommandLineOption oUdpDestino("udp-destino", "A dónde manda por UDP (127.0.0.1:14551).",
                                         "host:puerto", "127.0.0.1:14551");
    const QCommandLineOption oSinUdp("sin-udp", "No abrir UDP.");
    const QCommandLineOption oSerie("serie", "Puerto serie (115200 8N1), p. ej. COM6 o /dev/ttyUSB0.", "puerto");
    const QCommandLineOption oHz("hz", "Tramas por segundo (10).", "hz", "10");
    const QCommandLineOption oCasa("casa", "Casa y punto de partida: lat,lon (23.04,-82.43).", "lat,lon",
                                   "23.04,-82.43");
    const QCommandLineOption oVel("velocidad", "Velocidad en km/h (54).", "km/h", "54");
    const QCommandLineOption oAlt("altura", "Altura en m (120).", "m", "120");
    const QCommandLineOption oRegimen("regimen", "Régimen inicial: 0 manual, 1 semi, 2 automático (0).",
                                      "0|1|2", "0");
    const QCommandLineOption oOrbita("radio-orbita", "Radio de las vueltas a casa, en m (300).", "m", "300");
    p.addOptions({oTipo, oTcp, oSinTcp, oUdp, oUdpDestino, oSinUdp, oSerie, oHz, oCasa, oVel, oAlt,
                  oRegimen, oOrbita});
    p.process(app);

    SimuladorDron::Opciones op;
    op.tipo = p.value(oTipo).startsWith("cuad") ? SimuladorDron::Tipo::Cuadroptero
                                                 : SimuladorDron::Tipo::AlaFija;
    op.hz = p.value(oHz).toDouble();
    const QStringList casa = p.value(oCasa).split(',');
    if (casa.size() == 2)
        op.casa = QGeoCoordinate(casa.at(0).toDouble(), casa.at(1).toDouble());
    if (!op.casa.isValid() || op.hz <= 0.0) {
        escribir("Opciones no válidas (mira --help)");
        return 2;
    }
    op.velocidad = p.value(oVel).toDouble() / 3.6;
    op.altura = p.value(oAlt).toDouble();
    op.regimen = qBound(0, p.value(oRegimen).toInt(), 2);
    op.radioOrbita = p.value(oOrbita).toDouble();

    EnlacesSimulador enlaces;
    QString error;
    QStringList abiertas;
    if (!p.isSet(oSinTcp)) {
        if (!enlaces.abrirTcp(quint16(p.value(oTcp).toUInt()), &error)) {
            escribir(error);
            return 1;
        }
        abiertas << QString("TCP servidor en %1").arg(enlaces.puertoTcp());
    }
    if (!p.isSet(oSinUdp)) {
        QString host;
        quint16 puerto = 0;
        if (!hostPuerto(p.value(oUdpDestino), &host, &puerto)) {
            escribir("--udp-destino tiene que ser host:puerto");
            return 2;
        }
        if (!enlaces.abrirUdp(quint16(p.value(oUdp).toUInt()), host, puerto, &error)) {
            escribir(error);
            return 1;
        }
        abiertas << QString("UDP en %1 → %2").arg(enlaces.puertoUdp()).arg(p.value(oUdpDestino));
    }
    if (p.isSet(oSerie)) {
        if (!enlaces.abrirSerie(p.value(oSerie), &error)) {
            escribir(error);
            return 1;
        }
        abiertas << QString("serie %1").arg(p.value(oSerie));
    }
    if (abiertas.isEmpty()) {
        escribir("No hay ninguna vía abierta");
        return 2;
    }

    SimuladorDron dron(op);
    QObject::connect(&dron, &SimuladorDron::bytesSalida, &enlaces, &EnlacesSimulador::enviar);
    QObject::connect(&enlaces, &EnlacesSimulador::recibido, &dron, &SimuladorDron::recibir);
    QObject::connect(&dron, &SimuladorDron::aviso, &escribir);
    QObject::connect(&enlaces, &EnlacesSimulador::aviso, &escribir);

    escribir(QString("Dron simulado: %1, %2 Hz, casa %3")
                 .arg(op.tipo == SimuladorDron::Tipo::AlaFija ? "ala fija" : "cuadróptero")
                 .arg(op.hz).arg(op.casa.toString(QGeoCoordinate::Degrees)));
    for (const QString &v : abiertas)
        escribir("  " + v);
    dron.arrancar();

    QTimer estado;
    QObject::connect(&estado, &QTimer::timeout, [&]() {
        const QGeoCoordinate pos = dron.posicion();
        QString donde = dron.regimen() == 2 && !dron.plan().isEmpty()
            ? QString("hacia el punto %1/%2").arg(dron.puntoActual() + 1).arg(dron.plan().size())
            : QString("vueltas a casa");
        escribir(QString("%1, %2  rumbo %3°  régimen %4  %5  (TCP: %6 estación/es)")
                     .arg(pos.latitude(), 0, 'f', 5).arg(pos.longitude(), 0, 'f', 5)
                     .arg(dron.rumbo(), 0, 'f', 0).arg(dron.regimen()).arg(donde)
                     .arg(enlaces.clientesTcp()));
    });
    estado.start(2000);
    return app.exec();
}
