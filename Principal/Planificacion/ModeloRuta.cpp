// Modelo de la tabla de puntos de la ruta y su base de datos (antes dentro de
// Widget_Puntos.cpp).

#include "ModeloRuta.h"
#include "InformeRuta.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyle>
#include <QTextStream>
#include <QtMath>
#include <cmath>
#include <limits>
#include "Rutas.h"

// ============================================================================
// IMPLEMENTACIÓN DE MiModelo (SIN OPTIMIZACIÓN)
// ============================================================================

/**
 * @brief Constructor del modelo de datos de la tabla de puntos.
 * @param parent Objeto padre (opcional).
 */
MiModelo::MiModelo(QObject *parent)
    : QAbstractTableModel(parent), planner(nullptr), m_alturaWorker(nullptr)
{
    inicializarBaseDeDatos();
}

MiModelo::~MiModelo()
{
    delete planner;
}

// ------------------------------------------------------------
// GESTIÓN DE DRONES
// ------------------------------------------------------------

/**
 * @brief Carga la lista de drones desde la base de datos.
 * @return true si se cargaron correctamente (al menos uno), false en caso contrario.
 */
bool MiModelo::cargarDronesDesdeBD()
{
    dronesList.clear();
    QSqlQuery query("SELECT * FROM drones ORDER BY nombre");
    while (query.next()) {
        DroneCharacteristics drone;
        drone.id = query.value("id").toInt();
        drone.nombre = query.value("nombre").toString();
        drone.type = DroneCharacteristics::stringToTipo(query.value("tipo").toString());
        drone.maxRange = query.value("max_range").toDouble();
        drone.maxSpeed = query.value("max_speed").toDouble();
        drone.minSpeed = query.value("min_speed").toDouble();
        drone.maxAcceleration = query.value("max_acceleration").toDouble();
        drone.maxDeceleration = query.value("max_deceleration").toDouble();
        drone.minTurnRadius = query.value("min_turn_radius").toDouble();
        drone.maxWaypointDistance = query.value("max_waypoint_distance").toDouble();
        drone.minWaypointDistance = query.value("min_waypoint_distance").toDouble();
        drone.endurance = query.value("endurance").toDouble();
        drone.minAltitude = query.value("min_altitude").toDouble();
        drone.maxAltitude = query.value("max_altitude").toDouble();
        drone.maxClimbRate = query.value("max_climb_rate").toDouble();
        drone.maxDescentRate = query.value("max_descent_rate").toDouble();
        dronesList.append(drone);
    }
    return !dronesList.isEmpty();
}

/**
 * @brief Obtiene la lista de drones cargados.
 * @return Lista de características de drones.
 */
QList<DroneCharacteristics> MiModelo::getDronesList() const
{
    return dronesList;
}

/**
 * @brief Guarda un dron en la base de datos (inserta o actualiza).
 * @param drone Características del dron a guardar.
 * @return true si la operación fue exitosa, false en caso contrario.
 */
bool MiModelo::guardarDroneEnBD(const DroneCharacteristics &drone)
{
    QSqlQuery query;
    if (drone.id == -1) {
        query.prepare("INSERT INTO drones (nombre, tipo, max_range, max_speed, min_speed, "
                      "max_acceleration, max_deceleration, min_turn_radius, "
                      "max_waypoint_distance, min_waypoint_distance, endurance, "
                      "min_altitude, max_altitude, max_climb_rate, max_descent_rate) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    } else {
        query.prepare("UPDATE drones SET nombre = ?, tipo = ?, max_range = ?, max_speed = ?, "
                      "min_speed = ?, max_acceleration = ?, max_deceleration = ?, "
                      "min_turn_radius = ?, max_waypoint_distance = ?, min_waypoint_distance = ?, "
                      "endurance = ?, min_altitude = ?, max_altitude = ?, "
                      "max_climb_rate = ?, max_descent_rate = ? WHERE id = ?");
    }

    query.addBindValue(drone.nombre);
    query.addBindValue(drone.tipoToString());
    query.addBindValue(drone.maxRange);
    query.addBindValue(drone.maxSpeed);
    query.addBindValue(drone.minSpeed);
    query.addBindValue(drone.maxAcceleration);
    query.addBindValue(drone.maxDeceleration);
    query.addBindValue(drone.minTurnRadius);
    query.addBindValue(drone.maxWaypointDistance);
    query.addBindValue(drone.minWaypointDistance);
    query.addBindValue(drone.endurance);
    query.addBindValue(drone.minAltitude);
    query.addBindValue(drone.maxAltitude);
    query.addBindValue(drone.maxClimbRate);
    query.addBindValue(drone.maxDescentRate);

    if (drone.id != -1) query.addBindValue(drone.id);

    return query.exec();
}

// ------------------------------------------------------------
// CONFIGURACIÓN DE RUTA
// ------------------------------------------------------------

/**
 * @brief Configura una nueva ruta con un dron específico.
 * @param drone Características del dron.
 * @param routeName Nombre de la ruta.
 */
void MiModelo::configurarRuta(const DroneCharacteristics &drone, const QString &routeName)
{
    beginResetModel();
    listaDatos.clear();
    endResetModel();

    QSqlQuery dropQuery;
    dropQuery.exec(QString("DROP TABLE IF EXISTS \"%1\"").arg(routeName));

    QStringList columnas = {"nombre", "modo", "latitud", "longitud", "altura",
                            "velocidad", "radio", "distancia_anterior"};
    crearNuevaTabla(routeName, columnas);
    guardarMetadatosRuta(routeName, drone);

    this->droneChars = drone;
    currentRouteName = routeName;
    delete planner;
    planner = new RoutePlanner(droneChars);

    emit rutaConfigurada(drone.tipoToString(), routeName);
}

/**
 * @brief Guarda los metadatos de una ruta en la tabla `rutas_metadata`.
 * @param nombreRuta Nombre de la ruta.
 * @param drone Características del dron asociado.
 * @return true si la operación fue exitosa.
 */
bool MiModelo::guardarMetadatosRuta(const QString &nombreRuta, const DroneCharacteristics &drone)
{
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO rutas_metadata (nombre_ruta, drone_id, drone_nombre, drone_tipo) "
                  "VALUES (?, ?, ?, ?)");
    query.addBindValue(nombreRuta);
    query.addBindValue(drone.id);
    query.addBindValue(drone.nombre);
    query.addBindValue(drone.tipoToString());
    return query.exec();
}

