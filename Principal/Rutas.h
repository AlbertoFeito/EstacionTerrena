#ifndef RUTAS_H
#define RUTAS_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

/*!
 * \brief Donde estan conf.ini y los datos de la estacion (Recursos/data, con
 *        DronDB.db y rutas.sqlite).
 *
 * Siempre se buscaron respecto a la carpeta de trabajo: conf.ini en ella y los
 * datos en ../Recursos/data. Lanzada de otra forma (doble clic, un acceso
 * directo con otra carpeta), la estacion creaba en silencio una base de datos
 * vacia en otro sitio. Ahora se elige UNA carpeta base, una sola vez:
 *  1. la de trabajo, si tiene conf.ini o ../Recursos/data (lo de siempre);
 *  2. la del ejecutable, si tiene conf.ini o ../Recursos/data;
 *  3. si no, la de trabajo (como antes: ahi se crean).
 * Todo lo que lee o escribe conf.ini o la base de datos pasa por aqui.
 */
namespace Rutas {

namespace detalle {
// La carpeta "tiene datos": conf.ini dentro o ../Recursos/data al lado.
inline bool tieneDatos(const QString &carpeta)
{
    return QFileInfo::exists(carpeta + QStringLiteral("/conf.ini"))
        || QFileInfo::exists(carpeta + QStringLiteral("/../Recursos/data"));
}
}

//! Carpeta base (la de conf.ini), elegida la primera vez que se pide.
inline QString carpetaBase()
{
    static const QString base = []() {
        const QString trabajo = QDir::currentPath();
        if (detalle::tieneDatos(trabajo))
            return trabajo;
        const QString ejecutable = QCoreApplication::applicationDirPath();
        if (!ejecutable.isEmpty() && detalle::tieneDatos(ejecutable))
            return ejecutable;
        return trabajo;
    }();
    return base;
}

//! Fichero de configuracion de la estacion.
inline QString confIni()
{
    return carpetaBase() + QStringLiteral("/conf.ini");
}

//! Carpeta Recursos (junto a la base): el paquete de mapas por defecto.
inline QString carpetaRecursos()
{
    return QDir::cleanPath(carpetaBase() + QStringLiteral("/../Recursos"));
}

//! Carpeta de las bases de datos (Recursos/data); se crea si no existe.
inline QString carpetaDatos()
{
    const QString datos = carpetaRecursos() + QStringLiteral("/data");
    QDir().mkpath(datos);
    return datos;
}

} // namespace Rutas

#endif // RUTAS_H
