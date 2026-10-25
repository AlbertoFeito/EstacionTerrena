// Modelo de la tabla de puntos de la ruta y su base de datos (antes dentro de
// Widget_Puntos.cpp).

#include "ModeloRuta.h"

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
    erroresPorPunto.clear();
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
    emit analisisActualizado(analizarRutaDetallada());
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

/**
 * @brief Valida la ruta actual usando el planificador.
 * @return Par (válido, mensaje).
 */
QPair<bool, QString> MiModelo::validarRuta() const
{
    if (!isDroneConfigured())
        return qMakePair(false, QStringLiteral("Dron no configurado"));
    return planner->validarRuta(listaDatos);
}

/**
 * @brief Obtiene métricas básicas de la ruta (distancia, tiempo, porcentajes, etc.).
 * @return Mapa con las métricas.
 */
QVariantMap MiModelo::obtenerMetricasRuta() const
{
    QVariantMap metricas;
    if (!isDroneConfigured()) {
        metricas["error"] = "Dron no configurado";
        return metricas;
    }

    double distanciaTotal = planner->calcularDistanciaTotal(listaDatos);
    auto tiempoVuelo = planner->calcularTiempoVuelo(listaDatos);

    metricas["distancia_total"] = distanciaTotal;
    metricas["tiempo_vuelo"] = tiempoVuelo.first;
    metricas["energia_estimada"] = tiempoVuelo.second;
    metricas["num_waypoints"] = listaDatos.size();
    metricas["autonomia_restante"] = droneChars.maxRange - distanciaTotal;
    metricas["autonomia_porcentaje"] = (droneChars.maxRange > 0) ?
                (distanciaTotal / droneChars.maxRange * 100.0) : 0.0;
    metricas["endurance_restante"] = droneChars.endurance - tiempoVuelo.first;
    metricas["endurance_porcentaje"] = (droneChars.endurance > 0) ?
                (tiempoVuelo.first / droneChars.endurance * 100.0) : 0.0;

    auto validacion = validarRuta();
    metricas["valida"] = validacion.first;
    metricas["mensaje_validacion"] = validacion.second;

    return metricas;
}

/**
 * @brief Realiza un análisis detallado de la ruta (delega en RoutePlanner).
 * @return Mapa con todos los datos del análisis.
 */
QVariantMap MiModelo::analizarRutaDetallada() const
{
    if (!isDroneConfigured()) {
        QVariantMap resultado;
        resultado["error"] = "Dron no configurado";
        return resultado;
    }
    return planner->analizarRutaDetallada(listaDatos);
}

/**
 * @brief Obtiene una lista de recomendaciones basadas en el análisis de la ruta.
 * @return Lista de cadenas con recomendaciones.
 */
