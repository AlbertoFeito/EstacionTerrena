#include "GrabadorVuelo.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
// Columnas de DatosVuelos despues de Id_Vuelos y Dron_ID, en el orden de
// siempre (las lee Control de objetivo por su nombre).
const char *kColumnas[] = {
    "Lat", "Lon", "Realimentacion_Cabeceo", "Realimentacion_Banqueo",
    "Realimentacion_Velocidad", "Realimentacion_Altura", "Realimentacion_Curso",
    "Distancia_Chequeo", "Curso_Deseado", "Realimentacion_Regimen",
    "Realimentacion_ComandoMision", "Realimentacion_CursoIMU", "CantidadSatelites",
    "KPCabeceo", "KICabeceo", "KPBanqueo", "KIBanqueo", "KPVelocidad", "KIVelocidad",
    "KPAltura", "KIAltura", "KPCurso", "KPRumbo", "KIRumbo",
    "Control_Estabilizadores", "Control_Alerones", "Control_Motor", "Control_Rumbo",
    "Convergencia_Banqueo", "Convergencia_Cabeceo", "Fecha",
    "Punto_Norte", "Punto_Este", "Punto_Altura"};
// Columnas que no estaban en la tabla original: se anaden a una base de datos vieja.
const char *kNuevas[] = {"Punto_Norte", "Punto_Este", "Punto_Altura"};
const char *kEnteras[] = {"Realimentacion_Regimen", "Realimentacion_ComandoMision",
                          "CantidadSatelites"};

// Conexion del hilo de escritura (se abre alli la primera vez): WAL con
// synchronous=NORMAL, una sola espera al disco por transaccion.
QSqlDatabase conexionEscritura(const QString &nombre, const QString &fichero, QString *error)
{
    if (QSqlDatabase::contains(nombre))
        return QSqlDatabase::database(nombre);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), nombre);
    db.setDatabaseName(fichero);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open()) {
        *error = db.lastError().text();
        return db;
    }
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    return db;
}

// Un lote de muestras en una transaccion con una consulta preparada.
bool insertarMuestras(QSqlDatabase db, int vuelo, int dron, const QVector<MuestraVuelo> &lote,
                      QString *error)
{
    QStringList nombres = {QStringLiteral("Id_Vuelos"), QStringLiteral("Dron_ID")};
    for (const char *c : kColumnas)
        nombres << QString::fromLatin1(c);
    QStringList marcas;
    for (int i = 0; i < nombres.size(); ++i)
        marcas << QStringLiteral("?");

    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO DatosVuelos (%1) VALUES (%2)")
                  .arg(nombres.join(QStringLiteral(", ")), marcas.join(QStringLiteral(", "))));
    for (const MuestraVuelo &m : lote) {
        q.addBindValue(vuelo);
        q.addBindValue(dron);
        for (double v : {m.lat, m.lon, m.cabeceo, m.banqueo, m.velocidadKmh, m.altura, m.curso,
                         m.distanciaChequeo, m.cursoDeseado})
            q.addBindValue(v);
        q.addBindValue(m.regimen);
        q.addBindValue(m.mision);
        q.addBindValue(m.cursoImu);
        q.addBindValue(m.satelites);
        for (double g : m.ganancias)
            q.addBindValue(g);
        for (double a : m.actuadores)
            q.addBindValue(a);
        q.addBindValue(m.convergenciaBanqueo);
        q.addBindValue(m.convergenciaCabeceo);
        q.addBindValue(m.fecha);
        q.addBindValue(m.puntoNorte);
        q.addBindValue(m.puntoEste);
        q.addBindValue(m.puntoAltura);
        if (!q.exec()) {
            *error = q.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        *error = db.lastError().text();
        return false;
    }
    return true;
}

// Un plan entero en una transaccion, con la hora a la que se confirmo.
bool insertarPlan(QSqlDatabase db, int vuelo, int dron, const QVector<PuntoPlan> &plan,
                  const QDateTime &cuando, QString *error)
{
    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO PlanesVuelo (Dron_ID, Id_Vuelos, Fecha, Indice, Nombre, "
                             "Lat, Lon, Altura, Velocidad, Norte, Este) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; i < plan.size(); ++i) {
        const PuntoPlan &p = plan.at(i);
        for (const QVariant &v : {QVariant(dron), QVariant(vuelo), QVariant(cuando), QVariant(i),
                                  QVariant(p.nombre), QVariant(p.lat), QVariant(p.lon),
                                  QVariant(p.altura), QVariant(p.velocidad), QVariant(p.norte),
                                  QVariant(p.este)})
            q.addBindValue(v);
        if (!q.exec()) {
            *error = q.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        *error = db.lastError().text();
        return false;
    }
    return true;
}
}

