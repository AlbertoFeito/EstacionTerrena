// El simulador obedeciendo lo que manda la estacion: se le dan las mismas
// tramas que arma la estacion (plan y mandos) y se avanza la simulacion a mano,
// sin red ni reloj. Se comprueba cada mision (Puntos, Circular, Repetir, RTL),
// el regimen manual, el paracaidas, las ganancias con su "Actualizar", los
// acuses y que la telemetria devuelve el punto a cumplir tal como llego.

#include "Protocolo/ProyeccionMercator.h"
#include "Protocolo/SeguimientoCumplidos.h"
#include "Protocolo/Tramas.h"
#include "SimuladorDron.h"

#include <QSignalSpy>
#include <QtTest>

#include <cstring>

namespace {
// Ruta de tres puntos cerca de casa (23.04, -82.43), a unos 600 m entre si.
const QVector<QGeoCoordinate> kRuta = {QGeoCoordinate(23.045, -82.425),
                                       QGeoCoordinate(23.050, -82.420),
                                       QGeoCoordinate(23.045, -82.415)};

// El plan como lo arma CControladora::onEnviaPlan (metros Mercator enteros,
// norte primero; los puntos que sobran, a cero).
QByteArray tramaPlan(double altura = 150.0, double velocidad = 20.0)
{
    sPUNTOXYZ puntos[Tramas::kPuntosPlan];
    for (int i = 0; i < Tramas::kPuntosPlan; ++i) {
        puntos[i].x = 0;
        puntos[i].y = 0;
    }
    for (int i = 0; i < kRuta.size(); ++i) {
        const QPointF m = ProyeccionMercator::aMetros(kRuta.at(i));
        puntos[i].x = int(m.y());
        puntos[i].y = int(m.x());
        puntos[i].z = float(altura);
        puntos[i].velocidad = float(velocidad);
    }
    return Tramas::plan(puntos);
}

// Mandos con regimen y mision; el resto, como los deja la pantalla de Mandos.
sMANDOS mandos(int regimen, int mision)
{
    sMANDOS m;
    m.Regimen = quint8(regimen);
    m.Comando_Mision = quint8(mision);
    m.Radio_Acercamiento = 30.0f;
    return m;
}

// Lee la sTRAMA2 de una trama de ala fija.
sTRAMA2 t2De(const QByteArray &trama)
{
    sTRAMA2 t2;
    std::memcpy(&t2, trama.constData() + 3 + sizeof(sTRAMA1), sizeof(sTRAMA2));
    return t2;
}

// Avanza en pasos de 0,1 s hasta que se cumpla \a condicion o pasen \a segundos.
template <typename Condicion>
bool avanzarHasta(SimuladorDron &dron, double segundos, Condicion condicion)
{
    for (double t = 0.0; t < segundos; t += 0.1) {
        dron.avanzar(0.1);
        if (condicion())
            return true;
    }
    return false;
}
}

class TestSimulador : public QObject
{
    Q_OBJECT
private slots:
    void acusesDelPlanYLosMandos();
    void misionPuntosRecorreLaRutaUnaVez();
    void laEstacionMarcaTodosLosPuntos();
    void telemetriaDevuelveElPuntoACumplir();
    void repetirMisionVuelveAlPrimero();
    void circularSobreElPunto();
    void rtlVuelveACasaAVelocidadYAltura();
    void manualDaVueltasACasa();
    void laRutaSoloSeSigueEnAutomatico();
    void paracaidasBajaAlSuelo();
    void gananciasSoloLasMarcadas();
};