/**
 * @brief Carga los metadatos de una ruta y completa la estructura de dron.
 * @param nombreRuta Nombre de la ruta.
 * @param drone Referencia a la estructura que se llenará con los datos.
 * @return true si se encontró la ruta y se cargaron los datos.
 */
bool MiModelo::cargarMetadatosRuta(const QString &nombreRuta, DroneCharacteristics &drone)
{
    QSqlQuery query;
    query.prepare("SELECT d.* FROM rutas_metadata rm "
                  "JOIN drones d ON rm.drone_id = d.id "
                  "WHERE rm.nombre_ruta = ?");
    query.addBindValue(nombreRuta);
    if (query.exec() && query.next()) {
        drone.id = query.value("id").toInt();
        drone.nombre = query.value("nombre").toString();
        drone.type = DroneCharacteristics::stringToTipo(query.value("tipo").toString());
        drone.maxRange = query.value("max_range").toDouble();
        drone.maxSpeed = query.value("max_speed").toDouble();
        drone.minSpeed = query.value("min_speed").toDouble();
        drone.maxAcceleration = query.value("max_acceleration").toDouble();
        drone.maxDeceleration = query.value("max_deceleration").toDouble();
        drone.minTurnRadius = query.value("min_turn_radius").toDouble();
        drone.maxWaypointDistance = query.value("max_waypoint_distance").toDouble();
        drone.minWaypointDistance = query.value("min_waypoint_distance").toDouble();
        drone.endurance = query.value("endurance").toDouble();
        drone.minAltitude = query.value("min_altitude").toDouble();
        drone.maxAltitude = query.value("max_altitude").toDouble();
        drone.maxClimbRate = query.value("max_climb_rate").toDouble();
        drone.maxDescentRate = query.value("max_descent_rate").toDouble();
        return true;
    }
    return false;
}

/**
 * @brief Obtiene las características del dron actualmente configurado.
 * @return Estructura DroneCharacteristics.
 */
DroneCharacteristics MiModelo::getDroneCharacteristics() const
{
    return droneChars;
}

/**
 * @brief Obtiene el nombre de la ruta actual.
 * @return Nombre de la ruta (cadena vacía si no hay ninguna).
 */
QString MiModelo::getCurrentRouteName() const
{
    return currentRouteName;
}

/**
 * @brief Indica si hay un dron configurado (es decir, existe un planificador).
 * @return true si hay dron configurado.
 */
bool MiModelo::isDroneConfigured() const
{
    return planner != nullptr;
}

// ------------------------------------------------------------
// VALIDACIÓN Y ANÁLISIS
// ------------------------------------------------------------

// La revision de la ruta, rehecha solo si han cambiado los puntos o el dron
// (la firma junta lo que la afecta). Con el terreno del mapa si lo hay.
const RevisionRuta &MiModelo::revision() const
{
    // Bytes tal cual (sin formatear): la tabla pregunta por cada celda que pinta.
    QByteArray firma;
    firma.reserve(48 * listaDatos.size() + 8);
    const quintptr dron = quintptr(planner);
    firma.append(reinterpret_cast<const char *>(&dron), sizeof dron);
    firma.append(m_alturaWorker && m_alturaWorker->hayMapa() ? 'T' : '-');   // el mapa llega despues
    for (const E_Punto &p : listaDatos) {
        const double v[4] = {p.pos.latitude(), p.pos.longitude(), p.altura, p.velocidad};
        firma.append(reinterpret_cast<const char *>(v), sizeof v);
        firma.append(reinterpret_cast<const char *>(p.nombre.constData()), p.nombre.size() * int(sizeof(QChar)));
        firma.append('\0');
    }
    if (firma == m_firmaRevision)
        return m_revision;
    m_firmaRevision = firma;
    if (!isDroneConfigured()) {
        m_revision = RevisionRuta();
        return m_revision;
    }
    RoutePlanner::FuncionTerreno terreno;
    if (m_alturaWorker)
        terreno = [this](const QGeoCoordinate &c) { return m_alturaWorker->cota(c); };
    m_revision = planner->revisar(listaDatos, terreno);
    return m_revision;
}




// ------------------------------------------------------------
// GESTIÓN DE PUNTOS – CON RESTRICCIÓN DE 10 PUNTOS
// ------------------------------------------------------------

/**
 * @brief Valida y agrega un nuevo punto a la ruta, respetando el límite de 10 puntos.
 * @param elemento Punto a agregar.
 * @return Par (éxito, mensaje).
 */