QStringList MiModelo::obtenerRecomendaciones() const
{
    QStringList recomendaciones;
    if (!isDroneConfigured() || listaDatos.size() < 2)
        return recomendaciones;

    auto analisis = analizarRutaDetallada();
    if (analisis.contains("error"))
        return recomendaciones;

    double eficiencia = analisis["eficiencia_ruta"].toDouble();
    if (eficiencia < 70.0)
        recomendaciones.append("✓ Considera optimizar la ruta para mayor eficiencia");

    double margenSeguridad = analisis["margen_seguridad"].toDouble();
    if (margenSeguridad < 30.0)
        recomendaciones.append("⚠ Margen de seguridad bajo (" + QString::number(margenSeguridad, 'f', 0) + "%)");

    int puntosCriticos = analisis["num_puntos_criticos"].toInt();
    if (puntosCriticos > 0)
        recomendaciones.append("⚠ " + QString::number(puntosCriticos) + " puntos críticos detectados");

    double estres = analisis["estres_estimado"].toDouble();
    if (estres > 70.0)
        recomendaciones.append("⚠ Estrés de ruta alto (" + QString::number(estres, 'f', 0) + "%)");

    if (droneChars.type == DroneType::FIXED_WING) {
        double velocidadPromedio = analisis["velocidad_promedio"].toDouble();
        double variacionTotal = 0;
        for (const auto &punto : listaDatos)
            variacionTotal += abs(punto.velocidad - velocidadPromedio);
        double variacionPromedio = variacionTotal / listaDatos.size();
        if (variacionPromedio > velocidadPromedio * 0.4)
            recomendaciones.append("⚠ Para ala fija, mantén velocidad más constante");
    }

    double consumoWh = analisis["consumo_wh"].toDouble();
    if (consumoWh > 500)
        recomendaciones.append("💡 Consumo energético alto. Considera optimizar velocidad y altitud");

    return recomendaciones;
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
    nuevoPunto.advertencias.clear();
    nuevoPunto.puntoCritico = false;

    if (!listaDatos.isEmpty())
        nuevoPunto.distanciaAnterior = listaDatos.last().distanciaA(nuevoPunto);
    else
        nuevoPunto.distanciaAnterior = 0.0;

    QStringList errores;
    QStringList advertencias;
    QString error;

    if (!planner->validarAltitud(nuevoPunto, error))
        errores.append(error);
    if (!planner->validarVelocidadPunto(nuevoPunto, error))
        errores.append(error);
    if (!planner->validarRadioGiro(nuevoPunto, error))
        advertencias.append(error);

    if (!listaDatos.isEmpty()) {
        const E_Punto &anterior = listaDatos.last();
        double distancia = anterior.distanciaA(nuevoPunto);

        if (!planner->validarDistanciaWaypoint(anterior, nuevoPunto, error))
            errores.append(error);
        if (!planner->validarTransicionVelocidad(anterior, nuevoPunto, distancia, error))
            advertencias.append(error);
        // CAMBIADO: ahora es error
        if (!planner->validarTransicionAltitud(anterior, nuevoPunto, distancia, error))
            errores.append(error);

        nuevoPunto.tasaAscenso = nuevoPunto.calcularTasaVertical(anterior);
        if (abs(nuevoPunto.tasaAscenso) > 3.0)
            advertencias.append(QString("⚠ TASA VERTICAL ALTA: %1 m/s").arg(nuevoPunto.tasaAscenso, 0, 'f', 1));
    }

    beginInsertRows(QModelIndex(), listaDatos.size(), listaDatos.size());
    listaDatos.append(nuevoPunto);
    if (!errores.isEmpty())
        erroresPorPunto[nuevoPunto.id] = errores.join("\n\n");
    if (!advertencias.isEmpty())
        listaDatos.last().advertencias = advertencias;
    endInsertRows();

    if (!currentRouteName.isEmpty())
        insertarElementosEnTabla(currentRouteName, nuevoPunto);

    emit rutaModificada();
    emit analisisActualizado(analizarRutaDetallada());

    if (!errores.isEmpty())
        return qMakePair(false, "❌ ERRORES DETECTADOS:\n\n" + errores.join("\n\n"));
    if (!advertencias.isEmpty())
        return qMakePair(true, "⚠ PUNTO AGREGADO CON ADVERTENCIAS:\n\n" + advertencias.join("\n\n"));
    return qMakePair(true, QStringLiteral("✅ PUNTO AGREGADO CORRECTAMENTE"));
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
 * @brief Valida una lista de puntos (sin agregarlos) contra las restricciones del dron.
 * @param puntos Lista de puntos a validar.
 * @return Par (válido, mensaje).
 */
QPair<bool, QString> MiModelo::validarYActualizarPuntos(const QList<E_Punto> &puntos)
{
    if (!isDroneConfigured())
        return qMakePair(false, QStringLiteral("Dron no configurado"));

    for (int i = 0; i < puntos.size(); ++i) {
        QString error;
        if (!planner->validarAltitud(puntos[i], error))
            return qMakePair(false, error);
        if (!planner->validarVelocidadPunto(puntos[i], error))
            return qMakePair(false, error);
    }

    for (int i = 0; i < puntos.size() - 1; ++i) {
        QString error;
        double distancia = puntos[i].distanciaA(puntos[i + 1]);
        if (!planner->validarDistanciaWaypoint(puntos[i], puntos[i + 1], error))
            return qMakePair(false, error);
        if (!planner->validarTransicionVelocidad(puntos[i], puntos[i + 1], distancia, error))
            return qMakePair(false, error);
        if (!planner->validarTransicionAltitud(puntos[i], puntos[i + 1], distancia, error))
            return qMakePair(false, error);
    }
    return qMakePair(true, QStringLiteral("Puntos válidos"));
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

    erroresPorPunto.clear();
    for (auto &punto : listaDatos) {
        punto.advertencias.clear();
        punto.puntoCritico = false;
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
    emit analisisActualizado(analizarRutaDetallada());

    return todosActualizados;
}

/**
 * @brief Elimina todos los puntos de la ruta actual.
 */
void MiModelo::borraTodosLosElementos()
{
    beginResetModel();
    listaDatos.clear();
    erroresPorPunto.clear();
    endResetModel();
    emit rutaModificada();
    emit analisisActualizado(analizarRutaDetallada());
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

/**
 * @brief Ajusta el número de puntos de la ruta a un número fijo (por defecto 10), añadiendo o eliminando según sea necesario.
 * @param numRequerido Número de puntos deseado.
 */
void MiModelo::ajustarAPuntosFijos(int numRequerido)
{
    if (!isDroneConfigured()) {
        qWarning() << "No se puede ajustar: dron no configurado";
        return;
    }

    int actual = listaDatos.size();
    if (actual == numRequerido) {
        return;
    }

    beginResetModel();

    if (actual < numRequerido) {
        int aAgregar = numRequerido - actual;
        QGeoCoordinate base = listaDatos.isEmpty() ? QGeoCoordinate(23.0, -82.0) : listaDatos.last().pos;
        QList<E_Punto> nuevos = generarPuntosPorDefecto(aAgregar, base, 100.0, 90.0);

        for (int i = 0; i < nuevos.size(); ++i) {
            E_Punto p = nuevos[i];
            p.id = actual + i;
            p.nombre = QString("P%1").arg(actual + i + 1);
            listaDatos.append(p);
        }
    } else {
        listaDatos.erase(listaDatos.begin() + numRequerido, listaDatos.end());
    }

    endResetModel();

    for (int i = 0; i < listaDatos.size(); ++i) {
        if (i > 0)
            listaDatos[i].distanciaAnterior = listaDatos[i-1].distanciaA(listaDatos[i]);
        else
            listaDatos[i].distanciaAnterior = 0.0;
    }

    revalidarRutaCompleta();
}

/**
 * @brief Genera puntos por defecto para una nueva ruta, teniendo en cuenta el dron configurado y el terreno si está disponible.
 * @param count Número de puntos a generar.
 * @param base Coordenada base para el primer punto (si no se especifica, usa un punto por defecto).
 * @param distancia Distancia entre puntos (si es <=0, se usa la distancia óptima).
 * @param azimut Dirección de avance en grados.
 * @return Lista de puntos generados.
 */
QList<E_Punto> MiModelo::generarPuntosPorDefecto(int count, const QGeoCoordinate &base,
                                                 double distancia, double azimut) const
{
    QList<E_Punto> puntos;

    if (!isDroneConfigured()) {
        double alt = 100.0;
        double vel = 15.0;
        double rad = 20.0;
        QGeoCoordinate puntoActual = base;
        for (int i = 0; i < count; ++i) {
            E_Punto p;
            p.id = i;
            p.nombre = QString("P%1").arg(i + 1);
            p.modo = 0;
            p.pos = puntoActual;
            p.altura = alt;
            p.velocidad = vel;
            p.radio = rad;
            p.distanciaAnterior = (i == 0) ? 0.0 : p.pos.distanceTo(puntos.last().pos);
            p.tasaAscenso = 0.0;
            p.puntoCritico = false;
            puntos.append(p);
            if (i < count - 1)
                puntoActual = puntoActual.atDistanceAndAzimuth(distancia, azimut);
        }
        return puntos;
    }

    // Calcular distancia óptima: punto medio entre min y max
    double distanciaOptima = (droneChars.minWaypointDistance + droneChars.maxWaypointDistance) / 2.0;
    if (distancia <= 0) distancia = distanciaOptima;

    double defaultVelocidad;
    if (droneChars.type == DroneType::FIXED_WING)
        defaultVelocidad = droneChars.maxSpeed * 0.65;
    else
        defaultVelocidad = droneChars.maxSpeed * 0.45;
    defaultVelocidad = qMax(defaultVelocidad, droneChars.minSpeed);

    double defaultRadio;
    if (droneChars.type == DroneType::FIXED_WING) {
        double radioTeorico = (defaultVelocidad * defaultVelocidad) / (9.81 * 0.577);
        defaultRadio = qMax(radioTeorico * 1.5, droneChars.minTurnRadius * 2.0);
    } else {
        defaultRadio = 10.0;
    }

    const double SAFETY_OFFSET = 50.0; // margen sobre el terreno

    QGeoCoordinate puntoActual = base;
    for (int i = 0; i < count; ++i) {
        E_Punto p;
        p.id = i;
        p.nombre = QString("P%1").arg(i + 1);
        p.modo = 0;
        p.pos = puntoActual;

        // Obtener altura del terreno si es posible
        double terreno = 0;
        if (m_alturaWorker) {
            terreno = m_alturaWorker->obtenerAltura(p.pos.latitude(), p.pos.longitude());
            if (terreno < 0) terreno = 0; // valor negativo indica error
        }
        double altitudBase = terreno + SAFETY_OFFSET;
        p.altura = qBound(droneChars.minAltitude, altitudBase, droneChars.maxAltitude);

        p.velocidad = defaultVelocidad;
        p.radio = defaultRadio;
        p.distanciaAnterior = (i == 0) ? 0.0 : p.pos.distanceTo(puntos.last().pos);
        p.tasaAscenso = 0.0;
        p.puntoCritico = false;
        puntos.append(p);

        if (i < count - 1)
            puntoActual = puntoActual.atDistanceAndAzimuth(distancia, azimut);
    }
    return puntos;
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
    stream << "ID,Nombre,Modo,Latitud,Longitud,Distancia_Anterior(m),Altura,Velocidad,Radio,Advertencias\n";
    for (const E_Punto &punto : qAsConst(listaDatos)) {
        stream << punto.id
               << "," << punto.nombre
               << "," << punto.modo
               << "," << QString::number(punto.pos.latitude(), 'f', 8)
               << "," << QString::number(punto.pos.longitude(), 'f', 8)
               << "," << QString::number(punto.distanciaAnterior, 'f', 2)
               << "," << QString::number(punto.altura, 'f', 2)
               << "," << QString::number(punto.velocidad, 'f', 2)
               << "," << QString::number(punto.radio, 'f', 2)
               << "," << punto.advertencias.join("; ")
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
        emit analisisActualizado(analizarRutaDetallada());
        return;
    }

    beginResetModel();
    listaDatos.clear();
    erroresPorPunto.clear();

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
        punto.advertencias.clear();
        punto.puntoCritico = false;
        listaDatos.append(punto);
    }

    endResetModel();

    if (isDroneConfigured())
        revalidarRutaCompleta();

    emit rutaModificada();
    emit analisisActualizado(analizarRutaDetallada());
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
            limpiarErroresAdyacentes(id);
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
            emit analisisActualizado(analizarRutaDetallada());
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
    erroresPorPunto.clear();
    endResetModel();

    currentRouteName = "";
    delete planner;
    planner = nullptr;
    emit rutaModificada();
    emit analisisActualizado(analizarRutaDetallada());
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
    erroresPorPunto.clear();
    endResetModel();

    emit rutaModificada();
    emit analisisActualizado(analizarRutaDetallada());
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
    return 10;
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
    if (role == ErrorRole) return erroresPorPunto.contains(puntoId) ? erroresPorPunto[puntoId] : QString();
    if (role == WarningRole) return item.advertencias.isEmpty() ? QString() : item.advertencias.join("\n");
    if (role == CriticoRole) return item.puntoCritico;
    if (role == AnalisisRole) {
        QVariantMap analisisPunto;
        analisisPunto["tasa_ascenso"] = item.tasaAscenso;
        analisisPunto["distancia_anterior"] = item.distanciaAnterior;
        analisisPunto["velocidad"] = item.velocidad;
        analisisPunto["altitud"] = item.altura;
        return analisisPunto;
    }
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
        case 7: return QString::number(item.velocidad, 'f', 1);
        case 8: return QString::number(item.radio, 'f', 0);
        case 9:
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
        case 7: return item.velocidad;
        case 8: return item.radio;
        default: return QVariant();
        }
    }
    if (role == Qt::ToolTipRole) {
        QString tooltip;
        tooltip += QString("📍 <b>PUNTO: %1</b><br>").arg(item.nombre);
        tooltip += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━<br>";
        tooltip += QString("📋 <b>INFORMACIÓN BÁSICA</b><br>");
        tooltip += QString("• ID: %1<br>").arg(puntoId);
        tooltip += QString("• Posición: %1, %2<br>").arg(item.pos.latitude(), 0, 'f', 6).arg(item.pos.longitude(), 0, 'f', 6);
        tooltip += QString("• Altura: %1 m<br>").arg(item.altura, 0, 'f', 1);
        tooltip += QString("• Velocidad: %1 m/s<br>").arg(item.velocidad, 0, 'f', 1);
        tooltip += QString("• Radio de giro: %1 m<br>").arg(item.radio, 0, 'f', 1);
        tooltip += QString("• Distancia al anterior: %1 m<br>").arg(item.distanciaAnterior, 0, 'f', 1);
        if (item.tasaAscenso != 0) {
            QString direccion = (item.tasaAscenso > 0) ? "↑ ASCENSO" : "↓ DESCENSO";
            tooltip += QString("• Tasa vertical: %1 m/s (%2)<br>").arg(abs(item.tasaAscenso), 0, 'f', 2).arg(direccion);
        }
        tooltip += "<br>";

        if (erroresPorPunto.contains(puntoId)) {
            tooltip += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━<br>";
            tooltip += "🔴 <b><font color='red'>ERRORES DETECTADOS</font></b><br>";
            QStringList errores = erroresPorPunto[puntoId].split("\n");
            for (const QString &err : errores) {
                if (!err.trimmed().isEmpty())
                    tooltip += QString("• %1<br>").arg(err);
            }
            tooltip += "<br>";
        }
        if (!item.advertencias.isEmpty()) {
            tooltip += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━<br>";
            tooltip += "⚠ <b><font color='orange'>ADVERTENCIAS</font></b><br>";
            for (const QString &adv : item.advertencias)
                tooltip += QString("• %1<br>").arg(adv.split("\n").first());
            if (item.advertencias.size() > 1)
                tooltip += QString("<i>... y %1 advertencia(s) más</i><br>").arg(item.advertencias.size() - 1);
            tooltip += "<br>";
        }
        if (item.puntoCritico) {
            tooltip += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━<br>";
            tooltip += "🔴 <b><font color='red'>PUNTO CRÍTICO</font></b><br>";
            tooltip += "<i>Este punto requiere atención especial</i><br>";
            tooltip += "Posibles problemas:<br>";
            tooltip += "• Giro muy cerrado<br>";
            tooltip += "• Cambio brusco de velocidad<br>";
            tooltip += "• Pendiente pronunciada<br>";
            tooltip += "• Radio de giro insuficiente<br><br>";
        }
        tooltip += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━<br>";
        tooltip += "💡 <b>RECOMENDACIONES</b><br>";
        if (item.velocidad > getDroneCharacteristics().maxSpeed * 0.8)
            tooltip += "• Reduzca velocidad para ahorrar energía<br>";
        if (item.altura < getDroneCharacteristics().minAltitude + 10)
            tooltip += "• Considere aumentar altura para mayor seguridad<br>";
        if (item.radio < 10 && getDroneCharacteristics().type == DroneType::FIXED_WING)
            tooltip += "• Aumente radio de giro para ala fija<br>";
        return tooltip;
    }
    if (role == Qt::BackgroundRole) {
        if (erroresPorPunto.contains(puntoId)) {
            QLinearGradient gradient(0, 0, 1, 0);
            gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
            gradient.setColorAt(0, QColor(255, 230, 230));
            gradient.setColorAt(1, QColor(255, 200, 200));
            return QBrush(gradient);
        }
        if (item.puntoCritico) {
            QLinearGradient gradient(0, 0, 1, 0);
            gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
            gradient.setColorAt(0, QColor(255, 220, 200));
            gradient.setColorAt(1, QColor(255, 180, 150));
            return QBrush(gradient);
        }
        if (!item.advertencias.isEmpty()) {
            QLinearGradient gradient(0, 0, 1, 0);
            gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
            gradient.setColorAt(0, QColor(255, 255, 230));
            gradient.setColorAt(1, QColor(255, 255, 200));
            return QBrush(gradient);
        }
        return QVariant();
    }
    if (role == Qt::ForegroundRole) {
        if (erroresPorPunto.contains(puntoId) || item.puntoCritico)
            return QBrush(QColor(200, 0, 0));
        if (!item.advertencias.isEmpty())
            return QBrush(QColor(150, 100, 0));
        return QVariant();
    }
    // Sin FontRole: la negrita de las filas con errores (y su QFont() con la
    // letra de la aplicacion, no la de la tabla) ensanchaba las columnas y la
    // tabla se comia el perfil. El fondo rojo y el icono ya lo marcan.
    // Numeros a la derecha (se comparan de un vistazo); nombre y modo, centrados.
    if (role == Qt::TextAlignmentRole)
        return int(index.column() >= 3 && index.column() <= 8 ? Qt::AlignRight | Qt::AlignVCenter
                                                              : Qt::AlignCenter);
    if (role == Qt::DecorationRole && index.column() == 9) {
        // Iconos del estilo (los :/icons/... de antes no existian en ningun .qrc).
        QStyle *estilo = QApplication::style();
        if (erroresPorPunto.contains(puntoId) || item.puntoCritico)
            return estilo->standardIcon(QStyle::SP_MessageBoxCritical);
        if (!item.advertencias.isEmpty())
            return estilo->standardIcon(QStyle::SP_MessageBoxWarning);
        return estilo->standardIcon(QStyle::SP_DialogApplyButton);
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
    int puntoId = item.id;
    bool cambioAceptado = false;

    switch (index.column()) {
    case 1: item.nombre = value.toString(); cambioAceptado = true; break;
    case 2: item.modo = value.toInt(); cambioAceptado = true; break;
    case 3: item.pos.setLatitude(value.toDouble()); cambioAceptado = true; break;
    case 4: item.pos.setLongitude(value.toDouble()); cambioAceptado = true; break;
    case 5: return false; // no editable
    case 6: item.altura = value.toDouble(); cambioAceptado = true; break;
    case 7: item.velocidad = value.toDouble(); cambioAceptado = true; break;
    case 8: item.radio = value.toDouble(); cambioAceptado = true; break;
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

// Mueve un punto (posicion y, si llega, altura) con un solo guardado y una
// sola revalidacion: el arrastre en el mapa no debe repintar la ruta tres veces.
bool MiModelo::moverPunto(int fila, const QGeoCoordinate &pos, double altura)
{
    if (fila < 0 || fila >= listaDatos.size() || !pos.isValid())
        return false;

    E_Punto &item = listaDatos[fila];
    item.pos.setLatitude(pos.latitude());
    item.pos.setLongitude(pos.longitude());
    if (!std::isnan(altura))
        item.altura = altura;

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
    if (index.column() == 0 || index.column() == 5 || index.column() == 9)
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
        case 7: return "Vel. (m/s)";
        case 8: return "Radio (m)";
        case 9: return "";
        default: return QVariant();
        }
    }
    if (role == Qt::ToolTipRole) {
        switch (section) {
        case 5: return "Distancia desde el punto anterior, en metros";
        case 6: return "Altura de vuelo, en metros";
        case 7: return "Velocidad en m/s, la que vuela el dron (en pantalla se ve en km/h: x 3,6)";
        case 8: return "Radio de giro en el punto, en metros";
        case 9: return "Estado del punto: correcto, con advertencias o con errores";
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

/**
 * @brief Revalida completamente la ruta: recalcula distancias, errores, advertencias y puntos críticos.
 *        Emite dataChanged para toda la tabla.
 */
void MiModelo::revalidarRutaCompleta()
{
    if (!isDroneConfigured()) return;

    erroresPorPunto.clear();
    for (auto &punto : listaDatos) {
        punto.advertencias.clear();
        punto.puntoCritico = false;
    }

    for (int i = 0; i < listaDatos.size(); ++i) {
        if (i > 0)
            listaDatos[i].distanciaAnterior = listaDatos[i-1].distanciaA(listaDatos[i]);
        else
            listaDatos[i].distanciaAnterior = 0.0;
    }

    for (int i = 0; i < listaDatos.size(); ++i) {
        E_Punto &punto = listaDatos[i];
        QStringList erroresPunto, advertenciasPunto;
        QString error;

        if (!planner->validarAltitud(punto, error)) erroresPunto.append(error);
        if (!planner->validarVelocidadPunto(punto, error)) erroresPunto.append(error);
        if (!planner->validarRadioGiro(punto, error)) advertenciasPunto.append(error);

        if (i < listaDatos.size() - 1) {
            double distancia = punto.distanciaA(listaDatos[i+1]);
            if (!planner->validarDistanciaWaypoint(punto, listaDatos[i+1], error))
                erroresPunto.append(QString("Distancia con %1: %2").arg(listaDatos[i+1].nombre).arg(error));
            if (!planner->validarTransicionVelocidad(punto, listaDatos[i+1], distancia, error))
                advertenciasPunto.append(QString("Transición con %1: %2").arg(listaDatos[i+1].nombre).arg(error));
            // AÑADIDO: validar transición de altitud como error
            if (!planner->validarTransicionAltitud(punto, listaDatos[i+1], distancia, error))
                erroresPunto.append(QString("Transición altitud con %1: %2").arg(listaDatos[i+1].nombre).arg(error));
        }
        if (i > 0)
            punto.tasaAscenso = punto.calcularTasaVertical(listaDatos[i-1]);

        if (!erroresPunto.isEmpty())
            erroresPorPunto[punto.id] = erroresPunto.join("\n");
        if (!advertenciasPunto.isEmpty())
            punto.advertencias = advertenciasPunto;
    }

    if (listaDatos.size() >= 3) {
        for (int i = 1; i < listaDatos.size() - 1; ++i) {
            const E_Punto &previo = listaDatos[i-1];
            E_Punto &actual = listaDatos[i];
            const E_Punto &siguiente = listaDatos[i+1];
            double angulo = planner->calcularAnguloGiro(previo, actual, siguiente);
            double cambioVelocidad = abs(siguiente.velocidad - previo.velocidad);
            if (angulo > 90.0 || cambioVelocidad > droneChars.maxAcceleration * 1.5) {
                actual.puntoCritico = true;
                if (!actual.advertencias.contains("Punto crítico detectado"))
                    actual.advertencias.append("Punto crítico detectado");
            }
        }
    }

    if (!listaDatos.isEmpty())
        emit dataChanged(index(0, 0), index(rowCount()-1, columnCount()-1));
    emit rutaModificada();
    emit analisisActualizado(analizarRutaDetallada());
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

/**
 * @brief Limpia los errores y advertencias de un punto y sus adyacentes.
 * @param puntoId ID del punto central.
 */
void MiModelo::limpiarErroresAdyacentes(int puntoId)
{
    int index = -1;
    for (int i = 0; i < listaDatos.size(); ++i) {
        if (listaDatos[i].id == puntoId) {
            index = i;
            break;
        }
    }
    if (index != -1) {
        erroresPorPunto.remove(puntoId);
        listaDatos[index].advertencias.clear();
        listaDatos[index].puntoCritico = false;
        if (index > 0) {
            erroresPorPunto.remove(listaDatos[index-1].id);
            listaDatos[index-1].advertencias.clear();
            listaDatos[index-1].puntoCritico = false;
        }
        if (index < listaDatos.size() - 1) {
            erroresPorPunto.remove(listaDatos[index+1].id);
            listaDatos[index+1].advertencias.clear();
            listaDatos[index+1].puntoCritico = false;
        }
    }
}