// Plan y mandos se contestan con su acuse; con la suma mal, acuse negativo y
// no se aplican.
void TestSimulador::acusesDelPlanYLosMandos()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    QSignalSpy salida(&dron, &SimuladorDron::bytesSalida);

    dron.recibir(tramaPlan());
    QCOMPARE(salida.count(), 1);
    QCOMPARE(salida.takeFirst().first().toByteArray(), QByteArray("uvw1"));
    QCOMPARE(dron.plan().size(), kRuta.size());   // los puntos a cero no cuentan
    QVERIFY(dron.plan().at(1).pos.distanceTo(kRuta.at(1)) < 2.0);

    QByteArray mala = Tramas::mandos(mandos(2, 0));
    mala[20] = char(mala[20] ^ 0x55);
    dron.recibir(mala);
    QCOMPARE(salida.takeFirst().first().toByteArray(), QByteArray("xyz0"));
    QCOMPARE(dron.regimen(), 0);

    dron.recibir(Tramas::mandos(mandos(2, 0)));
    QCOMPARE(salida.takeFirst().first().toByteArray(), QByteArray("xyz1"));
    QCOMPARE(dron.regimen(), 2);
    QCOMPARE(dron.radioAcercamiento(), 30.0);
}

// Mision Puntos: los tres puntos en orden, a la altura y velocidad del plan, y,
// al cumplir el ultimo, vuelta a casa y vueltas sobre ella (mision cumplida).
void TestSimulador::misionPuntosRecorreLaRutaUnaVez()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    QSignalSpy avisos(&dron, &SimuladorDron::aviso);
    dron.recibir(tramaPlan(150.0, 20.0));
    dron.recibir(Tramas::mandos(mandos(2, 0)));

    QVERIFY(avanzarHasta(dron, 600.0, [&] { return dron.fase() == SimuladorDron::Fase::MisionCumplida; }));
    QStringList cumplidos;
    for (const auto &a : avisos)
        if (a.first().toString().contains("cumplido"))
            cumplidos << a.first().toString();
    QCOMPARE(cumplidos, QStringList({"Punto 1 de 3 cumplido", "Punto 2 de 3 cumplido",
                                     "Punto 3 de 3 cumplido"}));
    QCOMPARE(dron.puntoActual(), 2);
    QVERIFY(qAbs(dron.altura() - 150.0) < 5.0);      // la del plan
    QVERIFY(qAbs(dron.velocidad() - 20.0) < 0.5);

    avanzarHasta(dron, 240.0, [] { return false; });
    QCOMPARE(dron.fase(), SimuladorDron::Fase::MisionCumplida);
    QVERIFY(dron.posicion().distanceTo(dron.casa()) < 450.0);   // vueltas de 300 m sobre casa
    QVERIFY(qAbs(dron.altura() - 120.0) < 5.0);      // sin altura de casa: la de siempre
    const sTRAMA2 t2 = t2De(dron.avanzar(0.1));
    const QPointF c = ProyeccionMercator::aMetros(dron.casa());
    QVERIFY(qAbs(t2.Cumplir_Punto[0] - c.y()) < 1.0);   // ya no va a ningun punto del plan
}

// Lo que hace la estacion con cada trama (SeguimientoCumplidos, como en
// CControladora) marca los TRES puntos, tambien el ultimo, que antes se
// quedaba sin marcar porque tras el no hay "siguiente".
void TestSimulador::laEstacionMarcaTodosLosPuntos()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    dron.recibir(tramaPlan());
    dron.recibir(Tramas::mandos(mandos(2, 0)));
    QList<QPointF> plan;
    for (const QGeoCoordinate &p : kRuta) {
        const QPointF m = ProyeccionMercator::aMetros(p);
        plan.append(QPoint(int(m.y()), int(m.x())));
    }
    SeguimientoCumplidos cumplidos;
    cumplidos.setPlan(plan);

    QVector<int> orden;
    for (int i = 0; i < 6000 && cumplidos.cuantos() < plan.size(); ++i) {
        const QByteArray trama = dron.avanzar(0.1);
        sTRAMA1 t1;
        std::memcpy(&t1, trama.constData() + 3, sizeof(sTRAMA1));
        const sTRAMA2 t2 = t2De(trama);
        const QPointF m = ProyeccionMercator::aMetros(
            QGeoCoordinate(t1.Realimentacion_Latitud, t1.Realimentacion_Longitud));
        orden += cumplidos.actualizar(QPointF(t2.Cumplir_Punto[0], t2.Cumplir_Punto[1]),
                                      QPointF(m.y(), m.x()), 30.0);
    }
    QCOMPARE(orden, QVector<int>({0, 1, 2}));
}

