// Pruebas del protocolo (Protocolo/Tramas): los bytes tienen que ser EXACTOS,
// porque los manda y los lee el autopiloto de otra persona.
//
// La disposicion de cada campo se comprueba armando las tramas a mano con
// QDataStream (little endian, campo a campo, en el orden del protocolo), sin
// usar las estructuras: si alguien reordena un campo o cambia el empaquetado,
// falla. Ademas: la suma de 16 bits, lo que hacia la estacion antes (mandos y
// plan armados a mano) y la lectura a trozos, con basura, y de varias tramas
// seguidas.

#include "Protocolo/Tramas.h"

#include <QDataStream>
#include <QtTest>

#include <cstring>

namespace {
// Escritor little endian con floats de 32 bits, como el autopiloto.
struct Escritor
{
    QByteArray bytes;
    QDataStream s{&bytes, QIODevice::WriteOnly};
    Escritor()
    {
        s.setByteOrder(QDataStream::LittleEndian);
        s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    }
    Escritor &f(float v) { s << v; return *this; }
    Escritor &c(quint8 v) { s << v; return *this; }
    Escritor &txt(const char *t) { s.writeRawData(t, 3); return *this; }
};

// Mandos con un valor distinto en cada campo, para pillar campos cruzados.
sMANDOS mandosDePrueba()
{
    sMANDOS m;
    m.Sintonizacion_Cabeceo = 1;
    m.Sintonizacion_Banqueo = 0;
    m.Ganancias_Cabeceo_Manual1 = 1.5f;
    m.Ganancias_Cabeceo_Manual2 = 2.5f;
    m.Actualizar_Ganancias_Cabeceo = 1;
    m.Ganancias_Banqueo_Manual1 = 3.5f;
    m.Ganancias_Banqueo_Manual2 = 4.5f;
    m.Actualizar_Ganancias_Banqueo = 0;
    m.Ganancias_Altura_Manual1 = 5.5f;
    m.Ganancias_Altura_Manual2 = 6.5f;
    m.Actualizar_Ganancias_Altura = 1;
    m.Ganancias_Velocidad_Manual1 = 7.5f;
    m.Ganancias_Velocidad_Manual2 = 8.5f;
    m.Actualizar_Ganancias_Velocidad = 0;
    m.Ganancias_Curso_Manual = 9.5f;
    m.Actualizar_Ganancias_Curso = 1;
    m.Ganancias_Rumbo_Manual1 = 10.5f;
    m.Ganancias_Rumbo_Manual2 = 11.5f;
    m.Actualizar_Ganancias_Rumbo = 0;
    m.Regimen = 2;
    m.Comando_Mision = 3;
    m.Radio_Acercamiento = 35.0f;
    m.Distancia_Encuentro = 40.0f;
    m.Home[0] = 2647747.0f;
    m.Home[1] = -9170499.0f;
    m.Home[2] = 80.0f;
    m.Velocidad_RTH = 12.0f;
    m.Paracaidas = 1;
    return m;
}

// Telemetria de ala fija con valores reconocibles.
void telemetriaDePrueba(sTRAMA1 &t1, sTRAMA2 &t2)
{
    t1.Realimentacion_Cabeceo = 1.0f;
    t1.Realimentacion_Banqueo = -2.0f;
    t1.Realimentacion_Velocidad = 15.0f;
    t1.Realimentacion_Altura = 120.0f;
    t1.Realimentacion_Curso = 45.0f;
    t1.Realimentacion_Latitud = 23.1f;
    t1.Realimentacion_Longitud = -82.4f;
    t1.Realimentacion_CursoIMO = 44.0f;
    t2.Distancia_Chequeo = 250.0f;
    t2.Curso_Deseado = 60.0f;
    t2.Realimentacion_Regimen = 2;
    t2.Cumplir_Punto[0] = 2647747.0f;
    t2.Cumplir_Punto[1] = -9170499.0f;
    t2.Cumplir_Punto[2] = 100.0f;
    t2.Cantidad_Satelites = 11;
}
}

class TestTramas : public QObject
{
    Q_OBJECT
private slots:
    void sumaDe16Bits();
    void mandosCampoACampo();
    void mandosComoLosArmabaLaEstacion();
    void planCampoACampo();
    void constanteDeAlaFija();
    void cuadropteroEmpiezaPorAbc();
    void leerTramaAcuses();
    void leerByteAByte();
    void basuraEntreTramas();
    void unaTramaAMediasNoBloquea();
    void cuadroptero();
    void cuadropteroIgnoraAcuses();
    void estacionComprobandoLaSuma();
};

