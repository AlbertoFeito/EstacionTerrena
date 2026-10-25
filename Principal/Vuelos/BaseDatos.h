#ifndef BASEDATOS_H
#define BASEDATOS_H

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

/*!
 * \brief La base de datos de drones y vuelos (DronDB.db), con una conexion
 *        directa de Qt SQL.
 *
 * Sustituye al modulo Database (AsyncQuery, AsyncQueryModel y
 * ConnectionManager): lanzaba cada consulta en otro hilo y devolvia el
 * resultado mas tarde, lo que obligaba a encadenar callbacks y dejaba
 * carreras (respuestas que llegaban tarde o en otro orden). Las consultas de
 * la estacion son pequenas y SQLite las resuelve en milisegundos: ahora son
 * directas, en el hilo de la interfaz. La grabacion del vuelo va aparte
 * (GrabadorVuelo, con su conexion y escribiendo una vez por segundo).
 */
namespace BaseDatos {

//! Fichero de la base de datos; se fija una vez al arrancar.
void setFichero(const QString &fichero);
QString fichero();

//! La conexion (se abre la primera vez, con claves foraneas activas y espera
//! de hasta 5 s si otra conexion esta escribiendo).
QSqlDatabase drones();

//! Crea DronTable si no existe.
bool asegurarTablaDrones();

//! Nombres de los drones, por orden de alta.
QStringList nombresDrones();

//! Un dron por su nombre (id -1 si no existe).
struct Dron
{
    int id = -1;
    QString nombre;
    QString tipo;        //!< "Ala fija" o cuadroptero
    int vuelos = 0;
};
Dron dron(const QString &nombre);

} // namespace BaseDatos

#endif // BASEDATOS_H