QPair<bool, QString> MiModelo::validarYAgregarElemento(const E_Punto &elemento)
{
    if (!isDroneConfigured())
        return qMakePair(false, QStringLiteral("Dron no configurado"));

    if (listaDatos.size() >= 10) {
        return qMakePair(false, QStringLiteral(
                             "❌ No se pueden agregar más puntos.\n"
                             "La ruta debe tener EXACTAMENTE 10 puntos.\n"
                             "Elimine o modifique puntos existentes."));
    }

    E_Punto nuevoPunto = elemento;
    nuevoPunto.id = listaDatos.size();

    if (!listaDatos.isEmpty())
        nuevoPunto.distanciaAnterior = listaDatos.last().distanciaA(nuevoPunto);
    else
        nuevoPunto.distanciaAnterior = 0.0;

    if (!listaDatos.isEmpty())
        nuevoPunto.tasaAscenso = nuevoPunto.calcularTasaVertical(listaDatos.last());

    beginInsertRows(QModelIndex(), listaDatos.size(), listaDatos.size());
    listaDatos.append(nuevoPunto);
    endInsertRows();

    if (!currentRouteName.isEmpty())
        insertarElementosEnTabla(currentRouteName, nuevoPunto);

    emit rutaModificada();

    // Lo que la revision ve en el punto nuevo (o en el tramo que acaba en el).
    const QVector<Hallazgo> suyos = revision().delPunto(listaDatos.size() - 1);
    QStringList lineas;
    bool hayError = false;
    for (const Hallazgo &h : suyos) {
        lineas.append(QStringLiteral("%1: %2").arg(h.lugar, h.titulo));
        hayError = hayError || h.gravedad == Hallazgo::Gravedad::Error;
    }
    if (lineas.isEmpty())
        return qMakePair(true, QStringLiteral("Punto añadido"));
    return qMakePair(!hayError, lineas.join(QLatin1Char('\n')));
}

/**
 * @brief Agrega un punto a la ruta sin retornar mensaje (usa validarYAgregarElemento internamente).
 * @param elemento Punto a agregar.
 */
void MiModelo::agregarElemento(const E_Punto &elemento)
{
    validarYAgregarElemento(elemento);
}


/**
 * @brief Actualiza la posición de puntos existentes en la base de datos y revalida la ruta.
 * @param nombreTabla Nombre de la tabla de la ruta.
 * @param elementos Lista de puntos con los nuevos datos (solo se actualizan los que coinciden por id).
 * @return true si todas las actualizaciones fueron exitosas.
 */
bool MiModelo::actualizarElementosEnTabla(const QString &nombreTabla, const QList<E_Punto> &elementos)
{
    QString nombreTablaLimpio = nombreTabla;
    if (nombreTablaLimpio.contains('('))
        nombreTablaLimpio = nombreTablaLimpio.split('(').first().trimmed();
    nombreTablaLimpio = nombreTablaLimpio.replace("✓", "").trimmed()
            .replace("⚠", "").trimmed()
            .replace("✗", "").trimmed();

    QMap<int, int> idToIndex;
    for (int i = 0; i < listaDatos.size(); ++i)
        idToIndex[listaDatos[i].id] = i;

    for (const E_Punto &elemento : elementos) {
        if (idToIndex.contains(elemento.id)) {
            int idx = idToIndex[elemento.id];
            E_Punto &punto = listaDatos[idx];
            punto.pos = elemento.pos;
            if (idx > 0)
                punto.distanciaAnterior = listaDatos[idx-1].distanciaA(punto);
            else
                punto.distanciaAnterior = 0.0;
        }
    }

    for (const E_Punto &elemento : elementos) {
        if (idToIndex.contains(elemento.id)) {
            int idx = idToIndex[elemento.id];
            if (idx + 1 < listaDatos.size())
                listaDatos[idx + 1].distanciaAnterior = listaDatos[idx].distanciaA(listaDatos[idx + 1]);
        }
    }


    if (isDroneConfigured())
        revalidarRutaCompleta();

    QSqlQuery query;
    query.prepare(QString("UPDATE \"%1\" SET latitud = :latitud, longitud = :longitud, "
                          "distancia_anterior = :distancia WHERE id = :id").arg(nombreTablaLimpio));

    bool todosActualizados = true;
    for (const E_Punto &elemento : elementos) {
        if (idToIndex.contains(elemento.id)) {
            int idx = idToIndex[elemento.id];
            query.bindValue(":id", elemento.id);
            query.bindValue(":latitud", elemento.pos.latitude());
            query.bindValue(":longitud", elemento.pos.longitude());
            query.bindValue(":distancia", listaDatos[idx].distanciaAnterior);
            if (!query.exec())
                todosActualizados = false;
        }
    }

    if (!listaDatos.isEmpty())
        emit dataChanged(index(0, 0), index(rowCount()-1, columnCount()-1));

    emit rutaModificada();

    return todosActualizados;
}

/**
 * @brief Elimina todos los puntos de la ruta actual.
 */
void MiModelo::borraTodosLosElementos()
{
    beginResetModel();
    listaDatos.clear();
    endResetModel();
    emit rutaModificada();
}

/**
 * @brief Reinicia el modelo: borra puntos, elimina el planificador y limpia el nombre de ruta.
 */
void MiModelo::resetearModelo()
{
    beginResetModel();
    listaDatos.clear();
    delete planner;
    planner = nullptr;
    currentRouteName.clear();
    droneChars = DroneCharacteristics();
    endResetModel();
    emit rutaModificada();
}

/**
 * @brief Obtiene la lista de puntos actual.
 * @return Referencia constante a la lista interna.
 */
const QList<E_Punto> &MiModelo::obtenerLista() const
{
    return listaDatos;
}


// Cota del terreno del mapa, o NaN si no hay dato (ni mapa).
double MiModelo::cotaTerreno(const QGeoCoordinate &pos) const
{
    return m_alturaWorker ? m_alturaWorker->cota(pos) : std::numeric_limits<double>::quiet_NaN();
}

// Las rutas propuestas por el optimizador, con el terreno del mapa.
QVector<PropuestaRuta> MiModelo::proponerRutas(const QGeoCoordinate &p1, const QGeoCoordinate &p10,
                                               double sobreTerreno, double anchoPasillo) const
{
    if (!isDroneConfigured())
        return {};
    return planner->proponerRutas(p1, p10, sobreTerreno,
                                  [this](const QGeoCoordinate &c) { return cotaTerreno(c); }, anchoPasillo);
}