// Nombre de conexion propio para cada grabador y el hilo donde se escribe.
GrabadorVuelo::GrabadorVuelo(const QString &ficheroDb, QObject *parent)
    : QObject(parent), m_fichero(ficheroDb),
      m_conexion(QStringLiteral("grabador_vuelo_%1").arg(quintptr(this))),
      m_escritor(new QObject)
{
    m_hilo.setObjectName(QStringLiteral("GrabadorVuelo"));
    m_escritor->moveToThread(&m_hilo);
    m_hilo.start();
    m_reloj.setInterval(1000);
    connect(&m_reloj, &QTimer::timeout, this, [this]() { volcar(); });
}

// Escribe lo pendiente, cierra las dos conexiones (la de escritura, en su
// hilo) y para el hilo.
GrabadorVuelo::~GrabadorVuelo()
{
    terminar();
    const QString escritura = m_conexion + QStringLiteral("_escritura");
    QMetaObject::invokeMethod(m_escritor, [escritura]() {
        if (QSqlDatabase::contains(escritura)) {
            QSqlDatabase::database(escritura, false).close();
            QSqlDatabase::removeDatabase(escritura);
        }
    }, Qt::BlockingQueuedConnection);
    m_hilo.quit();
    m_hilo.wait();
    delete m_escritor;
    if (QSqlDatabase::contains(m_conexion)) {
        QSqlDatabase::database(m_conexion, false).close();
        QSqlDatabase::removeDatabase(m_conexion);
    }
}

// Un error del hilo de escritura, ya en el de la interfaz.
void GrabadorVuelo::anotarError(const QString &error)
{
    m_error = error;
}

// Abre (una vez) la conexion propia; espera hasta 5 s si otra conexion esta
// escribiendo en el mismo fichero.
bool GrabadorVuelo::abrir()
{
    QSqlDatabase db = QSqlDatabase::contains(m_conexion)
        ? QSqlDatabase::database(m_conexion, false)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_conexion);
    if (db.isOpen())
        return true;
    db.setDatabaseName(m_fichero);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open()) {
        m_error = db.lastError().text();
        return false;
    }
    return true;
}

// La tabla de siempre, mas un indice por dron y vuelo (lo que buscan las
// consultas de Control de objetivo).
bool GrabadorVuelo::asegurarTabla()
{
    if (!abrir())
        return false;
    QStringList columnas = {QStringLiteral("Id_Vuelos INTEGER NOT NULL"),
                            QStringLiteral("Dron_ID INTEGER NOT NULL REFERENCES DronTable(id_Dron) "
                                           "ON DELETE CASCADE ON UPDATE CASCADE")};
    for (const char *c : kColumnas) {
        const QString nombre = QString::fromLatin1(c);
        QString tipo = QStringLiteral("REAL");
        for (const char *e : kEnteras)
            if (nombre == QLatin1String(e))
                tipo = QStringLiteral("INTEGER");
        if (nombre == QLatin1String("Fecha"))
            tipo = QStringLiteral("DATETIME");
        columnas << nombre + QLatin1Char(' ') + tipo;
    }
    QSqlQuery q(QSqlDatabase::database(m_conexion));
    if (!q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS DatosVuelos(%1)").arg(columnas.join(QStringLiteral(", "))))
        || !q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS DatosVuelos_dron_vuelo "
                                  "ON DatosVuelos(Dron_ID, Id_Vuelos)"))) {
        m_error = q.lastError().text();
        return false;
    }
    // Una base de datos de antes no tiene las columnas del punto a cumplir.
    QStringList existentes;
    if (q.exec(QStringLiteral("PRAGMA table_info(DatosVuelos)")))
        while (q.next())
            existentes << q.value(1).toString();
    for (const char *c : kNuevas) {
        const QString nombre = QString::fromLatin1(c);
        if (!existentes.contains(nombre)
            && !q.exec(QStringLiteral("ALTER TABLE DatosVuelos ADD COLUMN %1 REAL").arg(nombre))) {
            m_error = q.lastError().text();
            return false;
        }
    }
    // Los planes que confirma el dron durante el vuelo, punto a punto.
    if (!q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS PlanesVuelo("
                               "Dron_ID INTEGER NOT NULL, Id_Vuelos INTEGER NOT NULL, "
                               "Fecha DATETIME NOT NULL, Indice INTEGER NOT NULL, Nombre TEXT, "
                               "Lat REAL, Lon REAL, Altura REAL, Velocidad REAL, "
                               "Norte REAL, Este REAL)"))
        || !q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS PlanesVuelo_dron_vuelo "
                                  "ON PlanesVuelo(Dron_ID, Id_Vuelos)"))) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

