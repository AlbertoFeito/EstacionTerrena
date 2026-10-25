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
}

// Nombre de conexion propio para cada grabador.
GrabadorVuelo::GrabadorVuelo(const QString &ficheroDb, QObject *parent)
    : QObject(parent), m_fichero(ficheroDb),
      m_conexion(QStringLiteral("grabador_vuelo_%1").arg(quintptr(this)))
{
    m_reloj.setInterval(1000);
    connect(&m_reloj, &QTimer::timeout, this, [this]() { volcar(); });
}

// Escribe lo pendiente y suelta la conexion.
GrabadorVuelo::~GrabadorVuelo()
{
    terminar();
    if (QSqlDatabase::contains(m_conexion)) {
        QSqlDatabase::database(m_conexion, false).close();
        QSqlDatabase::removeDatabase(m_conexion);
    }
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

// Todas las pendientes en una transaccion y con una sola consulta preparada.
// Si falla, se quedan para el siguiente intento.
bool GrabadorVuelo::volcar()
{
    if (m_pendientes.isEmpty())
        return true;
    if (!abrir())
        return false;
    QSqlDatabase db = QSqlDatabase::database(m_conexion);
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
    for (const MuestraVuelo &m : qAsConst(m_pendientes)) {
        q.addBindValue(m_vuelo);
        q.addBindValue(m_dron);
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
            m_error = q.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        m_error = db.lastError().text();
        return false;
    }
    m_pendientes.clear();
    return true;
}

// Escribe lo que quede y deja de aceptar muestras.
void GrabadorVuelo::terminar()
{
    if (!m_grabando)
        return;
    m_reloj.stop();
    volcar();
    m_grabando = false;
}

// El plan entero en una transaccion, con la hora a la que se confirmo.
bool GrabadorVuelo::anotarPlan(const QVector<PuntoPlan> &plan, const QDateTime &fecha)
{
    if (!m_grabando || plan.isEmpty() || !abrir())
        return false;
    const QDateTime cuando = fecha.isValid() ? fecha : QDateTime::currentDateTime();
    QSqlDatabase db = QSqlDatabase::database(m_conexion);
    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO PlanesVuelo (Dron_ID, Id_Vuelos, Fecha, Indice, Nombre, "
                             "Lat, Lon, Altura, Velocidad, Norte, Este) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (int i = 0; i < plan.size(); ++i) {
        const PuntoPlan &p = plan.at(i);
        for (const QVariant &v : {QVariant(m_dron), QVariant(m_vuelo), QVariant(cuando), QVariant(i),
                                  QVariant(p.nombre), QVariant(p.lat), QVariant(p.lon),
                                  QVariant(p.altura), QVariant(p.velocidad), QVariant(p.norte),
                                  QVariant(p.este)})
            q.addBindValue(v);
        if (!q.exec()) {
            m_error = q.lastError().text();
            db.rollback();
            return false;
        }
    }
    return db.commit();
}