// La trayectoria probable de la ruta actual (vacia sin dron).
QVector<QGeoCoordinate> MiModelo::trayectoriaProbable(double radioAcercamiento) const
{
    if (!isDroneConfigured())
        return {};
    return planner->trayectoriaProbable(listaDatos, radioAcercamiento);
}

// La ruta por defecto desde p1 (la genera RoutePlanner con el terreno del mapa).
QList<E_Punto> MiModelo::generarRuta(const QGeoCoordinate &p1, double sobreTerreno, bool *conTerreno) const
{
    if (!isDroneConfigured())
        return {};
    return planner->generarRuta(p1, sobreTerreno,
                                [this](const QGeoCoordinate &c) { return cotaTerreno(c); }, conTerreno);
}

// ------------------------------------------------------------
// PERSISTENCIA
// ------------------------------------------------------------

/**
 * @brief Guarda la ruta actual en un archivo CSV.
 * @param nombreFile Nombre del archivo (debe terminar en .csv).
 * @return true si se guardó correctamente.
 */
bool MiModelo::guardarCSV(const QString &nombreFile)
{
    QFile file(nombreFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream stream(&file);
    stream << "ID,Nombre,Modo,Latitud,Longitud,Distancia_Anterior(m),Altura,Velocidad,Radio,Observaciones\n";
    for (int fila = 0; fila < listaDatos.size(); ++fila) {
        const E_Punto &punto = listaDatos.at(fila);
        stream << punto.id
               << "," << punto.nombre
               << "," << punto.modo
               << "," << QString::number(punto.pos.latitude(), 'f', 8)
               << "," << QString::number(punto.pos.longitude(), 'f', 8)
               << "," << QString::number(punto.distanciaAnterior, 'f', 2)
               << "," << QString::number(punto.altura, 'f', 2)
               << "," << QString::number(punto.velocidad, 'f', 2)
               << "," << QString::number(punto.radio, 'f', 2)
               << "," << observacionesPunto(fila)
               << "\n";
    }
    file.close();
    return true;
}

/**
 * @brief Crea una nueva tabla en la base de datos para almacenar una ruta.
 * @param nombreTabla Nombre de la tabla.
 * @param nombresColumnas Lista de nombres de columnas (sin incluir id).
 * @return true si la tabla se creó correctamente.
 */
bool MiModelo::crearNuevaTabla(const QString &nombreTabla, const QStringList &nombresColumnas)
{
    QSqlQuery query;
    QString comando = QString("CREATE TABLE IF NOT EXISTS %1 (id INTEGER PRIMARY KEY AUTOINCREMENT").arg(nombreTabla);
    for (const QString &columna : nombresColumnas)
        comando += ", " + columna + " TEXT NOT NULL";
    comando += ")";
    if (!query.exec(comando)) {
        return false;
    }
    return true;
}

/**
 * @brief Inserta un punto en la tabla de la ruta.
 * @param nombreTabla Nombre de la tabla.
 * @param elemento Punto a insertar.
 * @return true si la inserción fue exitosa.
 */
bool MiModelo::insertarElementosEnTabla(const QString &nombreTabla, const E_Punto &elemento)
{
    QSqlQuery query;
    query.prepare(QString("INSERT INTO %1 (id, nombre, modo, latitud, longitud, altura, velocidad, radio, distancia_anterior) "
                          "VALUES (?,?,?,?,?,?,?,?,?)").arg(nombreTabla));
    query.addBindValue(elemento.id);
    query.addBindValue(elemento.nombre);
    query.addBindValue(elemento.modo);
    query.addBindValue(QString::number(elemento.pos.latitude(), 'f', 8));
    query.addBindValue(QString::number(elemento.pos.longitude(), 'f', 8));
    query.addBindValue(QString::number(elemento.altura, 'f', 2));
    query.addBindValue(QString::number(elemento.velocidad, 'f', 2));
    query.addBindValue(QString::number(elemento.radio, 'f', 2));
    query.addBindValue(QString::number(elemento.distanciaAnterior, 'f', 2));
    if (!query.exec()) {
        return false;
    }
    return true;
}

/**
 * @brief Actualiza un punto existente en la tabla de la ruta.
 * @param nombreTabla Nombre de la tabla.
 * @param elemento Punto con los nuevos datos (se identifica por id).
 * @return true si la actualización fue exitosa.
 */
bool MiModelo::actualizarElementoEnTabla(const QString &nombreTabla, const E_Punto &elemento)
{
    QString nombreTablaLimpio = nombreTabla;
    if (nombreTablaLimpio.contains('('))
        nombreTablaLimpio = nombreTablaLimpio.split('(').first().trimmed();
    nombreTablaLimpio = nombreTablaLimpio.replace("✓", "").trimmed()
            .replace("⚠", "").trimmed()
            .replace("✗", "").trimmed();

    QSqlQuery query;
    query.prepare(QString("UPDATE \"%1\" SET nombre = :nombre, modo = :modo, latitud = :latitud, longitud = :longitud,"
                          " altura = :altura, velocidad = :velocidad, radio = :radio, distancia_anterior = :distancia WHERE id = :id")
                  .arg(nombreTablaLimpio));
    query.bindValue(":id", elemento.id);
    query.bindValue(":nombre", elemento.nombre);
    query.bindValue(":modo", elemento.modo);
    query.bindValue(":latitud", elemento.pos.latitude());
    query.bindValue(":longitud", elemento.pos.longitude());
    query.bindValue(":altura", elemento.altura);
    query.bindValue(":velocidad", elemento.velocidad);
    query.bindValue(":radio", elemento.radio);
    query.bindValue(":distancia", elemento.distanciaAnterior);
    if (!query.exec()) {
        return false;
    }
    return true;
}

/**
 * @brief Carga una ruta desde la base de datos, incluyendo sus puntos y el dron asociado.
 * @param nombreRuta Nombre de la ruta (tabla).
 */
void MiModelo::CargarRutaDesdeBaseDatos(const QString &nombreRuta)
{
    DroneCharacteristics drone;
    if (cargarMetadatosRuta(nombreRuta, drone)) {
        droneChars = drone;
        delete planner;
        planner = new RoutePlanner(droneChars);
    } else {
        planner = nullptr;
    }

    currentRouteName = nombreRuta;

    QSqlQuery query;
    query.prepare(QString("SELECT * FROM %1 ORDER BY id").arg(nombreRuta));
    if (!query.exec()) {
        emit rutaModificada();
            return;
    }

    beginResetModel();
    listaDatos.clear();

    while (query.next()) {
        E_Punto punto;
        punto.id = query.value(0).toInt();
        punto.nombre = query.value(1).toString();
        punto.modo = query.value(2).toInt();
        punto.pos = QGeoCoordinate(query.value(3).toDouble(), query.value(4).toDouble());
        punto.altura = query.value(5).toDouble();
        punto.velocidad = query.value(6).toDouble();
        punto.radio = query.value(7).toDouble();
        punto.distanciaAnterior = query.value(8).toDouble();
        listaDatos.append(punto);
    }

    endResetModel();

    if (isDroneConfigured())
        revalidarRutaCompleta();

    emit rutaModificada();
}

/**
 * @brief Elimina un punto específico de la ruta por su ID.
 * @param nombreTabla Nombre de la tabla.
 * @param id ID del punto a eliminar.
 * @return true si se eliminó correctamente.
 */
bool MiModelo::eliminarRutaPorId(const QString &nombreTabla, int id)
{
    QString nombreTablaLimpio = nombreTabla;
    if (nombreTablaLimpio.contains('('))
        nombreTablaLimpio = nombreTablaLimpio.split('(').first().trimmed();

    QSqlQuery query;
    query.prepare(QString("DELETE FROM \"%1\" WHERE id = :id").arg(nombreTablaLimpio));
    query.bindValue(":id", id);
    if (!query.exec())
        return false;

    for (int i = 0; i < listaDatos.size(); ++i) {
        if (listaDatos.at(i).id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            listaDatos.removeAt(i);
            endRemoveRows();
            if (i < listaDatos.size()) {
                if (i > 0)
                    listaDatos[i].distanciaAnterior = listaDatos[i-1].distanciaA(listaDatos[i]);
                else
                    listaDatos[i].distanciaAnterior = 0.0;
                emit dataChanged(index(i, 5), index(i, 5));
            }
            emit rutaModificada();
                    return true;
        }
    }
    return false;
}

/**
 * @brief Elimina completamente una tabla de ruta y sus metadatos.
 * @param nombreTabla Nombre de la tabla a eliminar.
 * @return true si la operación fue exitosa.
 */
bool MiModelo::eliminarTablaRuta(const QString &nombreTabla)
{
    QString nombreTablaLimpio = nombreTabla;
    if (nombreTablaLimpio.contains('('))
        nombreTablaLimpio = nombreTablaLimpio.split('(').first().trimmed();

    QSqlQuery query;
    query.prepare(QString("DROP TABLE IF EXISTS \"%1\"").arg(nombreTablaLimpio));
    if (!query.exec())
        return false;

    QSqlQuery query2;
    query2.prepare("DELETE FROM rutas_metadata WHERE nombre_ruta = ?");
    query2.addBindValue(nombreTablaLimpio);
    query2.exec();

    beginResetModel();
    listaDatos.clear();
    endResetModel();

    currentRouteName = "";
    delete planner;
    planner = nullptr;
    emit rutaModificada();
    return true;
}

/**
 * @brief Limpia todos los puntos de la tabla de ruta (los elimina, pero conserva la tabla y metadatos).
 * @param nombreTabla Nombre de la tabla.
 * @return true si la operación fue exitosa.
 */
bool MiModelo::limpiaRutaCompleta(const QString &nombreTabla)
{
    QString nombreTablaLimpio = nombreTabla;
    if (nombreTablaLimpio.contains('('))
        nombreTablaLimpio = nombreTablaLimpio.split('(').first().trimmed();

    QSqlQuery query;
    query.prepare(QString("DELETE FROM \"%1\"").arg(nombreTablaLimpio));
    if (!query.exec())
        return false;

    beginResetModel();
    listaDatos.clear();
    endResetModel();

    emit rutaModificada();
    return true;
}

// ------------------------------------------------------------
// INTERFAZ QAbstractTableModel
// ------------------------------------------------------------

/**
 * @brief Número de filas del modelo.
 */
int MiModelo::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return listaDatos.size();
}