// En automatico la telemetria trae el punto a cumplir tal como llego en el
// plan (asi la estacion lo encuentra y pinta los cumplidos) y el regimen.
void TestSimulador::telemetriaDevuelveElPuntoACumplir()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    dron.recibir(tramaPlan());
    dron.recibir(Tramas::mandos(mandos(2, 0)));
    const sTRAMA2 t2 = t2De(dron.avanzar(0.1));
    const QPointF m = ProyeccionMercator::aMetros(kRuta.first());
    QCOMPARE(t2.Cumplir_Punto[0], float(int(m.y())));
    QCOMPARE(t2.Cumplir_Punto[1], float(int(m.x())));
    QCOMPARE(int(t2.Realimentacion_Regimen), 2);
    QCOMPARE(int(t2.Realimentacion_ComendoMision), 0);
}

// Repetir mision: tras el ultimo punto vuelve al primero, sin terminar.
void TestSimulador::repetirMisionVuelveAlPrimero()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    QSignalSpy avisos(&dron, &SimuladorDron::aviso);
    dron.recibir(tramaPlan());
    dron.recibir(Tramas::mandos(mandos(2, 2)));
    bool vioElTercero = false;
    QVERIFY(avanzarHasta(dron, 600.0, [&] {
        if (dron.puntoActual() == 2)
            vioElTercero = true;
        return vioElTercero && dron.puntoActual() == 0;
    }));
    QCOMPARE(dron.fase(), SimuladorDron::Fase::Ruta);
    bool repite = false;
    for (const auto &a : avisos)
        repite = repite || a.first().toString().contains("se repite");
    QVERIFY(repite);
}

// Circular: vueltas alrededor del punto al que iba (el primero).
void TestSimulador::circularSobreElPunto()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    dron.recibir(tramaPlan());
    dron.recibir(Tramas::mandos(mandos(2, 1)));
    avanzarHasta(dron, 240.0, [] { return false; });
    QCOMPARE(dron.fase(), SimuladorDron::Fase::Circular);
    QCOMPARE(dron.puntoActual(), 0);
    double minimo = 1e9, maximo = 0.0;
    for (int i = 0; i < 600; ++i) {
        dron.avanzar(0.1);
        const double d = dron.posicion().distanceTo(kRuta.first());
        minimo = qMin(minimo, d);
        maximo = qMax(maximo, d);
    }
    QVERIFY2(minimo > 200.0 && maximo < 400.0,
             qPrintable(QString("%1 .. %2").arg(minimo).arg(maximo)));
}

// RTL: vuelve a la casa de los mandos a V.RTH y a la altura de casa.
void TestSimulador::rtlVuelveACasaAVelocidadYAltura()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    dron.recibir(tramaPlan());
    dron.recibir(Tramas::mandos(mandos(2, 0)));
    avanzarHasta(dron, 60.0, [] { return false; });

    const QGeoCoordinate casa(23.060, -82.400);
    const QPointF c = ProyeccionMercator::aMetros(casa);
    sMANDOS m = mandos(2, 3);
    m.Home[0] = float(c.y());
    m.Home[1] = float(c.x());
    m.Home[2] = 80.0f;
    m.Velocidad_RTH = 25.0f;
    dron.recibir(Tramas::mandos(m));
    QVERIFY(dron.casa().distanceTo(casa) < 2.0);
    QCOMPARE(dron.fase(), SimuladorDron::Fase::Ruta);   // cambia en el siguiente paso
    dron.avanzar(0.1);
    QCOMPARE(dron.fase(), SimuladorDron::Fase::Regreso);

    QVERIFY(avanzarHasta(dron, 600.0, [&] { return dron.posicion().distanceTo(casa) < 400.0; }));
    avanzarHasta(dron, 60.0, [] { return false; });
    QVERIFY(dron.posicion().distanceTo(casa) < 450.0);
    QVERIFY(qAbs(dron.velocidad() - 25.0) < 0.5);
    QVERIFY(qAbs(dron.altura() - 80.0) < 5.0);
}