// Suma de 16 bits sin signo, con desbordamiento.
void TestTramas::sumaDe16Bits()
{
    QCOMPARE(Tramas::suma(QByteArray("abc")), quint16(0x61 + 0x62 + 0x63));
    QCOMPARE(Tramas::suma(QByteArray(300, char(0xFF))), quint16((300 * 255) & 0xFFFF));
}

// "xyz" + sMANDOS campo a campo + suma (byte bajo primero).
void TestTramas::mandosCampoACampo()
{
    const sMANDOS m = mandosDePrueba();
    Escritor e;
    e.txt("xyz").c(1).c(0)
        .f(1.5f).f(2.5f).c(1)
        .f(3.5f).f(4.5f).c(0)
        .f(5.5f).f(6.5f).c(1)
        .f(7.5f).f(8.5f).c(0)
        .f(9.5f).c(1)
        .f(10.5f).f(11.5f).c(0)
        .c(2).c(3)
        .f(35.0f).f(40.0f)
        .f(2647747.0f).f(-9170499.0f).f(80.0f)
        .f(12.0f).c(1);
    const quint16 s = Tramas::suma(e.bytes);
    e.c(quint8(s & 0xFF)).c(quint8(s >> 8));

    QCOMPARE(e.bytes.size(), 3 + 79 + 2);
    QCOMPARE(Tramas::mandos(m), e.bytes);
}

// Lo que armaba CControladora a mano antes de Tramas::mandos: mismos bytes.
void TestTramas::mandosComoLosArmabaLaEstacion()
{
    sMANDOS mandos = mandosDePrueba();
    QByteArray sendMando;
    sendMando.append ("xyz");
    sendMando.append (reinterpret_cast<const char*>(&mandos),sizeof (sMANDOS));
    quint16 checksum = 0;
    for (char byte : sendMando)
        checksum += static_cast<quint8>(byte);
    sendMando.append (static_cast<char>(checksum & 0xFF));
    sendMando.append (static_cast<char>(checksum >>8));

    QCOMPARE(Tramas::mandos(mandos), sendMando);
}

// "uvw" + 10 puntos (modo, x, y, z, 3 parametros, velocidad) + suma.
void TestTramas::planCampoACampo()
{
    sPUNTOXYZ puntos[Tramas::kPuntosPlan];
    Escritor e;
    e.txt("uvw");
    for (int i = 0; i < Tramas::kPuntosPlan; ++i) {
        puntos[i].modo = char(i % 5);
        puntos[i].x = 2647000.0f + i;
        puntos[i].y = -9170000.0f - i;
        puntos[i].z = 100.0f + i;
        puntos[i].parametro_00 = 0.25f;
        puntos[i].parametro_01 = 0.5f;
        puntos[i].parametro_02 = 0.75f;
        puntos[i].velocidad = 10.0f + i;
        e.c(quint8(i % 5)).f(2647000.0f + i).f(-9170000.0f - i).f(100.0f + i)
            .f(0.25f).f(0.5f).f(0.75f).f(10.0f + i);
    }
    const quint16 s = Tramas::suma(e.bytes);
    e.c(quint8(s & 0xFF)).c(quint8(s >> 8));

    QCOMPARE(e.bytes.size(), 3 + 10 * 29 + 2);
    QCOMPARE(Tramas::plan(puntos), e.bytes);
}