/**
 * @brief Número de columnas del modelo.
 */
int MiModelo::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 11;
}

// Lo que la revision ve en el punto \a fila en una linea, para los CSV (sin comas).
QString MiModelo::observacionesPunto(int fila) const
{
    QStringList lineas;
    for (const Hallazgo &h : revision().delPunto(fila))
        lineas.append(QStringLiteral("%1 %2: %3").arg(InformeRuta::nombreGravedad(h.gravedad), h.lugar, h.titulo));
    return lineas.join(QStringLiteral(" | ")).replace(QLatin1Char(','), QLatin1Char(';'));
}

/**
 * @brief Devuelve los datos del modelo para un rol específico.
 * @param index Índice de la celda.
 * @param role Rol solicitado.
 * @return Valor del dato.
 */
QVariant MiModelo::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= listaDatos.size())
        return QVariant();

    const E_Punto &item = listaDatos.at(index.row());
    int puntoId = item.id;

    if (role == IdRole) return puntoId;

    // Lo que la revision ve en este punto (o en sus tramos) y lo peor de ello.
    const QVector<Hallazgo> suyos = revision().delPunto(index.row());
    int peor = -1;   // -1: nada; si no, el Hallazgo::Gravedad mas grave
    for (const Hallazgo &h : suyos)
        if (peor < 0 || int(h.gravedad) < peor)
            peor = int(h.gravedad);
    if (role == GravedadRole) return peor;
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return item.id;
        case 1: return item.nombre;
        case 2: return item.modo;
        // Con los decimales que dicen algo: metros enteros, velocidad con uno.
        case 3: return QString::number(item.pos.latitude(), 'f', 5);
        case 4: return QString::number(item.pos.longitude(), 'f', 5);
        case 5: return QString::number(item.distanciaAnterior, 'f', 0);
        case 6: return QString::number(item.altura, 'f', 0);
        case 7: {
            const double cota = revision().cotas.value(index.row(), std::numeric_limits<double>::quiet_NaN());
            return std::isfinite(cota) ? QString::number(item.altura - cota, 'f', 0) : QStringLiteral("—");
        }
        case 8: return QString::number(item.velocidad, 'f', 1);
        case 9: return QString::number(item.radio, 'f', 0);
        case 10:
            return QVariant();   // el estado va como icono (DecorationRole), sin texto repetido
        default: return QVariant();
        }
    }
    // Al editar, el valor entero (antes no habia EditRole y el editor salia vacio).
    if (role == Qt::EditRole) {
        switch (index.column()) {
        case 1: return item.nombre;
        case 2: return item.modo;
        case 3: return item.pos.latitude();
        case 4: return item.pos.longitude();
        case 6: return item.altura;
        case 7: {
            const double cota = revision().cotas.value(index.row(), std::numeric_limits<double>::quiet_NaN());
            return std::isfinite(cota) ? QVariant(item.altura - cota) : QVariant();
        }
        case 8: return item.velocidad;
        case 9: return item.radio;
        default: return QVariant();
        }
    }
    // El punto en una linea y, debajo, cada problema con su porque y su arreglo.
    if (role == Qt::ToolTipRole) {
        const double cota = revision().cotas.value(index.row(), std::numeric_limits<double>::quiet_NaN());
        const QString alturas = std::isfinite(cota)
                ? QStringLiteral("%1 m sobre el mar (%2 sobre el terreno)").arg(item.altura, 0, 'f', 0)
                      .arg(item.altura - cota, 0, 'f', 0)
                : QStringLiteral("%1 m sobre el mar").arg(item.altura, 0, 'f', 0);
        QString t = QStringLiteral("<table width='400'><tr><td><b>%1</b> · %2 · %3 m/s")
                        .arg(item.nombre.toHtmlEscaped(), alturas, QString::number(item.velocidad, 'f', 1));
        if (index.row() > 0)
            t += QStringLiteral(" · %1 m desde el anterior").arg(item.distanciaAnterior, 0, 'f', 0);
        if (suyos.isEmpty())
            t += QStringLiteral("<br><span style='color:#2e7d32'>Sin problemas.</span>");
        for (const Hallazgo &h : suyos)
            t += QStringLiteral("<p style='margin-top:6px'><b style='color:%1'>%2 · %3:</b> <b>%4</b><br>%5<br>"
                                "<i>Cómo arreglarlo:</i> %6</p>")
                     .arg(InformeRuta::colorGravedad(h.gravedad), InformeRuta::nombreGravedad(h.gravedad), h.lugar.toHtmlEscaped(),
                          h.titulo.toHtmlEscaped(), h.explicacion.toHtmlEscaped(), h.solucion.toHtmlEscaped());
        return t + QStringLiteral("</td></tr></table>");
    }
    // Un tinte translucido del color de la gravedad: se lee con el tema claro
    // y con el oscuro (un fondo claro fijo dejaba la letra clara sin contraste).
    if (role == Qt::BackgroundRole) {
        if (peor == int(Hallazgo::Gravedad::Error) || peor == int(Hallazgo::Gravedad::Aviso)) {
            QColor tinte(InformeRuta::colorGravedad(Hallazgo::Gravedad(peor)));
            tinte.setAlpha(70);
            return QBrush(tinte);
        }
        return QVariant();
    }
    // Sin FontRole: la negrita de las filas con errores (y su QFont() con la
    // letra de la aplicacion, no la de la tabla) ensanchaba las columnas y la
    // tabla se comia el perfil. El fondo rojo y el icono ya lo marcan.
    // Numeros a la derecha (se comparan de un vistazo); nombre y modo, centrados.
    if (role == Qt::TextAlignmentRole)
        return int(index.column() >= 3 && index.column() <= 9 ? Qt::AlignRight | Qt::AlignVCenter
                                                              : Qt::AlignCenter);
    if (role == Qt::DecorationRole && index.column() == 10) {
        // Iconos del estilo (los :/icons/... de antes no existian en ningun .qrc).
        QStyle *estilo = QApplication::style();
        switch (peor) {
        case int(Hallazgo::Gravedad::Error): return estilo->standardIcon(QStyle::SP_MessageBoxCritical);
        case int(Hallazgo::Gravedad::Aviso): return estilo->standardIcon(QStyle::SP_MessageBoxWarning);
        case int(Hallazgo::Gravedad::Nota): return estilo->standardIcon(QStyle::SP_MessageBoxInformation);
        default: return estilo->standardIcon(QStyle::SP_DialogApplyButton);
        }
    }
    return QVariant();
}

