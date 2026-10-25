// Pestaña de Mandos (Widget_Mandos).
//
// Lo que dice la telemetria se ve en la pestaña y, en la primera trama tras
// conectar, la pestaña lo adopta: si el dron se quedo en automatico de antes,
// la pestaña no puede decir "Manual". Ademas, la trama que manda y V.RTH, que
// crecia en cada arranque.

#include "Protocolo/Tramas.h"
#include "Widgets/widget_mandos.h"

#include <QAbstractButton>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include <cstring>

class TestMandos : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void adoptaElEstadoDelDronAlConectar();
    void avisaSiLoElegidoNoEsLoDelDron();
    void tramaConLoElegido();
    void velocidadRthNoCreceAlArrancar();

private:
    QTemporaryDir m_dir;
};

// conf.ini se escribe en la carpeta de trabajo: una temporal.
void TestMandos::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QDir::setCurrent(m_dir.path());
}

// Primera trama tras conectar: la pestaña pasa a lo del dron (automatico, RTL).
void TestMandos::adoptaElEstadoDelDronAlConectar()
{
    sMANDOS m;
    Widget_Mandos w(&m);
    QCOMPARE(w.regimenElegido(), 0);
    QCOMPARE(w.textoEstadoDron(), QStringLiteral("Dron: sin datos"));
    w.setEstadoDron(2, 3, true);
    QCOMPARE(w.regimenElegido(), 2);
    QCOMPARE(w.misionElegida(), 3);
    QCOMPARE(w.textoEstadoDron(), QStringLiteral("Dron: Automático · RTL"));
    QVERIFY(w.findChild<QAbstractButton *>(QStringLiteral("rB_A"))->isChecked());
    w.olvidarEstadoDron();
    QCOMPARE(w.textoEstadoDron(), QStringLiteral("Dron: sin datos"));
}

// Las demas tramas no tocan lo elegido; si no coincide, la etiqueta lo marca.
void TestMandos::avisaSiLoElegidoNoEsLoDelDron()
{
    sMANDOS m;
    Widget_Mandos w(&m);
    auto *etiqueta = w.findChild<QWidget *>(QStringLiteral("lEstadoDron"));
    w.setEstadoDron(0, 0, false);
    QVERIFY(!etiqueta->property("distinto").toBool());
    w.findChild<QAbstractButton *>(QStringLiteral("rB_A"))->click();   // el operador elige automatico
    w.setEstadoDron(0, 0, false);                                     // el dron sigue en manual
    QCOMPARE(w.regimenElegido(), 2);
    QVERIFY(etiqueta->property("distinto").toBool());
}

// Enviar arma "xyz" + sMANDOS + suma con lo elegido (antes copiaba los bytes
// del puntero).
void TestMandos::tramaConLoElegido()
{
    sMANDOS m;
    Widget_Mandos w(&m);
    QSignalSpy enviados(&w, &Widget_Mandos::enviaMandosDron);
    w.findChild<QAbstractButton *>(QStringLiteral("rB_SA"))->click();
    w.findChild<QAbstractButton *>(QStringLiteral("rB_Circular"))->click();
    w.findChild<QAbstractButton *>(QStringLiteral("pB_Enviar"))->click();
    QCOMPARE(enviados.count(), 1);
    const QByteArray trama = enviados.first().first().toByteArray();
    QCOMPARE(trama, Tramas::mandos(m));
    sMANDOS leido;
    std::memcpy(&leido, trama.constData() + 3, sizeof(sMANDOS));
    QCOMPARE(int(leido.Regimen), 1);
    QCOMPARE(int(leido.Comando_Mision), 1);
}

// V.RTH guardada y vuelta a leer en otro arranque: la misma (antes x3,6).
void TestMandos::velocidadRthNoCreceAlArrancar()
{
    sMANDOS m;
    {
        Widget_Mandos w(&m);
        w.findChild<QDoubleSpinBox *>(QStringLiteral("dsB_Velocidad_RTH"))->setValue(72.0);
        w.findChild<QAbstractButton *>(QStringLiteral("pB_Enviar"))->click();
        QCOMPARE(double(m.Velocidad_RTH), double(float(72.0 / 3.6)));
    }
    Widget_Mandos otra(&m);
    QCOMPARE(otra.findChild<QDoubleSpinBox *>(QStringLiteral("dsB_Velocidad_RTH"))->value(), 72.0);
}

QTEST_MAIN(TestMandos)
#include "test_mandos.moc"