// Manual (o estabilizado): sin mando de radio, vueltas a casa aunque haya plan.
void TestSimulador::manualDaVueltasACasa()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    dron.recibir(tramaPlan());
    dron.recibir(Tramas::mandos(mandos(0, 0)));
    avanzarHasta(dron, 120.0, [] { return false; });
    QCOMPARE(dron.fase(), SimuladorDron::Fase::VueltasCasa);
    QVERIFY(dron.posicion().distanceTo(dron.casa()) < 450.0);
    const sTRAMA2 t2 = t2De(dron.avanzar(0.1));
    QCOMPARE(t2.Cumplir_Punto[0], 0.0f);   // sin punto a cumplir
}

// Primero la ruta y luego el regimen: con el plan en memoria, en manual y en
// estabilizado sigue dando vueltas a casa sin acercarse a la ruta; solo al
// pasar a automatico va al primer punto.
void TestSimulador::laRutaSoloSeSigueEnAutomatico()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    dron.recibir(tramaPlan());
    QCOMPARE(dron.plan().size(), kRuta.size());
    const QGeoCoordinate primero = dron.plan().first().pos;
    const double lejosAlEmpezar = dron.posicion().distanceTo(primero);

    for (int regimen : {0, 1}) {
        dron.recibir(Tramas::mandos(mandos(regimen, 0)));
        avanzarHasta(dron, 120.0, [] { return false; });
        QCOMPARE(dron.fase(), SimuladorDron::Fase::VueltasCasa);
        QVERIFY(dron.posicion().distanceTo(dron.casa()) < 450.0);
        QVERIFY(dron.posicion().distanceTo(primero) > lejosAlEmpezar - 450.0);
        QCOMPARE(t2De(dron.avanzar(0.1)).Cumplir_Punto[0], 0.0f);
    }

    dron.recibir(Tramas::mandos(mandos(2, 0)));
    dron.avanzar(0.1);
    QCOMPARE(dron.fase(), SimuladorDron::Fase::Ruta);
    QVERIFY(avanzarHasta(dron, 600.0, [&] { return dron.puntoActual() >= 1; }));
}

// Paracaidas: se para y baja hasta el suelo.
void TestSimulador::paracaidasBajaAlSuelo()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    sMANDOS m = mandos(2, 0);
    m.Paracaidas = 1;
    dron.recibir(Tramas::mandos(m));
    QVERIFY(avanzarHasta(dron, 120.0, [&] { return dron.altura() <= 0.0; }));
    QCOMPARE(dron.fase(), SimuladorDron::Fase::Paracaidas);
    avanzarHasta(dron, 10.0, [] { return false; });
    QVERIFY(dron.velocidad() < 0.1);
    QCOMPARE(dron.altura(), 0.0);
}

// Ganancias: solo cambian las que llegan con "Actualizar"; se devuelven en la
// telemetria.
void TestSimulador::gananciasSoloLasMarcadas()
{
    SimuladorDron dron(SimuladorDron::Opciones{});
    sMANDOS m = mandos(0, 0);
    m.Ganancias_Cabeceo_Manual1 = 1.25f;
    m.Ganancias_Cabeceo_Manual2 = 0.5f;
    m.Actualizar_Ganancias_Cabeceo = 1;
    m.Ganancias_Altura_Manual1 = 9.0f;   // sin Actualizar: no cuenta
    dron.recibir(Tramas::mandos(m));
    sTRAMA2 t2 = t2De(dron.avanzar(0.1));
    QCOMPARE(t2.Ganancia_Cabeceo[0], 1.25f);
    QCOMPARE(t2.Ganancia_Cabeceo[1], 0.5f);
    QCOMPARE(t2.Ganancia_Altura[0], 0.0f);

    m.Actualizar_Ganancias_Cabeceo = 0;
    m.Ganancias_Cabeceo_Manual1 = 7.0f;   // sin Actualizar: se queda la anterior
    m.Actualizar_Ganancias_Altura = 1;
    dron.recibir(Tramas::mandos(m));
    t2 = t2De(dron.avanzar(0.1));
    QCOMPARE(t2.Ganancia_Cabeceo[0], 1.25f);
    QCOMPARE(t2.Ganancia_Altura[0], 9.0f);
}

QTEST_GUILESS_MAIN(TestSimulador)
#include "test_simulador.moc"