/**
 * @brief Establece el valor de una celda en el modelo y revalida toda la ruta.
 * @param index Índice de la celda.
 * @param value Nuevo valor.
 * @param role Rol (debe ser Qt::EditRole).
 * @return true si se pudo cambiar.
 */
bool MiModelo::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    E_Punto &item = listaDatos[index.row()];
    bool cambioAceptado = false;

    switch (index.column()) {
    case 1: item.nombre = value.toString(); cambioAceptado = true; break;
    case 2: item.modo = value.toInt(); cambioAceptado = true; break;
    case 3: item.pos.setLatitude(value.toDouble()); cambioAceptado = true; break;
    case 4: item.pos.setLongitude(value.toDouble()); cambioAceptado = true; break;
    case 5: return false; // no editable
    case 6: item.altura = value.toDouble(); cambioAceptado = true; break;
    case 7: {
        // Sobre el terreno: se guarda la altura sobre el mar que le corresponde.
        const double cota = cotaTerreno(item.pos);
        if (!std::isfinite(cota))
            return false;
        item.altura = float(cota + value.toDouble());
        cambioAceptado = true;
        break;
    }
    case 8: item.velocidad = value.toDouble(); cambioAceptado = true; break;
    case 9: item.radio = value.toDouble(); cambioAceptado = true; break;
    default: return false;
    }

    if (cambioAceptado) {
        // Actualizar base de datos si hay una ruta activa
        if (!currentRouteName.isEmpty())
            actualizarElementoEnTabla(currentRouteName, item);

        // Revalidar toda la ruta (actualiza distancias, errores, puntos críticos y emite dataChanged global)
        revalidarRutaCompleta();

        return true;
    }
    return false;
}