// "abc" + sTRAMA1 + sTRAMA2 campo a campo, sin suma.
void TestTramas::constanteDeAlaFija()
{
    sTRAMA1 t1;
    sTRAMA2 t2;
    telemetriaDePrueba(t1, t2);
    Escritor e;
    e.txt("abc").f(1.0f).f(-2.0f).f(15.0f).f(120.0f).f(45.0f).f(23.1f).f(-82.4f).f(44.0f);
    e.f(0).f(0).f(0).f(0).f(0).f(0)            // controles y convergencias
        .f(250.0f).f(60.0f)                    // distancia de chequeo, curso deseado
        .f(0).f(0).f(0).f(0).f(0).f(0).f(0).f(0)   // ganancias cab, ban, vel, alt
        .f(0).f(0).f(0)                        // curso, rumbo[2]
        .c(2).c(0)                             // regimen, comando
        .f(2647747.0f).f(-9170499.0f).f(100.0f)
        .c(11);
    QCOMPARE(e.bytes.size(), 3 + 32 + 91);
    QCOMPARE(Tramas::constante(t1, t2), e.bytes);

    Tramas::DecodificadorDron d;
    d.agregar(e.bytes);
    const auto ev = d.eventos();
    QCOMPARE(ev.size(), 1);
    QCOMPARE(ev.first().tipo, Tramas::DecodificadorDron::Tipo::Constante);
    QCOMPARE(ev.first().t1.Realimentacion_Latitud, 23.1f);
    QCOMPARE(ev.first().t1.Realimentacion_Curso, 45.0f);
    QCOMPARE(ev.first().t2.Curso_Deseado, 60.0f);
    QCOMPARE(int(ev.first().t2.Realimentacion_Regimen), 2);
    QCOMPARE(ev.first().t2.Cumplir_Punto[1], -9170499.0f);
    QCOMPARE(int(ev.first().t2.Cantidad_Satelites), 11);
}

// La trama del cuadroptero es la estructura entera, que empieza por "abc".
void TestTramas::cuadropteroEmpiezaPorAbc()
{
    sCUADROPTEROS c;
    c.Realimentacion_Latitud = 123.0f;
    const QByteArray b = Tramas::cuadroptero(c);
    QCOMPARE(b.size(), 126);
    QCOMPARE(b.left(3), QByteArray("abc"));
    float lat = 0;
    std::memcpy(&lat, b.constData() + 3 + 5 * 4, 4);   // 6.º float tras la cabecera
    QCOMPARE(lat, 123.0f);
}

// Acuses: '1' es bien; cualquier otro, mal.
void TestTramas::leerTramaAcuses()
{
    Tramas::DecodificadorDron d;
    d.agregar(Tramas::acuse("xyz", true) + Tramas::acuse("uvw", false) + QByteArray("uvw1"));
    const auto ev = d.eventos();
    QCOMPARE(ev.size(), 3);
    QCOMPARE(ev.at(0).tipo, Tramas::DecodificadorDron::Tipo::AcuseMandos);
    QVERIFY(ev.at(0).recibido);
    QCOMPARE(ev.at(1).tipo, Tramas::DecodificadorDron::Tipo::AcusePlan);
    QVERIFY(!ev.at(1).recibido);
    QVERIFY(ev.at(2).recibido);
    QCOMPARE(d.pendientes(), 0);
}

// Por serie las tramas llegan a trozos: byte a byte sale lo mismo.
void TestTramas::leerByteAByte()
{
    sTRAMA1 t1;
    sTRAMA2 t2;
    telemetriaDePrueba(t1, t2);
    const QByteArray flujo = Tramas::constante(t1, t2) + Tramas::acuse("uvw", true)
                           + Tramas::constante(t1, t2);
    Tramas::DecodificadorDron d;
    QVector<Tramas::DecodificadorDron::Evento> ev;
    for (char c : flujo) {
        d.agregar(QByteArray(1, c));
        ev += d.eventos();
    }
    QCOMPARE(ev.size(), 3);
    QCOMPARE(ev.at(0).tipo, Tramas::DecodificadorDron::Tipo::Constante);
    QCOMPARE(ev.at(1).tipo, Tramas::DecodificadorDron::Tipo::AcusePlan);
    QCOMPARE(ev.at(2).t1.Realimentacion_Altura, 120.0f);
    QCOMPARE(d.pendientes(), 0);
}

// Basura antes, entre y despues de las tramas: se salta.
void TestTramas::basuraEntreTramas()
{
    sTRAMA1 t1;
    sTRAMA2 t2;
    telemetriaDePrueba(t1, t2);
    Tramas::DecodificadorDron d;
    d.agregar(QByteArray("\x01\x02ruido") + Tramas::constante(t1, t2) + QByteArray("zz")
              + Tramas::acuse("xyz", true) + QByteArray("\xFF\xFE"));
    const auto ev = d.eventos();
    QCOMPARE(ev.size(), 2);
    QCOMPARE(ev.at(0).t1.Realimentacion_Longitud, -82.4f);
    QCOMPARE(ev.at(1).tipo, Tramas::DecodificadorDron::Tipo::AcuseMandos);
    QVERIFY(d.pendientes() <= 2);
}