// Busca el dron, le suma un vuelo en DronTable y empieza a aceptar muestras.
// Todo en el momento: la primera muestra ya va al vuelo nuevo.
bool GrabadorVuelo::empezar(const QString &nombreDron)
{
    terminar();
    m_error.clear();
    if (!asegurarTabla())
        return false;
    QSqlDatabase db = QSqlDatabase::database(m_conexion);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT Id_Dron, Vuelos FROM DronTable WHERE Nombre = ?"));
    q.addBindValue(nombreDron);
    if (!q.exec() || !q.next()) {
        m_error = q.lastError().isValid() ? q.lastError().text()
                                          : tr("No existe el dron «%1»").arg(nombreDron);
        return false;
    }
    m_dron = q.value(0).toInt();
    m_vuelo = q.value(1).toInt() + 1;
    QSqlQuery u(db);
    u.prepare(QStringLiteral("UPDATE DronTable SET Vuelos = ? WHERE Id_Dron = ?"));
    u.addBindValue(m_vuelo);
    u.addBindValue(m_dron);
    if (!u.exec()) {
        m_error = u.lastError().text();
        return false;
    }
    m_pendientes.clear();
    m_grabando = true;
    m_reloj.start();
    return true;
}

// Solo en memoria: a 10 Hz no se toca el disco.
void GrabadorVuelo::anotar(const MuestraVuelo &m)
{
    if (!m_grabando)
        return;
    m_pendientes.append(m);
    if (!m_pendientes.last().fecha.isValid())
        m_pendientes.last().fecha = QDateTime::currentDateTime();
}

// Manda las pendientes al hilo de escritura (una transaccion por lote) y
// vuelve en el acto: el hilo de la interfaz no espera al disco.
bool GrabadorVuelo::volcar()
{
    if (m_pendientes.isEmpty())
        return true;
    QVector<MuestraVuelo> lote;
    lote.swap(m_pendientes);
    const QString nombre = m_conexion + QStringLiteral("_escritura");
    const QString fichero = m_fichero;
    const int vuelo = m_vuelo, dron = m_dron;
    QMetaObject::invokeMethod(m_escritor, [this, nombre, fichero, vuelo, dron, lote]() {
        QString error;
        QSqlDatabase db = conexionEscritura(nombre, fichero, &error);
        if (error.isEmpty())
            insertarMuestras(db, vuelo, dron, lote, &error);
        if (!error.isEmpty())
            QMetaObject::invokeMethod(this, [this, error]() { anotarError(error); }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    return true;
}

// Espera a que el hilo de escritura acabe lo que tiene encargado (va en orden).
void GrabadorVuelo::esperar()
{
    QMetaObject::invokeMethod(m_escritor, []() {}, Qt::BlockingQueuedConnection);
}

// Escribe lo que quede y deja de aceptar muestras.
void GrabadorVuelo::terminar()
{
    if (!m_grabando)
        return;
    m_reloj.stop();
    volcar();
    esperar();   // al terminar, todo en disco (la lista de vuelos lo lee enseguida)
    m_grabando = false;
}

// El plan, al hilo de escritura detras de las muestras ya mandadas.
bool GrabadorVuelo::anotarPlan(const QVector<PuntoPlan> &plan, const QDateTime &fecha)
{
    if (!m_grabando || plan.isEmpty())
        return false;
    volcar();   // lo anterior, antes
    const QDateTime cuando = fecha.isValid() ? fecha : QDateTime::currentDateTime();
    const QString nombre = m_conexion + QStringLiteral("_escritura");
    const QString fichero = m_fichero;
    const int vuelo = m_vuelo, dron = m_dron;
    QMetaObject::invokeMethod(m_escritor, [this, nombre, fichero, vuelo, dron, plan, cuando]() {
        QString error;
        QSqlDatabase db = conexionEscritura(nombre, fichero, &error);
        if (error.isEmpty())
            insertarPlan(db, vuelo, dron, plan, cuando, &error);
        if (!error.isEmpty())
            QMetaObject::invokeMethod(this, [this, error]() { anotarError(error); }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    return true;
}