// Mueve un punto conservando su altura sobre el terreno (si sabia 30 m sobre
// una loma, sigue a 30 m sobre lo que haya en el sitio nuevo). Sin cota en
// alguno de los dos sitios, se queda la altura sobre el mar que tenia. Un solo
// guardado y una sola revalidacion: el arrastre no debe repintar tres veces.
bool MiModelo::moverPunto(int fila, const QGeoCoordinate &pos)
{
    if (fila < 0 || fila >= listaDatos.size() || !pos.isValid())
        return false;

    E_Punto &item = listaDatos[fila];
    const double antes = cotaTerreno(item.pos), despues = cotaTerreno(pos);
    if (std::isfinite(antes) && std::isfinite(despues))
        item.altura = float(item.altura - antes + despues);
    item.pos.setLatitude(pos.latitude());
    item.pos.setLongitude(pos.longitude());

    if (!currentRouteName.isEmpty())
        actualizarElementoEnTabla(currentRouteName, item);
    revalidarRutaCompleta();
    return true;
}

/**
 * @brief Devuelve los flags de la celda (editable excepto ID, distancia y estado).
 */
Qt::ItemFlags MiModelo::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    if (index.column() == 0 || index.column() == 5 || index.column() == 10)
        return flags;
    // Sin cota, la altura sobre el terreno no se sabe: no se edita.
    if (index.column() == 7 && !std::isfinite(revision().cotas.value(index.row(),
                                                                     std::numeric_limits<double>::quiet_NaN())))
        return flags;
    return flags | Qt::ItemIsEditable;
}

/**
 * @brief Devuelve los encabezados de las columnas.
 */
QVariant MiModelo::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();
    // Cabeceras cortas (la tabla cabe sin estirarse) y el nombre largo en el tooltip.
    if (role == Qt::DisplayRole) {
        switch (section) {
        case 0: return "ID";
        case 1: return "Punto";
        case 2: return "Modo";
        case 3: return "Latitud";
        case 4: return "Longitud";
        case 5: return "Dist. (m)";
        case 6: return "Alt. (m)";
        case 7: return "S/terr. (m)";
        case 8: return "Vel. (m/s)";
        case 9: return "Radio (m)";
        case 10: return "";
        default: return QVariant();
        }
    }
    if (role == Qt::ToolTipRole) {
        switch (section) {
        case 5: return "Distancia desde el punto anterior, en metros";
        case 6: return "Altura sobre el nivel del mar, en metros: la que se manda al dron";
        case 7: return "Altura sobre el terreno de debajo, en metros (— si no hay datos de elevación). "
                       "Editarla cambia la altura sobre el mar";
        case 8: return "Velocidad en m/s, la que vuela el dron (en pantalla se ve en km/h: x 3,6)";
        case 9: return "Radio de giro en el punto, en metros (no se manda al dron)";
        case 10: return "Estado del punto: bien, con avisos o con errores (el detalle, al pasar el ratón por la fila)";
        default: return QVariant();
        }
    }
    return QVariant();
}

/**
 * @brief No se permite insertar filas individualmente (límite fijo de 10 puntos).
 */
bool MiModelo::insertRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(row); Q_UNUSED(count); Q_UNUSED(parent);
    qWarning() << "No se pueden insertar puntos individualmente (máximo 10 fijos)";
    return false;
}

/**
 * @brief No se permite eliminar filas individualmente.
 */
bool MiModelo::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(row); Q_UNUSED(count); Q_UNUSED(parent);
    qWarning() << "No se pueden eliminar puntos individualmente";
    return false;
}

// ------------------------------------------------------------
// REVALIDACIÓN COMPLETA
// ------------------------------------------------------------

// Distancias y tasas de cada punto al dia, y aviso a la tabla y al panel (que
// releen la revision, rehecha sola porque han cambiado los puntos).
void MiModelo::revalidarRutaCompleta()
{
    if (!isDroneConfigured()) return;

    for (int i = 0; i < listaDatos.size(); ++i) {
        listaDatos[i].distanciaAnterior = i > 0 ? listaDatos[i-1].distanciaA(listaDatos[i]) : 0.0;
        listaDatos[i].tasaAscenso = i > 0 ? listaDatos[i].calcularTasaVertical(listaDatos[i-1]) : 0.0;
    }

    if (!listaDatos.isEmpty())
        emit dataChanged(index(0, 0), index(rowCount()-1, columnCount()-1));
    emit rutaModificada();
}

