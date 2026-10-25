#include "myqsetting.h"
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRunnable>
#include <QSettings>
#include <QThreadPool>
#include <QVariant>

namespace {
// Un solo hilo para escribir conf.ini: las escrituras van en orden y nunca en
// el hilo de la interfaz. Al salir del programa espera a que acaben.
QThreadPool &hiloEscritura()
{
    static QThreadPool pool;
    static const bool configurado = [] { pool.setMaxThreadCount(1); return true; }();
    Q_UNUSED(configurado);
    return pool;
}

// Lo ultimo mandado a escribir de cada fichero y grupo, para no repetirlo.
QHash<QString, QVariantMap> &ultimoGuardado()
{
    static QHash<QString, QVariantMap> h;
    return h;
}

// Protege ultimoGuardado().
QMutex &cerrojoUltimo()
{
    static QMutex m;
    return m;
}

// Escribe un grupo de valores en un fichero .ini (en el hilo de escritura).
class EscrituraIni : public QRunnable
{
public:
    EscrituraIni(QString fichero, QString grupo, QVariantMap valores)
        : m_fichero(std::move(fichero)), m_grupo(std::move(grupo)), m_valores(std::move(valores)) {}
    void run() override
    {
        QSettings s(m_fichero, QSettings::IniFormat);
        s.beginGroup(m_grupo);
        for (auto it = m_valores.cbegin(); it != m_valores.cend(); ++it)
            s.setValue(it.key(), it.value());
        s.endGroup();
        s.sync();
    }

private:
    QString m_fichero, m_grupo;
    QVariantMap m_valores;
};
}

MyQSetting::MyQSetting(QObject *parent) : QObject(parent)
{

}

// Un valor: lo mismo que guardarSettings con uno solo.
void MyQSetting::guardarSetting(const QString &fileName, const QString &llave, const QVariant &valor, const QString &grupo)
{
    guardarSettings(fileName, grupo, QVariantMap{{llave, valor}});
}

// Cada QSettings lee el fichero entero y al escribir crea un fichero de bloqueo,
// uno temporal y lo renombra: en Windows, con el antivirus mirando cada fichero
// nuevo, decenas o cientos de ms por valor. Antes se hacia valor a valor (4 al
// enviar mandos, 13 con las ganancias) en el hilo de la interfaz y la pantalla
// se paraba 1 o 2 s. Ahora va todo en una escritura, en otro hilo, y solo si
// algo cambio.
void MyQSetting::guardarSettings(const QString &fileName, const QString &grupo, const QVariantMap &valores)
{
    {
        QMutexLocker cerrojo(&cerrojoUltimo());
        QVariantMap &ultimo = ultimoGuardado()[fileName + QLatin1Char('|') + grupo];
        bool cambia = false;
        for (auto it = valores.cbegin(); it != valores.cend(); ++it)
            if (!ultimo.contains(it.key()) || ultimo.value(it.key()) != it.value()) {
                cambia = true;
                ultimo.insert(it.key(), it.value());
            }
        if (!cambia)
            return;
    }
    hiloEscritura().start(new EscrituraIni(fileName, grupo, valores));
}

// Lee un valor; antes espera a que se escriba lo pendiente, para no leer uno viejo.
QVariant MyQSetting::cargarSetting(const QString &fileName, const QString &llave, const QString &grupo,const QVariant &valDefecto)
{
    hiloEscritura().waitForDone();
    QVariant v;
    QSettings S(fileName,QSettings::IniFormat,nullptr);
    S.beginGroup(grupo);
    v = S.value(llave,valDefecto);
    S.endGroup();
    return v;
}
