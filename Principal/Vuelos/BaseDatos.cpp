#include "BaseDatos.h"

#include <QSqlQuery>
#include <QVariant>

namespace BaseDatos {

namespace {
const char *kConexion = "drones";

// Fichero elegido al arrancar.
QString &ficheroElegido()
{
    static QString f;
    return f;
}
}

// Fija el fichero (antes de la primera consulta).
void setFichero(const QString &fichero)
{
    ficheroElegido() = fichero;
}

// Fichero de la base de datos.
QString fichero()
{
    return ficheroElegido();
}

// Abre la conexion la primera vez que se pide.
QSqlDatabase drones()
{
    const QString nombre = QString::fromLatin1(kConexion);
    if (QSqlDatabase::contains(nombre)) {
        QSqlDatabase db = QSqlDatabase::database(nombre, false);
        if (!db.isOpen())
            db.open();
        return db;
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), nombre);
    db.setDatabaseName(ficheroElegido());
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (db.open())
        QSqlQuery(db).exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    return db;
}

// La tabla de drones de siempre.
bool asegurarTablaDrones()
{
    QSqlQuery q(drones());
    return q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS DronTable("
                                 "Id_Dron INTEGER PRIMARY KEY NOT NULL UNIQUE, "
                                 "Nombre TEXT, Tipo TEXT, Vuelos INTEGER DEFAULT 0)"));
}

// Nombres de los drones.
QStringList nombresDrones()
{
    QStringList nombres;
    QSqlQuery q(drones());
    if (q.exec(QStringLiteral("SELECT Nombre FROM DronTable ORDER BY Id_Dron")))
        while (q.next())
            nombres << q.value(0).toString();
    return nombres;
}

// Un dron por su nombre.
Dron dron(const QString &nombre)
{
    Dron d;
    QSqlQuery q(drones());
    q.prepare(QStringLiteral("SELECT Id_Dron, Nombre, Tipo, Vuelos FROM DronTable WHERE Nombre = ?"));
    q.addBindValue(nombre);
    if (q.exec() && q.next()) {
        d.id = q.value(0).toInt();
        d.nombre = q.value(1).toString();
        d.tipo = q.value(2).toString();
        d.vuelos = q.value(3).toInt();
    }
    return d;
}

} // namespace BaseDatos