/**
 * @brief Obtiene las coordenadas de todos los puntos de la ruta como pares (lat, lon).
 * @return Lista de pares.
 */
QList<QPair<double, double>> MiModelo::obtenerCoordenadasRuta()
{
    QList<QPair<double, double>> coordenadas;
    for (const E_Punto &elemento : listaDatos)
        coordenadas.append(qMakePair(elemento.pos.latitude(), elemento.pos.longitude()));
    return coordenadas;
}

/**
 * @brief Inicializa la base de datos SQLite y crea las tablas necesarias si no existen.
 */
void MiModelo::inicializarBaseDeDatos()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    const QString dirFich = Rutas::carpetaDatos();
    db.setDatabaseName(dirFich + "/rutas.sqlite");

    if (!db.open()) {
    } else {
        QSqlQuery query;
        query.exec("CREATE TABLE IF NOT EXISTS drones ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "nombre TEXT UNIQUE NOT NULL, "
                   "tipo TEXT NOT NULL, "
                   "max_range REAL NOT NULL, "
                   "max_speed REAL NOT NULL, "
                   "min_speed REAL NOT NULL DEFAULT 0.0, "
                   "max_acceleration REAL NOT NULL DEFAULT 2.0, "
                   "max_deceleration REAL NOT NULL DEFAULT 3.0, "
                   "min_turn_radius REAL NOT NULL, "
                   "max_waypoint_distance REAL NOT NULL, "
                   "min_waypoint_distance REAL NOT NULL, "
                   "endurance REAL NOT NULL, "
                   "min_altitude REAL NOT NULL, "
                   "max_altitude REAL NOT NULL, "
                   "max_climb_rate REAL NOT NULL DEFAULT 5.0, "
                   "max_descent_rate REAL NOT NULL DEFAULT 5.0)");

        query.exec("CREATE TABLE IF NOT EXISTS rutas_metadata ("
                   "nombre_ruta TEXT PRIMARY KEY, "
                   "drone_id INTEGER, "
                   "drone_nombre TEXT, "
                   "drone_tipo TEXT, "
                   "FOREIGN KEY(drone_id) REFERENCES drones(id))");

        QSqlQuery checkQuery("PRAGMA table_info(drones)");
        bool hasNewFields = false;
        while (checkQuery.next()) {
            QString columnName = checkQuery.value(1).toString();
            if (columnName == "min_speed" || columnName == "max_acceleration") {
                hasNewFields = true;
                break;
            }
        }

        if (!hasNewFields) {
            query.exec("ALTER TABLE drones RENAME TO drones_old");
            query.exec("CREATE TABLE drones ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "nombre TEXT UNIQUE NOT NULL, "
                       "tipo TEXT NOT NULL, "
                       "max_range REAL NOT NULL, "
                       "max_speed REAL NOT NULL, "
                       "min_speed REAL NOT NULL DEFAULT 0.0, "
                       "max_acceleration REAL NOT NULL DEFAULT 2.0, "
                       "max_deceleration REAL NOT NULL DEFAULT 3.0, "
                       "min_turn_radius REAL NOT NULL, "
                       "max_waypoint_distance REAL NOT NULL, "
                       "min_waypoint_distance REAL NOT NULL, "
                       "endurance REAL NOT NULL, "
                       "min_altitude REAL NOT NULL, "
                       "max_altitude REAL NOT NULL, "
                       "max_climb_rate REAL NOT NULL DEFAULT 5.0, "
                       "max_descent_rate REAL NOT NULL DEFAULT 5.0)");
            query.exec("INSERT INTO drones (id, nombre, tipo, max_range, max_speed, min_turn_radius, "
                       "max_waypoint_distance, min_waypoint_distance, endurance, min_altitude, max_altitude) "
                       "SELECT id, nombre, tipo, max_range, max_speed, min_turn_radius, "
                       "max_waypoint_distance, min_waypoint_distance, endurance, min_altitude, max_altitude "
                       "FROM drones_old");
            query.exec("DROP TABLE drones_old");
            query.exec("UPDATE drones SET "
                       "min_speed = CASE WHEN tipo = 'Ala Fija' THEN 10.0 ELSE 0.0 END, "
                       "max_acceleration = CASE WHEN tipo = 'Quadcopter' THEN 3.0 ELSE 2.0 END, "
                       "max_deceleration = CASE WHEN tipo = 'Quadcopter' THEN 4.0 ELSE 3.0 END, "
                       "max_climb_rate = CASE WHEN tipo = 'Quadcopter' THEN 5.0 ELSE 3.0 END, "
                       "max_descent_rate = CASE WHEN tipo = 'Quadcopter' THEN 5.0 ELSE 3.0 END");
        }

        query.exec("INSERT OR IGNORE INTO drones (nombre, tipo, max_range, max_speed, min_speed, "
                   "max_acceleration, max_deceleration, min_turn_radius, "
                   "max_waypoint_distance, min_waypoint_distance, endurance, "
                   "min_altitude, max_altitude, max_climb_rate, max_descent_rate) "
                   "VALUES ('Quadcopter Standard', 'Quadcopter', 5000, 15, 0, 3, 4, 2, 100, 5, 1800, 10, 120, 5, 5)");
        query.exec("INSERT OR IGNORE INTO drones (nombre, tipo, max_range, max_speed, min_speed, "
                   "max_acceleration, max_deceleration, min_turn_radius, "
                   "max_waypoint_distance, min_waypoint_distance, endurance, "
                   "min_altitude, max_altitude, max_climb_rate, max_descent_rate) "
                   "VALUES ('Ala Fija Standard', 'Ala Fija', 20000, 25, 10, 2, 3, 50, 500, 100, 3600, 50, 500, 3, 3)");
    }
}