// Una trama a medias espera al resto sin perder nada (el antiguo
// SerialManager se quedaba dando vueltas en ese caso).
void TestTramas::unaTramaAMediasNoBloquea()
{
    sTRAMA1 t1;
    sTRAMA2 t2;
    telemetriaDePrueba(t1, t2);
    const QByteArray t = Tramas::constante(t1, t2);
    Tramas::DecodificadorDron d;
    d.agregar(t.left(50));
    QVERIFY(d.eventos().isEmpty());
    QCOMPARE(d.pendientes(), 50);
    d.agregar(t.mid(50));
    QCOMPARE(d.eventos().size(), 1);

    // Y si solo llega basura, no crece sin limite.
    d.agregar(QByteArray(10000, 'q'));
    QVERIFY(d.pendientes() <= 2);
}

// Con cuadroptero, "abc" es el principio de la estructura (126 bytes).
void TestTramas::cuadroptero()
{
    sCUADROPTEROS c;
    c.Realimentacion_Latitud = -1234.5f;
    c.Realimentacion_Longitud = 6789.25f;
    c.Realimentacion_Regimen = 2;
    c.Cumplir_Punto[0] = 10.0f;
    c.Cantidad_Satelites = 9;
    Tramas::DecodificadorDron d;
    d.setTipoDron(1);
    const QByteArray b = QByteArray("ruido") + Tramas::cuadroptero(c);
    for (int i = 0; i < b.size(); i += 7)
        d.agregar(b.mid(i, 7));
    const auto ev = d.eventos();
    QCOMPARE(ev.size(), 1);
    QCOMPARE(ev.first().tipo, Tramas::DecodificadorDron::Tipo::Cuadroptero);
    QCOMPARE(ev.first().cuadro.Realimentacion_Latitud, -1234.5f);
    QCOMPARE(ev.first().cuadro.Realimentacion_Longitud, 6789.25f);
    QCOMPARE(int(ev.first().cuadro.Realimentacion_Regimen), 2);
    QCOMPARE(ev.first().cuadro.Cumplir_Punto[0], 10.0f);
    QCOMPARE(int(ev.first().cuadro.Cantidad_Satelites), 9);
}

// En modo cuadroptero un "xyz" suelto no atasca la lectura (antes si).
void TestTramas::cuadropteroIgnoraAcuses()
{
    sCUADROPTEROS c;
    c.Realimentacion_Altura = 50.0f;
    Tramas::DecodificadorDron d;
    d.setTipoDron(1);
    d.agregar(Tramas::acuse("xyz", true) + Tramas::cuadroptero(c));
    const auto ev = d.eventos();
    QCOMPARE(ev.size(), 1);
    QCOMPARE(ev.first().cuadro.Realimentacion_Altura, 50.0f);
}

// Lo que lee el simulador: mandos y plan, con la suma comprobada.
void TestTramas::estacionComprobandoLaSuma()
{
    const sMANDOS m = mandosDePrueba();
    sPUNTOXYZ puntos[Tramas::kPuntosPlan];
    puntos[3].x = 2647700.0f;
    QByteArray mala = Tramas::mandos(m);
    mala[10] = char(mala[10] + 1);   // un byte cambiado: la suma ya no cuadra

    Tramas::DecodificadorEstacion d;
    d.agregar(QByteArray("..") + Tramas::mandos(m) + Tramas::plan(puntos) + mala);
    const auto ev = d.eventos();
    QCOMPARE(ev.size(), 3);
    QCOMPARE(ev.at(0).tipo, Tramas::DecodificadorEstacion::Tipo::Mandos);
    QVERIFY(ev.at(0).sumaCorrecta);
    QCOMPARE(int(ev.at(0).mandos.Regimen), 2);
    QCOMPARE(ev.at(0).mandos.Radio_Acercamiento, 35.0f);
    QCOMPARE(ev.at(1).tipo, Tramas::DecodificadorEstacion::Tipo::Plan);
    QVERIFY(ev.at(1).sumaCorrecta);
    QCOMPARE(ev.at(1).puntos[3].x, 2647700.0f);
    QVERIFY(!ev.at(2).sumaCorrecta);
}

QTEST_APPLESS_MAIN(TestTramas)
#include "test_tramas.moc"
