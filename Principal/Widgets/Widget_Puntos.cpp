// ============================================================================
// Widget_Puntos.cpp
// ============================================================================

#include "Widget_Puntos.h"
#include "dronedialog.h"
#include "cmapaplot.h"  // necesario para onPointMoved

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QSqlQuery>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QClipboard>
#include <QApplication>
#include <QProgressDialog>
#include <limits>

// ============================================================================
// IMPLEMENTACIÓN DE RoutePlanner (SIN OPTIMIZACIÓN)
// ============================================================================

/**
 * @brief Constructor del planificador de rutas.
 * @param chars Características del dron que se usarán para las validaciones.
 */
RoutePlanner::RoutePlanner(const DroneCharacteristics &chars)
    : droneChars(chars), g(9.81), densidadAire(1.225)
{
}

/**
 * @brief Valida que la distancia entre dos waypoints esté dentro de los límites del dron.
 * @param wp1 Primer waypoint.
 * @param wp2 Segundo waypoint.
 * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
 * @return true si la distancia es válida, false en caso contrario.
 */
bool RoutePlanner::validarDistanciaWaypoint(const E_Punto &wp1, const E_Punto &wp2, QString &errorMsg) const
{
    double distancia = wp1.distanciaA(wp2);
    if (distancia > droneChars.maxWaypointDistance) {
        errorMsg = QString("Distancia entre %1 y %2 es %3 m, excede el máximo de %4 m")
                .arg(wp1.nombre).arg(wp2.nombre)
                .arg(distancia, 0, 'f', 1)
                .arg(droneChars.maxWaypointDistance);
        return false;
    }
    if (distancia < droneChars.minWaypointDistance) {
        errorMsg = QString("Distancia entre %1 y %2 es %3 m, menor al mínimo de %4 m")
                .arg(wp1.nombre).arg(wp2.nombre)
                .arg(distancia, 0, 'f', 1)
                .arg(droneChars.minWaypointDistance);
        return false;
    }
    return true;
}

/**
 * @brief Valida que la altitud de un punto esté dentro del rango permitido.
 * @param punto Punto a validar.
 * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
 * @return true si la altitud es válida, false en caso contrario.
 */
bool RoutePlanner::validarAltitud(const E_Punto &punto, QString &errorMsg) const
{
    if (punto.altura < droneChars.minAltitude) {
        errorMsg = QString("Altitud de %1 es %2 m, menor al mínimo de %3 m")
                .arg(punto.nombre).arg(punto.altura, 0, 'f', 1)
                .arg(droneChars.minAltitude);
        return false;
    }
    if (punto.altura > droneChars.maxAltitude) {
        errorMsg = QString("Altitud de %1 es %2 m, mayor al máximo de %3 m")
                .arg(punto.nombre).arg(punto.altura, 0, 'f', 1)
                .arg(droneChars.maxAltitude);
        return false;
    }
    return true;
}

/**
 * @brief Valida que la velocidad en un punto esté dentro de los límites del dron.
 * @param punto Punto a validar.
 * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
 * @return true si la velocidad es válida, false en caso contrario.
 */
bool RoutePlanner::validarVelocidadPunto(const E_Punto &punto, QString &errorMsg) const
{
    if (punto.velocidad > droneChars.maxSpeed) {
        errorMsg = QString("Velocidad en %1 (%2 m/s) excede máximo (%3 m/s)")
                .arg(punto.nombre).arg(punto.velocidad, 0, 'f', 1)
                .arg(droneChars.maxSpeed, 0, 'f', 1);
        return false;
    }
    if (droneChars.type == DroneType::FIXED_WING && punto.velocidad < droneChars.minSpeed) {
        errorMsg = QString("Velocidad en %1 (%2 m/s) menor al mínimo (%3 m/s) para ala fija")
                .arg(punto.nombre).arg(punto.velocidad, 0, 'f', 1)
                .arg(droneChars.minSpeed, 0, 'f', 1);
        return false;
    }
    if (droneChars.type == DroneType::QUADCOPTER && punto.velocidad < 0) {
        errorMsg = QString("Velocidad en %1 no puede ser negativa").arg(punto.nombre);
        return false;
    }
    return true;
}

/**
 * @brief Valida la transición de velocidad entre dos waypoints, comprobando que la aceleración no supere los límites.
 * @param actual Waypoint actual.
 * @param siguiente Siguiente waypoint.
 * @param distancia Distancia entre ellos.
 * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
 * @return true si la transición es válida, false en caso contrario.
 */
bool RoutePlanner::validarTransicionVelocidad(const E_Punto &actual, const E_Punto &siguiente,
                                              double distancia, QString &errorMsg) const
{
    double cambioVelocidad = abs(siguiente.velocidad - actual.velocidad);
    if (cambioVelocidad < 0.01) return true;

    double velocidadPromedio = (actual.velocidad + siguiente.velocidad) / 2.0;
    if (velocidadPromedio < 0.01) return true;

    double tiempoEstimado = distancia / velocidadPromedio;
    if (tiempoEstimado < 0.01) return true;

    double aceleracionNecesaria = cambioVelocidad / tiempoEstimado;
    bool esAceleracion = siguiente.velocidad > actual.velocidad;
    double aceleracionMaxima = esAceleracion ? droneChars.maxAcceleration : droneChars.maxDeceleration;

    if (aceleracionNecesaria > aceleracionMaxima) {
        QString tipo = esAceleracion ? "aceleración" : "desaceleración";
        errorMsg = QString("Transición %1→%2 requiere %3 de %4 m/s², máximo: %5 m/s²")
                .arg(actual.nombre).arg(siguiente.nombre).arg(tipo)
                .arg(aceleracionNecesaria, 0, 'f', 2)
                .arg(aceleracionMaxima, 0, 'f', 2);
        return false;
    }
    return true;
}

/**
 * @brief Valida la transición de altitud entre dos waypoints, comprobando pendiente y tasa vertical.
 * @param actual Waypoint actual.
 * @param siguiente Siguiente waypoint.
 * @param distancia Distancia entre ellos.
 * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
 * @return true si la transición es válida, false en caso contrario.
 */
bool RoutePlanner::validarTransicionAltitud(const E_Punto &actual, const E_Punto &siguiente,
                                            double distancia, QString &errorMsg) const
{
    double diferenciaAltura = siguiente.altura - actual.altura;
    if (abs(diferenciaAltura) < 0.1) return true;

    double pendiente = actual.calcularPendiente(siguiente);
    if (abs(pendiente) > 30.0) {
        errorMsg = QString("Pendiente entre %1 y %2 es %3°, máximo 30°")
                .arg(actual.nombre).arg(siguiente.nombre)
                .arg(pendiente, 0, 'f', 1);
        return false;
    }

    double velocidadPromedio = (actual.velocidad + siguiente.velocidad) / 2.0;
    if (velocidadPromedio < 0.01) return true;

    double tiempoEstimado = distancia / velocidadPromedio;
    if (tiempoEstimado < 0.01) return true;

    double tasaVertical = abs(diferenciaAltura) / tiempoEstimado;
    double tasaMaxima = (diferenciaAltura > 0) ? droneChars.maxClimbRate : droneChars.maxDescentRate;

    if (tasaVertical > tasaMaxima) {
        QString tipo = (diferenciaAltura > 0) ? "ascenso" : "descenso";
        errorMsg = QString("Transición %1→%2 requiere tasa de %3 de %4 m/s, máximo: %5 m/s")
                .arg(actual.nombre).arg(siguiente.nombre).arg(tipo)
                .arg(tasaVertical, 0, 'f', 2)
                .arg(tasaMaxima, 0, 'f', 2);
        return false;
    }
    return true;
}

/**
 * @brief Valida que el radio de giro en un punto sea suficiente para la velocidad y tipo de dron.
 * @param punto Punto a validar.
 * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
 * @return true si el radio es válido, false en caso contrario.
 */
bool RoutePlanner::validarRadioGiro(const E_Punto &punto, QString &errorMsg) const
{
    if (droneChars.type == DroneType::FIXED_WING) {
        double radioMinimoTeorico = calcularRadioMinimoTeorico(punto.velocidad);
        if (punto.radio < droneChars.minTurnRadius) {
            errorMsg = QString("Radio de giro en %1 es %2 m, menor al mínimo configurado %3 m")
                    .arg(punto.nombre).arg(punto.radio, 0, 'f', 1)
                    .arg(droneChars.minTurnRadius, 0, 'f', 1);
            return false;
        }
        if (punto.radio < radioMinimoTeorico) {
            errorMsg = QString("Radio de giro en %1 es %2 m, menor al mínimo teórico %3 m para velocidad %4 m/s")
                    .arg(punto.nombre).arg(punto.radio, 0, 'f', 1)
                    .arg(radioMinimoTeorico, 0, 'f', 1)
                    .arg(punto.velocidad, 0, 'f', 1);
            return false;
        }
        if (punto.velocidad > 0) {
            double radioRecomendado = calcularRadioGiroRecomendado(punto.velocidad);
            if (punto.radio < radioRecomendado * 0.8) {
                errorMsg = QString("Radio de giro en %1 es muy pequeño para velocidad %2 m/s. Recomendado: %3 m")
                        .arg(punto.nombre).arg(punto.velocidad, 0, 'f', 1)
                        .arg(radioRecomendado, 0, 'f', 1);
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Calcula el radio de giro mínimo teórico para una velocidad dada (solo para ala fija).
 * @param velocidad Velocidad en m/s.
 * @return Radio mínimo teórico en metros.
 */
double RoutePlanner::calcularRadioMinimoTeorico(double velocidad) const
{
    if (velocidad < 0.1) return 0.0;
    return (velocidad * velocidad) / (g * 0.577);
}

/**
 * @brief Calcula el radio de giro recomendado (1.5 veces el mínimo teórico).
 * @param velocidad Velocidad en m/s.
 * @return Radio recomendado en metros.
 */
double RoutePlanner::calcularRadioGiroRecomendado(double velocidad) const
{
    return calcularRadioMinimoTeorico(velocidad) * 1.5;
}

/**
 * @brief Calcula la distancia total de la ruta sumando las distancias entre waypoints consecutivos.
 * @param puntos Lista de puntos de la ruta.
 * @return Distancia total en metros.
 */
double RoutePlanner::calcularDistanciaTotal(const QList<E_Punto> &puntos) const
{
    double total = 0;
    for (int i = 0; i < puntos.size() - 1; ++i)
        total += puntos[i].distanciaA(puntos[i + 1]);
    return total;
}

/**
 * @brief Calcula el tiempo total de vuelo y la energía estimada para la ruta.
 * @param puntos Lista de puntos de la ruta.
 * @return Par (tiempo en segundos, energía en julios).
 */
QPair<double, double> RoutePlanner::calcularTiempoVuelo(const QList<E_Punto> &puntos) const
{
    double tiempoTotal = 0;
    double energiaTotal = 0;
    for (int i = 0; i < puntos.size() - 1; ++i) {
        const E_Punto &actual = puntos[i];
        const E_Punto &siguiente = puntos[i + 1];
        double distancia = actual.distanciaA(siguiente);
        double velocidadPromedio = (actual.velocidad + siguiente.velocidad) / 2.0;
        if (velocidadPromedio > 0.01) {
            double tiempoSegmento = distancia / velocidadPromedio;
            tiempoTotal += tiempoSegmento;
            double potenciaEstimada = estimarPotencia(actual, siguiente, distancia);
            energiaTotal += potenciaEstimada * tiempoSegmento;
        } else {
            double potenciaHovering = droneChars.type == DroneType::QUADCOPTER ? 200.0 : 0.0;
            double tiempoEstimado = distancia > 0 ? distancia / 0.1 : 1.0;
            tiempoTotal += tiempoEstimado;
            energiaTotal += potenciaHovering * tiempoEstimado;
        }
    }
    return qMakePair(tiempoTotal, energiaTotal);
}

/**
 * @brief Estima la potencia requerida para un segmento de vuelo.
 * @param p1 Punto de inicio.
 * @param p2 Punto de fin.
 * @param distancia Distancia del segmento.
 * @return Potencia estimada en vatios.
 */
double RoutePlanner::estimarPotencia(const E_Punto &p1, const E_Punto &p2, double distancia) const
{
    double velocidadPromedio = (p1.velocidad + p2.velocidad) / 2.0;
    double diferenciaAltura = p2.altura - p1.altura;
    double potenciaBase = (droneChars.type == DroneType::QUADCOPTER) ? 150.0 : 300.0;
    double factorVelocidad = 1.0 + (velocidadPromedio / droneChars.maxSpeed) * 0.5;
    double factorAltitud = 1.0;
    if (distancia > 0) {
        double tasaVertical = diferenciaAltura / (distancia / velocidadPromedio);
        if (tasaVertical > 0)
            factorAltitud += tasaVertical * 0.1;
        else if (tasaVertical < 0)
            factorAltitud -= tasaVertical * 0.05;
    }
    return potenciaBase * factorVelocidad * factorAltitud;
}

/**
 * @brief Estima el consumo de energía total de la ruta en vatios-hora, con un factor de seguridad.
 * @param puntos Lista de puntos de la ruta.
 * @return Consumo estimado en Wh.
 */
double RoutePlanner::estimarConsumoEnergia(const QList<E_Punto> &puntos) const
{
    auto resultado = calcularTiempoVuelo(puntos);
    double energiaTotal = resultado.second;
    double consumoWh = energiaTotal / 3600.0;
    double factorSeguridad = droneChars.type == DroneType::QUADCOPTER ? 1.2 : 1.3;
    return consumoWh * factorSeguridad;
}

/**
 * @brief Valida la ruta completa contra todas las restricciones del dron.
 * @param puntos Lista de puntos de la ruta.
 * @return Par (válido, mensaje de error si no lo es).
 */
QPair<bool, QString> RoutePlanner::validarRuta(const QList<E_Punto> &puntos) const
{
    if (puntos.size() < 2)
        return qMakePair(false, QStringLiteral("La ruta debe tener al menos 2 puntos"));

    for (const auto &punto : puntos) {
        QString error;
        if (!validarAltitud(punto, error))
            return qMakePair(false, error);
        if (!validarVelocidadPunto(punto, error))
            return qMakePair(false, error);
        if (!validarRadioGiro(punto, error))
            return qMakePair(false, error);
    }

    for (int i = 0; i < puntos.size() - 1; ++i) {
        QString error;
        double distancia = puntos[i].distanciaA(puntos[i + 1]);
        if (!validarDistanciaWaypoint(puntos[i], puntos[i + 1], error))
            return qMakePair(false, error);
        if (!validarTransicionVelocidad(puntos[i], puntos[i + 1], distancia, error))
            return qMakePair(false, error);
        if (!validarTransicionAltitud(puntos[i], puntos[i + 1], distancia, error))
            return qMakePair(false, error);
    }

    double distanciaTotal = calcularDistanciaTotal(puntos);
    if (distanciaTotal > droneChars.maxRange) {
        return qMakePair(false, QStringLiteral("Distancia total %1 m excede autonomía máxima %2 m")
                         .arg(distanciaTotal, 0, 'f', 1).arg(droneChars.maxRange));
    }

    auto tiempoVuelo = calcularTiempoVuelo(puntos);
    if (tiempoVuelo.first > droneChars.endurance) {
        return qMakePair(false, QStringLiteral("Tiempo de vuelo estimado %1 s excede endurance %2 s")
                         .arg(tiempoVuelo.first, 0, 'f', 0).arg(droneChars.endurance));
    }

    if (puntos.size() > 3) {
        QString error = validarPerfilCompleto(puntos);
        if (!error.isEmpty())
            return qMakePair(false, error);
    }

    return qMakePair(true, QStringLiteral("✓ Ruta válida"));
}

/**
 * @brief Realiza validaciones adicionales sobre el perfil completo de la ruta (cambios bruscos, etc.).
 * @param puntos Lista de puntos de la ruta.
 * @return Cadena de error vacía si todo es correcto, o mensaje descriptivo.
 */
QString RoutePlanner::validarPerfilCompleto(const QList<E_Punto> &puntos) const
{
    int cambiosBruscosVelocidad = 0;
    int cambiosBruscosAltitud = 0;

    for (int i = 1; i < puntos.size() - 1; ++i) {
        const E_Punto &anterior = puntos[i-1];
        const E_Punto &actual = puntos[i];
        const E_Punto &siguiente = puntos[i+1];

        double cambio1 = abs(actual.velocidad - anterior.velocidad);
        double cambio2 = abs(siguiente.velocidad - actual.velocidad);
        if (cambio1 > droneChars.maxAcceleration * 0.7 && cambio2 > droneChars.maxAcceleration * 0.7)
            cambiosBruscosVelocidad++;

        double pendiente1 = anterior.calcularPendiente(actual);
        double pendiente2 = actual.calcularPendiente(siguiente);
        if (abs(pendiente1) > 20 && abs(pendiente2) > 20)
            cambiosBruscosAltitud++;
    }

    QStringList errores;
    if (cambiosBruscosVelocidad > 1)
        errores.append("Demasiados cambios bruscos de velocidad en la ruta");
    if (cambiosBruscosAltitud > 1)
        errores.append("Demasiados cambios bruscos de altitud en la ruta");

    if (droneChars.type == DroneType::FIXED_WING) {
        double velocidadPromedio = 0;
        for (const auto &punto : puntos)
            velocidadPromedio += punto.velocidad;
        velocidadPromedio /= puntos.size();
        double variacionTotal = 0;
        for (const auto &punto : puntos)
            variacionTotal += abs(punto.velocidad - velocidadPromedio);
        double variacionPromedio = variacionTotal / puntos.size();
        if (variacionPromedio > velocidadPromedio * 0.4) {
            errores.append(QString("Variación de velocidad demasiado alta para ala fija (%1%)")
                           .arg((variacionPromedio / velocidadPromedio) * 100, 0, 'f', 0));
        }
    }

    return errores.join("; ");
}

/**
 * @brief Realiza un análisis detallado de la ruta, devolviendo múltiples métricas y puntos críticos.
 * @param puntos Lista de puntos de la ruta.
 * @return Mapa con todos los datos del análisis (distancia, tiempo, energía, puntos críticos, etc.).
 */
QVariantMap RoutePlanner::analizarRutaDetallada(const QList<E_Punto> &puntos) const
{
    QVariantMap analisis;
    if (puntos.isEmpty()) {
        analisis["error"] = "Ruta vacía";
        return analisis;
    }

    analisis["num_puntos"] = puntos.size();
    double distanciaTotal = calcularDistanciaTotal(puntos);
    analisis["distancia_total"] = distanciaTotal;

    auto tiempoEnergia = calcularTiempoVuelo(puntos);
    analisis["tiempo_vuelo"] = tiempoEnergia.first;
    analisis["energia_estimada"] = tiempoEnergia.second;
    analisis["consumo_wh"] = estimarConsumoEnergia(puntos);

    double velocidadMin = std::numeric_limits<double>::max();
    double velocidadMax = 0, velocidadSum = 0;
    for (const auto &punto : puntos) {
        velocidadMin = qMin(velocidadMin, (double)punto.velocidad);
        velocidadMax = qMax(velocidadMax, (double)punto.velocidad);
        velocidadSum += punto.velocidad;
    }
    analisis["velocidad_min"] = velocidadMin;
    analisis["velocidad_max"] = velocidadMax;
    analisis["velocidad_promedio"] = velocidadSum / puntos.size();

    double altitudMin = std::numeric_limits<double>::max();
    double altitudMax = 0, altitudSum = 0, ascensoTotal = 0, descensoTotal = 0;
    for (int i = 0; i < puntos.size(); ++i) {
        altitudMin = qMin(altitudMin, (double)puntos[i].altura);
        altitudMax = qMax(altitudMax, (double)puntos[i].altura);
        altitudSum += puntos[i].altura;
        if (i > 0) {
            double diferencia = puntos[i].altura - puntos[i-1].altura;
            if (diferencia > 0) ascensoTotal += diferencia;
            else descensoTotal += abs(diferencia);
        }
    }
    analisis["altitud_min"] = altitudMin;
    analisis["altitud_max"] = altitudMax;
    analisis["altitud_promedio"] = altitudSum / puntos.size();
    analisis["ascenso_total"] = ascensoTotal;
    analisis["descenso_total"] = descensoTotal;
    analisis["desnivel_total"] = ascensoTotal + descensoTotal;

    double distanciaLineaRecta = puntos.first().distanciaA(puntos.last());
    double eficiencia = (distanciaLineaRecta / distanciaTotal) * 100;
    analisis["eficiencia_ruta"] = eficiencia;
    analisis["distancia_linea_recta"] = distanciaLineaRecta;

    QVariantList puntosCriticos = detectarPuntosCriticos(puntos);
    analisis["puntos_criticos"] = puntosCriticos;
    analisis["num_puntos_criticos"] = puntosCriticos.size();

    QVariantList analisisSegmentos = analizarSegmentos(puntos);
    analisis["segmentos"] = analisisSegmentos;

    auto validacion = validarRuta(puntos);
    analisis["valida"] = validacion.first;
    analisis["mensaje_validacion"] = validacion.second;

    double margenSeguridad = calcularMargenSeguridad(puntos);
    analisis["margen_seguridad"] = margenSeguridad;
    analisis["nivel_seguridad"] = clasificarNivelSeguridad(margenSeguridad);
    analisis["factor_carga_max"] = calcularFactorCargaMaximo(puntos);
    analisis["estres_estimado"] = calcularEstresRuta(puntos);

    return analisis;
}

/**
 * @brief Detecta puntos críticos en la ruta (giros cerrados, cambios bruscos, etc.).
 * @param puntos Lista de puntos de la ruta.
 * @return Lista de mapas con información detallada de cada punto crítico.
 */
QVariantList RoutePlanner::detectarPuntosCriticos(const QList<E_Punto> &puntos) const
{
    QVariantList criticos;
    for (int i = 1; i < puntos.size() - 1; ++i) {
        const E_Punto &previo = puntos[i-1];
        const E_Punto &actual = puntos[i];
        const E_Punto &siguiente = puntos[i+1];

        QVariantMap puntoCritico;
        QStringList problemas, explicaciones, soluciones;

        double angulo = calcularAnguloGiro(previo, actual, siguiente);
        if (angulo > 90.0) {
            problemas.append("Giro muy cerrado");
            explicaciones.append(QString("Ángulo de giro: %1° (límite: 90°)").arg(angulo, 0, 'f', 1));
            soluciones.append("Aumente el radio de giro o reubique los puntos");
        } else if (angulo > 60.0) {
            problemas.append("Giro moderado");
            explicaciones.append(QString("Ángulo de giro: %1°").arg(angulo, 0, 'f', 1));
            soluciones.append("Considere suavizar la trayectoria");
        }

        double cambioVelocidad = abs(siguiente.velocidad - previo.velocidad);
        if (cambioVelocidad > droneChars.maxAcceleration * 1.5) {
            problemas.append("Cambio de velocidad brusco");
            explicaciones.append(QString("Cambio de %1 a %2 m/s (%3 m/s) en %4 m")
                                 .arg(previo.velocidad, 0, 'f', 1)
                                 .arg(siguiente.velocidad, 0, 'f', 1)
                                 .arg(cambioVelocidad, 0, 'f', 1)
                                 .arg(previo.distanciaA(siguiente), 0, 'f', 1));
            soluciones.append(QString("Ajuste velocidad del punto %1 a %2 m/s")
                              .arg(actual.nombre).arg((previo.velocidad + siguiente.velocidad) / 2.0, 0, 'f', 1));
        }

        double pendiente1 = previo.calcularPendiente(actual);
        double pendiente2 = actual.calcularPendiente(siguiente);
        if (abs(pendiente1) > 25 || abs(pendiente2) > 25) {
            problemas.append("Pendiente pronunciada");
            explicaciones.append(QString("Pendientes: %1° y %2°").arg(pendiente1, 0, 'f', 1).arg(pendiente2, 0, 'f', 1));
            soluciones.append("Reduzca pendiente añadiendo puntos intermedios");
        }

        if (angulo > 45.0) {
            double radioRequerido = calcularRadioMinimoTeorico(actual.velocidad);
            if (actual.radio < radioRequerido) {
                problemas.append("Radio de giro insuficiente");
                explicaciones.append(QString("Radio actual: %1 m, mínimo requerido: %2 m")
                                     .arg(actual.radio, 0, 'f', 1).arg(radioRequerido, 0, 'f', 1));
                soluciones.append(QString("Aumente radio a al menos %1 m").arg(radioRequerido * 1.5, 0, 'f', 1));
            }
        }

        if (!problemas.isEmpty()) {
            puntoCritico["punto"] = actual.nombre;
            puntoCritico["id"] = actual.id;
            puntoCritico["problemas"] = problemas;
            puntoCritico["explicaciones"] = explicaciones;
            puntoCritico["soluciones"] = soluciones;
            puntoCritico["nivel"] = clasificarCriticidad(problemas.size(), angulo, cambioVelocidad);
            puntoCritico["color"] = obtenerColorCriticidad(puntoCritico["nivel"].toString());
            criticos.append(puntoCritico);
        }
    }
    return criticos;
}

/**
 * @brief Obtiene el color hexadecimal correspondiente al nivel de criticidad.
 * @param nivel Nivel de criticidad ("CRÍTICO", "ALTO", "MODERADO", "BAJO").
 * @return Código de color en formato #RRGGBB.
 */
QString RoutePlanner::obtenerColorCriticidad(const QString &nivel) const
{
    if (nivel == "CRÍTICO") return "#FF0000";
    if (nivel == "ALTO") return "#FF6600";
    if (nivel == "MODERADO") return "#FFCC00";
    return "#00AA00";
}

/**
 * @brief Clasifica el nivel de criticidad de un punto basado en el número de problemas, ángulo y cambio de velocidad.
 * @param numProblemas Número de problemas detectados.
 * @param angulo Ángulo de giro.
 * @param cambioVelocidad Cambio de velocidad en el segmento.
 * @return Cadena con el nivel ("CRÍTICO", "ALTO", "MODERADO", "BAJO").
 */
QString RoutePlanner::clasificarCriticidad(int numProblemas, double angulo, double cambioVelocidad) const
{
    bool esAlaFija = (droneChars.type == DroneType::FIXED_WING);
    if (angulo > 120 || cambioVelocidad > droneChars.maxAcceleration * 2.0 ||
            (esAlaFija && angulo > 90) || numProblemas >= 3)
        return "CRÍTICO";
    if (angulo > 90 || cambioVelocidad > droneChars.maxAcceleration * 1.5 || numProblemas >= 2)
        return "ALTO";
    if (angulo > 60 || cambioVelocidad > droneChars.maxAcceleration || numProblemas >= 1)
        return "MODERADO";
    return "BAJO";
}

/**
 * @brief Clasifica el nivel de seguridad según el margen calculado.
 * @param margen Margen de seguridad en porcentaje.
 * @return Cadena con el nivel ("MUY ALTO", "ALTO", "MODERADO", "BAJO", "MUY BAJO").
 */
QString RoutePlanner::clasificarNivelSeguridad(double margen) const
{
    if (margen >= 70) return "MUY ALTO";
    if (margen >= 50) return "ALTO";
    if (margen >= 30) return "MODERADO";
    if (margen >= 15) return "BAJO";
    return "MUY BAJO";
}

/**
 * @brief Analiza cada segmento de la ruta y devuelve métricas por segmento.
 * @param puntos Lista de puntos de la ruta.
 * @return Lista de mapas con datos de cada segmento.
 */
QVariantList RoutePlanner::analizarSegmentos(const QList<E_Punto> &puntos) const
{
    QVariantList segmentos;
    for (int i = 0; i < puntos.size() - 1; ++i) {
        const E_Punto &inicio = puntos[i];
        const E_Punto &fin = puntos[i+1];
        QVariantMap segmento;
        segmento["nombre"] = QString("%1→%2").arg(inicio.nombre).arg(fin.nombre);
        segmento["distancia"] = inicio.distanciaA(fin);
        segmento["velocidad_promedio"] = (inicio.velocidad + fin.velocidad) / 2.0;
        segmento["diferencia_altitud"] = fin.altura - inicio.altura;
        segmento["pendiente"] = inicio.calcularPendiente(fin);
        segmento["tiempo_estimado"] = segmento["distancia"].toDouble() /
                qMax(0.1, segmento["velocidad_promedio"].toDouble());
        double dificultad = 0;
        if (abs(segmento["pendiente"].toDouble()) > 15) dificultad += 2;
        if (abs(inicio.velocidad - fin.velocidad) > droneChars.maxAcceleration * 0.5) dificultad += 1;
        if (segmento["distancia"].toDouble() > droneChars.maxWaypointDistance * 0.8) dificultad += 1;
        segmento["dificultad"] = dificultad;
        segmentos.append(segmento);
    }
    return segmentos;
}

/**
 * @brief Calcula el ángulo de giro entre tres puntos consecutivos.
 * @param p1 Punto anterior.
 * @param p2 Punto actual.
 * @param p3 Punto siguiente.
 * @return Ángulo en grados (0-180).
 */
double RoutePlanner::calcularAnguloGiro(const E_Punto &p1, const E_Punto &p2, const E_Punto &p3) const
{
    double v1x = p2.pos.longitude() - p1.pos.longitude();
    double v1y = p2.pos.latitude() - p1.pos.latitude();
    double v2x = p3.pos.longitude() - p2.pos.longitude();
    double v2y = p3.pos.latitude() - p2.pos.latitude();
    double dot = v1x * v2x + v1y * v2y;
    double mag1 = sqrt(v1x * v1x + v1y * v1y);
    double mag2 = sqrt(v2x * v2x + v2y * v2y);
    if (mag1 < 0.001 || mag2 < 0.001) return 0.0;
    double cosAngulo = dot / (mag1 * mag2);
    cosAngulo = qMax(-1.0, qMin(1.0, cosAngulo));
    return qRadiansToDegrees(acos(cosAngulo));
}

/**
 * @brief Calcula el margen de seguridad de la ruta en porcentaje.
 * @param puntos Lista de puntos de la ruta.
 * @return Margen de seguridad (0-100).
 */
double RoutePlanner::calcularMargenSeguridad(const QList<E_Punto> &puntos) const
{
    double margen = 100.0;
    double distanciaTotal = calcularDistanciaTotal(puntos);
    double porcentajeAutonomia = (distanciaTotal / droneChars.maxRange) * 100;
    margen -= porcentajeAutonomia * 0.6;

    auto tiempoVuelo = calcularTiempoVuelo(puntos);
    double porcentajeEndurance = (tiempoVuelo.first / droneChars.endurance) * 100;
    margen -= porcentajeEndurance * 0.4;

    QVariantList puntosCriticos = detectarPuntosCriticos(puntos);
    for (const auto &punto : puntosCriticos) {
        QVariantMap datos = punto.toMap();
        QString nivel = datos["nivel"].toString();
        if (nivel == "CRÍTICO") margen -= 15;
        else if (nivel == "ALTO") margen -= 10;
        else margen -= 5;
    }

    if (droneChars.type == DroneType::FIXED_WING) {
        double velocidadPromedio = 0;
        for (const auto &punto : puntos) velocidadPromedio += punto.velocidad;
        velocidadPromedio /= puntos.size();
        double variacionTotal = 0;
        for (const auto &punto : puntos) variacionTotal += abs(punto.velocidad - velocidadPromedio);
        double variacionPromedio = variacionTotal / puntos.size();
        if (variacionPromedio > velocidadPromedio * 0.3) margen -= 10.0;
    }

    int pendientesPronunciadas = 0;
    for (int i = 0; i < puntos.size() - 1; ++i) {
        double pendiente = puntos[i].calcularPendiente(puntos[i+1]);
        if (abs(pendiente) > 20) pendientesPronunciadas++;
    }
    margen -= pendientesPronunciadas * 2;

    return qMax(0.0, qMin(100.0, margen));
}

/**
 * @brief Calcula el factor de carga máximo experimentado por el dron en la ruta.
 * @param puntos Lista de puntos de la ruta.
 * @return Factor de carga máximo (adimensional, g's).
 */
double RoutePlanner::calcularFactorCargaMaximo(const QList<E_Punto> &puntos) const
{
    double factorMax = 1.0;
    for (int i = 0; i < puntos.size() - 1; ++i) {
        const E_Punto &actual = puntos[i];
        const E_Punto &siguiente = puntos[i+1];
        if (i > 0 && i < puntos.size() - 1) {
            double angulo = calcularAnguloGiro(puntos[i-1], actual, siguiente);
            if (angulo > 45) {
                double velocidadGiro = (actual.velocidad + siguiente.velocidad) / 2.0;
                double factorGiro = 1.0 / cos(qDegreesToRadians(qMin(60.0, angulo/2.0)));
                factorMax = qMax(factorMax, factorGiro);
            }
        }
        double pendiente = actual.calcularPendiente(siguiente);
        if (abs(pendiente) > 15) {
            double factorPendiente = 1.0 + abs(pendiente) / 45.0;
            factorMax = qMax(factorMax, factorPendiente);
        }
    }
    return factorMax;
}

/**
 * @brief Calcula un índice de estrés estimado para la ruta (0-100).
 * @param puntos Lista de puntos de la ruta.
 * @return Índice de estrés.
 */
double RoutePlanner::calcularEstresRuta(const QList<E_Punto> &puntos) const
{
    double estres = 0;
    for (int i = 0; i < puntos.size(); ++i) {
        double estresPunto = 0;
        estresPunto += puntos[i].velocidad / droneChars.maxSpeed * 0.3;
        if (i > 0) {
            double cambioVelocidad = abs(puntos[i].velocidad - puntos[i-1].velocidad);
            estresPunto += cambioVelocidad / droneChars.maxAcceleration * 0.2;
            double diferenciaAltura = puntos[i].altura - puntos[i-1].altura;
            double tasaMaxima = diferenciaAltura > 0 ? droneChars.maxClimbRate : droneChars.maxDescentRate;
            estresPunto += abs(diferenciaAltura) / (tasaMaxima * 10) * 0.2;
        }
        if (i > 0 && i < puntos.size() - 1) {
            double angulo = calcularAnguloGiro(puntos[i-1], puntos[i], puntos[i+1]);
            estresPunto += (angulo / 180.0) * 0.3;
        }
        estres += estresPunto;
    }
    return estres / puntos.size() * 100;
}

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
        case 3: return QString::number(item.pos.latitude(), 'f', 5);
        case 4: return QString::number(item.pos.longitude(), 'f', 5);
        case 5: return QString::number(item.distanciaAnterior, 'f', 2);
        case 6: return QString::number(item.altura, 'f', 2);
        case 7: return QString::number(item.velocidad, 'f', 2);
        case 8: return QString::number(item.radio, 'f', 2);
        case 9:
            if (erroresPorPunto.contains(puntoId)) return "❌";
            if (item.puntoCritico) return "🔴";
            if (!item.advertencias.isEmpty()) return "⚠";
            return "✓";
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
    if (role == Qt::FontRole) {
        QFont font;
        if (erroresPorPunto.contains(puntoId) || item.puntoCritico)
            font.setBold(true);
        else if (!item.advertencias.isEmpty())
            font.setBold(true);
        return font;
    }
    if (role == Qt::TextAlignmentRole) return Qt::AlignCenter;
    if (role == Qt::DecorationRole && index.column() == 9) {
        if (erroresPorPunto.contains(puntoId)) return QIcon(":/icons/error.png");
        if (item.puntoCritico) return QIcon(":/icons/critical.png");
        if (!item.advertencias.isEmpty()) return QIcon(":/icons/warning.png");
        return QIcon(":/icons/ok.png");
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
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return "ID";
        case 1: return "Nombre";
        case 2: return "Modo";
        case 3: return "Latitud";
        case 4: return "Longitud";
        case 5: return "Distancia (m)";
        case 6: return "Altura (m)";
        case 7: return "Velocidad (m/s)";
        case 8: return "Radio (m)";
        case 9: return "Estado";
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
    QDir RecursosDir = QDir::currentPath();
    RecursosDir.cdUp();
    QString dirFich = RecursosDir.path() + "/Recursos/data";
    RecursosDir.mkdir(dirFich);
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

// ============================================================================
// IMPLEMENTACIÓN DE Widget_Puntos (VERSIÓN COMPACTA, SIN .ui)
// ============================================================================

/**
 * @brief Constructor del widget principal de planificación de rutas.
 * @param alturaWorker Puntero al worker de alturas (para obtener elevación del terreno).
 * @param parent Widget padre.
 */
Widget_Puntos::Widget_Puntos(AlturaWorker *alturaWorker, QWidget *parent) :
    QWidget(parent),
    mdb(nullptr),
    m_modoSeleccionPuntoInicial(false),
    m_alturaWorker(alturaWorker)
{
    // Configuración de ventana
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);

    // Fondo del widget principal igual al de la barra de título para evitar contraste
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#2c3e50"));
    setPalette(pal);
    setAutoFillBackground(true);

    // Crear barra de título personalizada
    titleBar = new QWidget(this);
    titleBar->setFixedHeight(30);
    titleBar->setStyleSheet("background-color: #2c3e50; color: white; border: none;");
    titleBar->installEventFilter(this);

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(5, 0, 5, 0);

    QLabel *titleLabel = new QLabel("Planificador de Rutas");
    titleLabel->setStyleSheet("font-weight: bold;");

    m_closeButton = new QPushButton("✕");
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border: none; border-radius: 3px; }"
                                 "QPushButton:hover { background-color: #c0392b; }");
    connect(m_closeButton, &QPushButton::clicked, this, &Widget_Puntos::on_closeButtonClicked);

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeButton);

    // Crear el modelo y la referencia a la base de datos
    modelo = new MiModelo();
    mdb = new QSqlDatabase(QSqlDatabase::database());
    modelo->setAlturaWorker(m_alturaWorker);

    // Crear widget de perfil
    m_perfilWidget = new PerfilAltitudWidget(m_alturaWorker, this);
    m_perfilWidget->setWindowFlags(Qt::Dialog);
    m_perfilWidget->setWindowTitle("Perfil de altitud");
    m_perfilWidget->resize(600, 400);
    m_perfilWidget->hide();

    // ------------------------------------------------------------
    // CREAR TODOS LOS WIDGETS DE LA INTERFAZ (antes en .ui)
    // ------------------------------------------------------------
    // Combo de rutas
    cB_Rutas = new QComboBox(this);
    cB_Rutas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Tabla de puntos
    tw_puntosRuta = new QTableView(this);
    tw_puntosRuta->setModel(modelo);
    tw_puntosRuta->hideColumn(0);               // ID
    for (int i = 1; i < modelo->columnCount(); ++i) {
        tw_puntosRuta->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    }
    tw_puntosRuta->verticalHeader()->setDefaultSectionSize(20);
    tw_puntosRuta->setSelectionBehavior(QAbstractItemView::SelectRows);
    QFont tablaFont = tw_puntosRuta->font();
    tablaFont.setPointSize(9);
    tw_puntosRuta->setFont(tablaFont);

    // Etiquetas de métricas
    label_Distancia = new QLabel("0 m", this);
    label_AutonomiaRest = new QLabel("0%", this);
    label_Tiempo = new QLabel("--:--", this);
    progressAutonomia = new QProgressBar(this);
    progressAutonomia->setFixedWidth(60);
    progressAutonomia->setRange(0, 100);
    progressAutonomia->setTextVisible(true);
    progressAutonomia->setFormat("%p%");
    progressAutonomia->setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }");
    label_Estado = new QLabel("Configure una ruta primero", this);
    label_Estado->setStyleSheet("color: red; font-weight: bold;");
    labelInfoDron = new QLabel("No configurado", this);
    label_PuntosFijos = new QLabel("Ruta fija de 10 puntos (no se pueden añadir/eliminar)", this);
    label_PuntosFijos->setStyleSheet("color: gray; font-style: italic;");

    // Botones principales
    pB_CrearRuta = new QPushButton("Nueva", this);
    pB_EliminaRuta = new QPushButton("Eliminar", this);
    pB_LimpiaRuta = new QPushButton("Restablecer", this);
    pb_guardar = new QPushButton("Guardar", this);
    pB_EnviaRuta = new QPushButton("Enviar", this);
    pb_Mostrar = new QPushButton("Mostrar", this);
    pb_Mostrar->setCheckable(true);
    pB_AnalisisDetallado = new QPushButton("Análisis", this);
    pB_CancelarSeleccion = new QPushButton("Cancelar", this);
    pB_CancelarSeleccion->setVisible(false);

    // Botones ocultos (por compatibilidad con código existente)
    pB_EliminaPunto = new QPushButton("Eliminar punto", this);
    pB_EliminaPunto->setVisible(false);
    pB_Cargar = new QPushButton("Cargar", this);
    pB_Cargar->setVisible(false);
    pB_ExportarAnalisis = new QPushButton("Exportar análisis", this);
    pB_ExportarAnalisis->setVisible(false);

    // Botones adicionales
    m_btnPerfil = new QPushButton("Perfil", this);
    m_btnPerfil->setToolTip("Mostrar perfil de altitud y terreno");
    connect(m_btnPerfil, &QPushButton::clicked, this, &Widget_Puntos::mostrarPerfil);

    QPushButton *btnGuia = new QPushButton("Guía", this);
    btnGuia->setToolTip("Mostrar guía para establecer una ruta correcta");
    connect(btnGuia, &QPushButton::clicked, this, &Widget_Puntos::mostrarGuia);

    // Establecer ancho fijo para todos los botones
    int anchoBoton = 95;
    int altoBoton = 30;
    QList<QPushButton*> botones = {
        pB_CrearRuta, pB_EliminaRuta, pB_LimpiaRuta, pb_guardar, pB_EnviaRuta,
        pb_Mostrar, pB_AnalisisDetallado, m_btnPerfil, btnGuia, pB_CancelarSeleccion
    };
    for (QPushButton* btn : botones) {
        btn->setFixedWidth(anchoBoton);
        btn->setFixedHeight(altoBoton);
    }

    // ------------------------------------------------------------
    // CONSTRUIR EL LAYOUT PRINCIPAL
    // ------------------------------------------------------------
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    mainLayout->addWidget(titleBar);

    QWidget *contentWidget = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(2, 2, 2, 2);
    contentLayout->setSpacing(4);

    // ---- FILA SUPERIOR: selector de rutas + info dron ----
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    topRow->addWidget(new QLabel("Ruta:"));
    topRow->addWidget(cB_Rutas);
    topRow->addSpacing(10);
    topRow->addWidget(new QLabel("Dron:"));
    topRow->addWidget(labelInfoDron);
    topRow->addStretch();
    contentLayout->addLayout(topRow);

    // ---- TABLA DE PUNTOS ----
    contentLayout->addWidget(tw_puntosRuta, 1);

    // ---- FILA DE MÉTRICAS ----
    QHBoxLayout *metricsRow = new QHBoxLayout();
    metricsRow->setSpacing(4);
    metricsRow->addWidget(label_Distancia);
    metricsRow->addWidget(new QLabel("|"));
    metricsRow->addWidget(label_AutonomiaRest);
    metricsRow->addWidget(new QLabel("|"));
    metricsRow->addWidget(label_Tiempo);
    metricsRow->addSpacing(6);
    metricsRow->addWidget(progressAutonomia);
    metricsRow->addStretch();
    metricsRow->addWidget(label_Estado);
    contentLayout->addLayout(metricsRow);

    // ---- FILA DE BOTONES PRINCIPALES ----
    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(6);
    buttonRow->addWidget(pB_CrearRuta);
    buttonRow->addWidget(pB_EliminaRuta);
    buttonRow->addWidget(pB_LimpiaRuta);
    buttonRow->addWidget(pb_guardar);
    buttonRow->addWidget(pB_EnviaRuta);
    buttonRow->addWidget(pb_Mostrar);
    buttonRow->addWidget(pB_AnalisisDetallado);
    buttonRow->addWidget(pB_CancelarSeleccion);
    buttonRow->addWidget(m_btnPerfil);
    buttonRow->addWidget(btnGuia);
    buttonRow->addStretch();
    contentLayout->addLayout(buttonRow);

    // ---- ETIQUETA INFORMATIVA ----
    QHBoxLayout *infoRow = new QHBoxLayout();
    infoRow->setContentsMargins(0, 0, 0, 0);
    infoRow->addStretch();
    infoRow->addWidget(label_PuntosFijos);
    infoRow->addStretch();
    contentLayout->addLayout(infoRow);

    mainLayout->addWidget(contentWidget, 1);

    // ------------------------------------------------------------
    // CONEXIONES Y CONFIGURACIÓN INICIAL
    // ------------------------------------------------------------
    // Conexiones de los botones
    connect(pB_CrearRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_CrearRuta_clicked);
    connect(pB_EliminaRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_EliminaRuta_clicked);
    connect(pB_LimpiaRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_LimpiaRuta_clicked);
    connect(pb_guardar, &QPushButton::clicked, this, &Widget_Puntos::on_pb_guardar_clicked);
    connect(pB_EnviaRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_EnviaRuta_clicked);
    connect(pb_Mostrar, &QPushButton::toggled, this, &Widget_Puntos::on_pb_Mostrar_toggled);
    connect(pB_AnalisisDetallado, &QPushButton::clicked, this, &Widget_Puntos::on_pB_AnalisisDetallado_clicked);
    connect(pB_CancelarSeleccion, &QPushButton::clicked, this, &Widget_Puntos::cancelarSeleccionPuntoInicial);

    // Conexiones del modelo
    connect(modelo, &MiModelo::rutaModificada, this, &Widget_Puntos::mostrarMetricasRuta, Qt::UniqueConnection);
    connect(modelo, &MiModelo::rutaConfigurada, this, &Widget_Puntos::onRutaConfigurada, Qt::UniqueConnection);
    connect(modelo, &MiModelo::analisisActualizado, this, &Widget_Puntos::onAnalisisActualizado, Qt::UniqueConnection);
    connect(modelo, &QAbstractItemModel::dataChanged,
            [this](const QModelIndex &topLeft, const QModelIndex &topRight, const QVector<int> &roles) {
        Q_UNUSED(topLeft); Q_UNUSED(topRight);
        if (roles.contains(MiModelo::ErrorRole) || roles.contains(MiModelo::WarningRole))
            tw_puntosRuta->viewport()->update();
        emit sI_pintaRuta(true);
        mostrarMetricasRuta();
    });

    // Conexión del combo de rutas
    connect(cB_Rutas, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            QString realName = cB_Rutas->itemData(index, Qt::UserRole).toString();
            if (!realName.isEmpty()) seleccionarRuta(realName);
        }
    });

    // Inicializar combo de rutas
    cB_Rutas->blockSignals(true);
    actualizarComboBoxRutas();
    if (cB_Rutas->count() > 0) {
        cB_Rutas->setCurrentIndex(0);
    }
    cB_Rutas->blockSignals(false);
    if (cB_Rutas->count() > 0) {
        seleccionarRuta(cB_Rutas->itemData(0, Qt::UserRole).toString());
    }

    // Timer para acuse de recibo
    m_originalStyle = pB_EnviaRuta->styleSheet();
    connect(&m_timer, &QTimer::timeout, this, &Widget_Puntos::on_restoreStyle);

    // Estado inicial de la UI
    pB_AnalisisDetallado->setEnabled(false);
    pb_Mostrar->setEnabled(false);
    pb_Mostrar->setChecked(false);
}

Widget_Puntos::~Widget_Puntos()
{
    delete m_perfilWidget;
    delete modelo;
    delete mdb;
}

// ------------------------------------------------------------
// ACCESO A DATOS
// ------------------------------------------------------------

/**
 * @brief Obtiene el puntero al modelo de datos.
 * @return MiModelo*.
 */
MiModelo *Widget_Puntos::getModelo() const
{
    return modelo;
}

/**
 * @brief Obtiene el combo box de selección de rutas.
 * @return QComboBox*.
 */
QComboBox *Widget_Puntos::getBoxNombreRuta() const
{
    return cB_Rutas;
}

/**
 * @brief Indica si la ruta está actualmente visible en el mapa.
 * @return true si visible.
 */
bool Widget_Puntos::isRouteVisible() const
{
    return pb_Mostrar->isChecked();
}

/**
 * @brief Establece la visibilidad de la ruta en el mapa.
 * @param visible true para mostrar, false para ocultar.
 */
void Widget_Puntos::setRouteVisible(bool visible)
{
    pb_Mostrar->setChecked(visible);
}

// ------------------------------------------------------------
// MÉTODOS PRIVADOS AUXILIARES
// ------------------------------------------------------------

/**
 * @brief Calcula la altitud de vuelo recomendada para una coordenada, sumando un margen de seguridad sobre el terreno.
 * @param terreno Altura del terreno en metros.
 * @param drone Características del dron para acotar la altitud.
 * @return Altitud de vuelo en metros.
 */
double Widget_Puntos::calcularAltitudVuelo(double terreno, const DroneCharacteristics &drone) const
{
    const double SAFETY_OFFSET = 50.0;
    double altitud = terreno + SAFETY_OFFSET;
    altitud = qBound(drone.minAltitude, altitud, drone.maxAltitude);
    altitud = qMax(altitud, 50.0);
    return altitud;
}

/**
 * @brief Genera puntos por defecto para la ruta actual (10 puntos) usando el modelo.
 */
void Widget_Puntos::generarPuntosPorDefecto()
{
    if (!modelo->isDroneConfigured()) {
        qWarning() << "No se puede generar puntos: dron no configurado";
        return;
    }
    modelo->borraTodosLosElementos();
    QList<E_Punto> puntosDefault = modelo->generarPuntosPorDefecto(10);
    for (const E_Punto &punto : puntosDefault)
        modelo->agregarElemento(punto);
}

// ------------------------------------------------------------
// ACTUALIZACIÓN DE INTERFAZ
// ------------------------------------------------------------

/**
 * @brief Actualiza el combo box de rutas consultando las tablas existentes en la base de datos.
 */
void Widget_Puntos::actualizarComboBoxRutas()
{
    QString currentRoute = modelo->getCurrentRouteName();
    cB_Rutas->blockSignals(true);
    cB_Rutas->clear();

    QSqlQuery query("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
                    "AND name NOT LIKE 'drones' AND name NOT LIKE 'rutas_metadata'");
    while (query.next()) {
        QString ruta = query.value(0).toString();
        QSqlQuery metaQuery;
        metaQuery.prepare("SELECT rm.nombre_ruta, d.nombre, d.tipo "
                          "FROM rutas_metadata rm "
                          "LEFT JOIN drones d ON rm.drone_id = d.id "
                          "WHERE rm.nombre_ruta = ?");
        metaQuery.addBindValue(ruta);
        if (metaQuery.exec() && metaQuery.next()) {
            QString droneNombre = metaQuery.value(1).toString();
            QString droneTipo = metaQuery.value(2).toString();
            if (!droneNombre.isEmpty()) {
                QString displayText = QString("%1").arg(ruta);
                cB_Rutas->addItem(displayText);
                cB_Rutas->setItemData(cB_Rutas->count()-1, ruta, Qt::UserRole);
            } else {
                QString displayText = QString("%1").arg(ruta);
                cB_Rutas->addItem(displayText);
                cB_Rutas->setItemData(cB_Rutas->count()-1, ruta, Qt::UserRole);
            }
        } else {
            QString displayText = QString("%1").arg(ruta);
            cB_Rutas->addItem(displayText);
            cB_Rutas->setItemData(cB_Rutas->count()-1, ruta, Qt::UserRole);
        }
    }
    // Restaurar selección
    if (!currentRoute.isEmpty()) {
        setCurrentRouteInCombo(currentRoute, false);
    } else if (cB_Rutas->count() > 0) {
        cB_Rutas->setCurrentIndex(0);
    }

    cB_Rutas->blockSignals(false);
}

/**
 * @brief Actualiza las etiquetas de métricas y el estado de la ruta en la interfaz.
 */
void Widget_Puntos::mostrarMetricasRuta()
{
    if (!modelo->isDroneConfigured() || modelo->rowCount() == 0) {
        label_Distancia->setText("0 m");
        label_AutonomiaRest->setText("0%");
        progressAutonomia->setValue(0);
        label_Tiempo->setText("--:--");
        label_Estado->setText(modelo->isDroneConfigured() ? "Ruta vacía" : "No configurado");
        label_Estado->setStyleSheet(modelo->isDroneConfigured() ?
                                        "color: blue; font-weight: bold;" :
                                        "color: gray; font-weight: bold;");
        return;
    }

    QVariantMap metricas = modelo->obtenerMetricasRuta();
    if (metricas.contains("error")) {
        label_Distancia->setText("0 m");
        label_AutonomiaRest->setText("0%");
        progressAutonomia->setValue(0);
        label_Tiempo->setText("--:--");
        label_Estado->setText("Error");
        label_Estado->setStyleSheet("color: red; font-weight: bold;");
        return;
    }

    double distancia = metricas["distancia_total"].toDouble();
    double porcentaje = metricas["autonomia_porcentaje"].toDouble();
    double tiempoVuelo = metricas["tiempo_vuelo"].toDouble();
    bool valida = metricas["valida"].toBool();

    label_Distancia->setText(QString("%1 m").arg(distancia, 0, 'f', 0));
    label_AutonomiaRest->setText(QString("%1%").arg(100.0 - porcentaje, 0, 'f', 0));
    if (tiempoVuelo > 0) {
        int minutos = static_cast<int>(tiempoVuelo / 60);
        int segundos = static_cast<int>(tiempoVuelo) % 60;
        label_Tiempo->setText(QString("%1:%2").arg(minutos).arg(segundos, 2, 10, QChar('0')));
    } else {
        label_Tiempo->setText("--:--");
    }

    progressAutonomia->setValue(porcentaje);
    progressAutonomia->setFormat("");

    QVariantMap analisis = modelo->analizarRutaDetallada();
    bool tieneAdvertencias = !analisis.value("puntos_criticos").toList().isEmpty() ||
            analisis.value("num_puntos_criticos").toInt() > 0 ||
            !modelo->obtenerRecomendaciones().isEmpty();

    if (valida) {
        if (tieneAdvertencias) {
            label_Estado->setText("Válida (con advertencias)");
            label_Estado->setStyleSheet("color: orange; font-weight: bold;");
        } else {
            label_Estado->setText("Válida");
            label_Estado->setStyleSheet("color: green; font-weight: bold;");
        }
    } else {
        label_Estado->setText("Inválida");
        label_Estado->setStyleSheet("color: red; font-weight: bold;");
    }
}

/**
 * @brief Muestra un cuadro de diálogo con el análisis detallado de la ruta.
 */
void Widget_Puntos::mostrarAnalisisDetallado()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::information(this, "Análisis", "Configure un dron primero.");
        return;
    }

    auto analisis = modelo->analizarRutaDetallada();
    auto recomendaciones = modelo->obtenerRecomendaciones();

    if (analisis.contains("error")) {
        QMessageBox::warning(this, "Error", analisis["error"].toString());
        return;
    }

    QString mensaje = "📊 ANÁLISIS DETALLADO DE RUTA\n\n";
    mensaje += "📋 INFORMACIÓN BÁSICA:\n";
    mensaje += QString("• Ruta: %1\n").arg(modelo->getCurrentRouteName());
    mensaje += QString("• Dron: %1\n").arg(modelo->getDroneCharacteristics().nombre);
    mensaje += QString("• Tipo: %1\n").arg(modelo->getDroneCharacteristics().tipoToString());
    mensaje += QString("• Waypoints: %1\n").arg(analisis["num_puntos"].toInt());
    mensaje += QString("• Estado: %1\n\n").arg(analisis["valida"].toBool() ? "✓ Válida" : "✗ Inválida");

    mensaje += "📏 DISTANCIA Y TIEMPO:\n";
    mensaje += QString("• Distancia total: %1 m\n").arg(analisis["distancia_total"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Distancia en línea recta: %1 m\n").arg(analisis["distancia_linea_recta"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Eficiencia de ruta: %1%\n").arg(analisis["eficiencia_ruta"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Tiempo estimado: %1 s\n").arg(analisis["tiempo_vuelo"].toDouble(), 0, 'f', 0);
    mensaje += QString("• Consumo estimado: %1 Wh\n\n").arg(analisis["consumo_wh"].toDouble(), 0, 'f', 1);

    mensaje += "⚡ VELOCIDAD:\n";
    mensaje += QString("• Mínima: %1 m/s\n").arg(analisis["velocidad_min"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Máxima: %1 m/s\n").arg(analisis["velocidad_max"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Promedio: %1 m/s\n\n").arg(analisis["velocidad_promedio"].toDouble(), 0, 'f', 1);

    mensaje += "📈 ALTITUD:\n";
    mensaje += QString("• Mínima: %1 m\n").arg(analisis["altitud_min"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Máxima: %1 m\n").arg(analisis["altitud_max"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Promedio: %1 m\n").arg(analisis["altitud_promedio"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Ascenso total: %1 m\n").arg(analisis["ascenso_total"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Descenso total: %1 m\n").arg(analisis["descenso_total"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Desnivel total: %1 m\n\n").arg(analisis["desnivel_total"].toDouble(), 0, 'f', 1);

    mensaje += "🛡️ SEGURIDAD:\n";
    mensaje += QString("• Margen de seguridad: %1%\n").arg(analisis["margen_seguridad"].toDouble(), 0, 'f', 0);
    mensaje += QString("• Nivel de seguridad: %1\n").arg(analisis["nivel_seguridad"].toString());
    mensaje += QString("• Factor de carga máximo: %1 g\n").arg(analisis["factor_carga_max"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Estrés estimado: %1%\n").arg(analisis["estres_estimado"].toDouble(), 0, 'f', 0);
    mensaje += QString("• Puntos críticos: %1\n\n").arg(analisis["num_puntos_criticos"].toInt());

    QVariantList puntosCriticos = analisis["puntos_criticos"].toList();
    if (!puntosCriticos.isEmpty()) {
        mensaje += "🔴 PUNTOS CRÍTICOS DETECTADOS:\n";
        for (const auto &punto : puntosCriticos) {
            QVariantMap datos = punto.toMap();
            mensaje += QString("• %1 (%2): %3\n")
                    .arg(datos["punto"].toString())
                    .arg(datos["nivel"].toString())
                    .arg(datos["problemas"].toStringList().join(", "));
        }
        mensaje += "\n";
    }

    if (!recomendaciones.isEmpty()) {
        mensaje += "💡 RECOMENDACIONES:\n";
        for (const auto &rec : recomendaciones)
            mensaje += "• " + rec + "\n";
    }

    QMessageBox msgBox;
    msgBox.setWindowTitle("Análisis Detallado de Ruta");
    msgBox.setText(mensaje);
    msgBox.setIcon(QMessageBox::Information);
    QPushButton *btnPuntosCriticos = nullptr;
    QPushButton *btnExportar = nullptr;
    if (!puntosCriticos.isEmpty())
        btnPuntosCriticos = msgBox.addButton("Ver puntos críticos", QMessageBox::ActionRole);
    btnExportar = msgBox.addButton("Exportar análisis", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Ok);

    int result = msgBox.exec();
    if (msgBox.clickedButton() == btnPuntosCriticos)
        mostrarPuntosCriticosDetalles(puntosCriticos);
    else if (msgBox.clickedButton() == btnExportar)
        exportarAnalisis(analisis);
}

/**
 * @brief Exporta el análisis de la ruta a un archivo de texto.
 * @param analisis Mapa con los datos del análisis.
 */
void Widget_Puntos::exportarAnalisis(const QVariantMap &analisis)
{
    QString fileName = QFileDialog::getSaveFileName(this, "Guardar análisis",
                                                    QDir::currentPath(),
                                                    "Archivos de texto (*.txt);;Archivos CSV (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "No se pudo crear el archivo");
        return;
    }

    QTextStream out(&file);
    out << "ANÁLISIS DE RUTA\n";
    out << "================\n\n";
    out << "Ruta: " << modelo->getCurrentRouteName() << "\n";
    out << "Dron: " << modelo->getDroneCharacteristics().nombre << "\n";
    out << "Tipo: " << modelo->getDroneCharacteristics().tipoToString() << "\n\n";

    out << "Distancia total: " << analisis["distancia_total"].toDouble() << " m\n";
    out << "Tiempo estimado: " << analisis["tiempo_vuelo"].toDouble() << " s\n";
    out << "Consumo estimado: " << analisis["consumo_wh"].toDouble() << " Wh\n";
    out << "Velocidad promedio: " << analisis["velocidad_promedio"].toDouble() << " m/s\n";
    out << "Eficiencia: " << analisis["eficiencia_ruta"].toDouble() << "%\n";
    out << "Margen de seguridad: " << analisis["margen_seguridad"].toDouble() << "%\n";
    out << "Puntos críticos: " << analisis["num_puntos_criticos"].toInt() << "\n";

    file.close();
    QMessageBox::information(this, "Exportado", "Análisis exportado correctamente.");
}

// ------------------------------------------------------------
// SLOTS PÚBLICOS
// ------------------------------------------------------------

/**
 * @brief Agrega un nuevo punto a la ruta (llamado desde el mapa o desde otros módulos).
 * @param punto Punto a agregar.
 */
void Widget_Puntos::sL_agregarNuevoPunto(const E_Punto &punto)
{
    modelo->validarYAgregarElemento(punto);
    mostrarMetricasRuta();
    updateUIForDroneConfigured(true);
    tw_puntosRuta->viewport()->update();
    tw_puntosRuta->resizeColumnsToContents();
}

/**
 * @brief Actualiza puntos existentes en la ruta (llamado desde el mapa).
 * @param puntos Lista de puntos modificados.
 */
void Widget_Puntos::sL_actualizaPunto(const QList<E_Punto> &puntos)
{
    if (puntos.isEmpty()) return;
    if (!modelo->isDroneConfigured()) return;

    QString nombreRuta = cB_Rutas->currentText();
    if (nombreRuta.isEmpty()) return;

    QString nombreRutaLimpio = obtenerNombreTablaLimpio(nombreRuta);
    modelo->actualizarElementosEnTabla(nombreRutaLimpio, puntos);
    mostrarMetricasRuta();
    emit sI_pintaRuta(true);
    tw_puntosRuta->viewport()->update();
    tw_puntosRuta->resizeColumnsToContents();
    updateUIForDroneConfigured(true);
}

/**
 * @brief Selecciona la ruta cuyo nombre real está en el combo (usado internamente).
 * @param nombreRuta No usado.
 */
void Widget_Puntos::seleccionarRuta(const QString &/*nombreRuta*/)
{
    int index = cB_Rutas->currentIndex();
    if (index < 0) return;

    QString realRouteName = cB_Rutas->itemData(index, Qt::UserRole).toString();
    if (!realRouteName.isEmpty()) {
        cargarRuta(realRouteName);
    }
}

/**
 * @brief Muestra un acuse de recibo visual en el botón de enviar.
 */
void Widget_Puntos::on_AcuseRecibo()
{
    if (!m_timer.isActive())
        m_originalStyle = pB_EnviaRuta->styleSheet();
    pB_EnviaRuta->setStyleSheet("background-color: #00FF00; color: black; font-weight: bold; border-radius: 4px;");
    m_timer.start(1000);
}

/**
 * @brief Restaura el estilo original del botón de enviar.
 */
void Widget_Puntos::on_restoreStyle()
{
    pB_EnviaRuta->setStyleSheet(m_originalStyle);
    m_timer.stop();
}

/**
 * @brief Slot llamado cuando el modelo emite rutaConfigurada.
 * @param tipoDron Tipo de dron configurado.
 * @param nombreRuta Nombre de la ruta configurada.
 */
void Widget_Puntos::onRutaConfigurada(const QString &tipoDron, const QString &nombreRuta)
{
    QMessageBox::information(this, "Ruta Configurada",
                             QString("Ruta '%1' configurada exitosamente.\n"
                                     "Tipo de dron: %2")
                             .arg(nombreRuta).arg(tipoDron));

    updateUIForDroneConfigured(true);
    mostrarInfoDronActual();
    emit sI_DronConfigurado(tipoDron, nombreRuta);
    mostrarMetricasRuta();
    emit sI_rutasModificadas();
    emit sI_rutaSeleccionada(nombreRuta);
}

/**
 * @brief Slot llamado cuando el modelo emite analisisActualizado.
 * @param analisis Mapa con el análisis actualizado.
 */
void Widget_Puntos::onAnalisisActualizado(const QVariantMap &analisis)
{
    actualizarPanelAnalisis(analisis);
}

/**
 * @brief Maneja el movimiento de un punto en el mapa, actualizando coordenadas y altitud.
 * ...
 *
 * \startuml
 * participant "CMapaPlot (mapa)" as Map
 * participant "Widget_Puntos" as Widget
 * participant "MiModelo" as Model
 * participant "AlturaWorker" as AltWorker
 *
 * Map -> Widget: onPointMoved(index)
 * activate Widget
 *
 * Widget -> Map: currentPoints()
 * Map --> Widget: pointsMetros
 *
 * Widget -> Map: getProjection()->inverse()
 * Map --> Widget: nuevaPos (QGeoCoordinate)
 *
 * Widget -> Model: setData(idxLat, nuevaPos.latitude())
 * Widget -> Model: setData(idxLon, nuevaPos.longitude())
 *
 * Widget -> AltWorker: obtenerAltura(lat, lon)
 * AltWorker --> Widget: terreno
 *
 * Widget -> Model: getDroneCharacteristics()
 * Model --> Widget: drone
 *
 * Widget -> Widget: calcularAltitudVuelo(terreno, drone)
 * Widget --> Widget: nuevaAltitud
 *
 * Widget -> Model: setData(idxAlt, nuevaAltitud)
 *
 * deactivate Widget
 * \enduml
 */
void Widget_Puntos::onPointMoved(int index)
{
    if (!modelo || !modelo->isDroneConfigured() || modelo->rowCount() == 0)
        return;
    if (index < 0 || index >= modelo->rowCount())
        return;

    CMapaPlot *mapa = qobject_cast<CMapaPlot*>(sender());
    if (!mapa) {
        return;
    }
    if (!mapa->getProjection()) {
        return;
    }

    QVector<QPointF> pointsMetros = mapa->currentPoints();
    if (index >= pointsMetros.size()) {
        return;
    }

    QGeoCoordinate nuevaPos = mapa->getProjection()->inverse(pointsMetros[index]);

    QModelIndex idxLat = modelo->index(index, 3);
    QModelIndex idxLon = modelo->index(index, 4);
    modelo->setData(idxLat, nuevaPos.latitude(), Qt::EditRole);
    modelo->setData(idxLon, nuevaPos.longitude(), Qt::EditRole);

    // Ajustar altitud según el terreno
    if (m_alturaWorker) {
        DroneCharacteristics drone = modelo->getDroneCharacteristics();
        qint16 terreno = m_alturaWorker->obtenerAltura(nuevaPos.latitude(),
                                                       nuevaPos.longitude());
        if (terreno < 0) terreno = 0;
        double nuevaAltitud = calcularAltitudVuelo(terreno, drone);

        QModelIndex idxAlt = modelo->index(index, 6);
        modelo->setData(idxAlt, nuevaAltitud, Qt::EditRole);

        qDebug() << "Punto" << index << "movido a:" << nuevaPos.toString()
                 << "| Terreno:" << terreno << "m"
                 << "| Nueva altitud:" << nuevaAltitud << "m";
    }
}

/**
 * @brief Inicia el modo de selección del punto inicial (P1) haciendo clic derecho en el mapa.
 */
void Widget_Puntos::iniciarSeleccionPuntoInicial()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "No hay ruta configurada.");
        return;
    }
    if (modelo->rowCount() > 0) {
        QMessageBox::warning(this, "Error", "La ruta ya tiene puntos. No se puede iniciar selección.");
        return;
    }

    m_modoSeleccionPuntoInicial = true;
    label_Estado->setText("Seleccione punto inicial P1 en el mapa (clic derecho)");
    label_Estado->setStyleSheet("color: blue; font-weight: bold;");
    pB_CancelarSeleccion->setVisible(true);

    emit sI_iniciarSeleccionPuntoInicial();
}

/**
 * @brief Cancela el modo de selección de punto inicial.
 */
void Widget_Puntos::cancelarSeleccionPuntoInicial()
{
    if (!m_modoSeleccionPuntoInicial) return;

    m_modoSeleccionPuntoInicial = false;
    pB_CancelarSeleccion->setVisible(false);
    label_Estado->setText("Selección cancelada");
    label_Estado->setStyleSheet("color: orange; font-weight: bold;");

    emit sI_cancelarSeleccionPuntoInicial();
}

/**
 * @brief Agrega el punto inicial y genera los 9 restantes.
 * ...
 *
 * \startuml
 * start
 * :Verificar modo selección y configuración;
 * if (no válido) then (sí)
 *   :cancelarSeleccionPuntoInicial();
 *   stop
 * endif
 *
 * :Obtener drone del modelo;
 * :Obtener altura terreno para P1;
 * :Calcular altitud P1 con margen;
 * :Calcular velocidad por defecto;
 * :Calcular radio por defecto;
 * :Calcular distancia óptima;
 *
 * :Crear punto P1;
 * :Agregar P1 al modelo;
 *
 * :Inicializar puntoActual = coordenada;
 * :azimut = 90°;
 *
 * while (i desde 2 hasta 10)
 *   :puntoActual = puntoActual.atDistanceAndAzimuth(distanciaOptima, azimut);
 *   :Obtener altura terreno;
 *   :Calcular altitud con margen;
 *   :Crear punto Pi;
 *   :Agregar Pi al modelo;
 * endwhile
 *
 * :Finalizar modo selección;
 * :Actualizar interfaz;
 * :emitir señales;
 * stop
 * \enduml
 */
void Widget_Puntos::agregarPuntoInicialYGenerarResto(const QGeoCoordinate &coordenada)
{
    // Verificar que estamos en modo selección y que todo está configurado
    if (!m_modoSeleccionPuntoInicial) return;
    if (!modelo->isDroneConfigured() || !m_alturaWorker) {
        cancelarSeleccionPuntoInicial();
        return;
    }

    DroneCharacteristics drone = modelo->getDroneCharacteristics();

    // ------------------------------------------------------------
    // 1. Obtener altura del terreno para el punto P1
    // ------------------------------------------------------------
    qint16 terrenoP1 = m_alturaWorker->obtenerAltura(coordenada.latitude(),
                                                     coordenada.longitude());
    if (terrenoP1 < 0) terrenoP1 = 0;  // si no hay datos, asumimos nivel del mar
    double altitudP1 = calcularAltitudVuelo(terrenoP1, drone);  // aplica margen y límites

    // ------------------------------------------------------------
    // 2. Calcular valores por defecto para velocidad y radio
    // ------------------------------------------------------------
    double defaultVelocidad;
    if (drone.type == DroneType::FIXED_WING) {
        defaultVelocidad = drone.maxSpeed * 0.65;        // 65% de la máxima para ala fija
    } else {
        defaultVelocidad = drone.maxSpeed * 0.45;        // 45% para quadcóptero
    }
    defaultVelocidad = qMax(defaultVelocidad, drone.minSpeed);  // no bajar del mínimo

    double defaultRadio;
    if (drone.type == DroneType::FIXED_WING) {
        double radioTeorico = (defaultVelocidad * defaultVelocidad) / (9.81 * 0.577);
        defaultRadio = qMax(radioTeorico * 1.5, drone.minTurnRadius * 2.0);
    } else {
        defaultRadio = 10.0;  // valor fijo para quadcóptero
    }

    // ------------------------------------------------------------
    // 3. Calcular distancia óptima entre puntos (punto medio)
    // ------------------------------------------------------------
    double distanciaOptima = (drone.minWaypointDistance + drone.maxWaypointDistance) / 2.0;

    // ------------------------------------------------------------
    // 4. Crear el punto P1 y agregarlo al modelo
    // ------------------------------------------------------------
    E_Punto p1;
    p1.id = 0;
    p1.nombre = "P1";
    p1.modo = 0;
    p1.pos = coordenada;
    p1.altura = altitudP1;
    p1.velocidad = defaultVelocidad;
    p1.radio = defaultRadio;
    p1.distanciaAnterior = 0.0;
    p1.tasaAscenso = 0.0;
    p1.puntoCritico = false;

    modelo->agregarElemento(p1);  // esto ya valida y añade el punto

    // ------------------------------------------------------------
    // 5. Generar los puntos P2 a P10
    // ------------------------------------------------------------
    double azimut = 90.0;  // dirección este (se puede cambiar o hacer configurable)
    QGeoCoordinate puntoActual = coordenada;

    for (int i = 2; i <= 10; ++i) {
        // Calcular la siguiente coordenada a la distancia óptima en el azimut fijo
        puntoActual = puntoActual.atDistanceAndAzimuth(distanciaOptima, azimut);

        // Obtener altura del terreno para este nuevo punto
        qint16 terreno = m_alturaWorker->obtenerAltura(puntoActual.latitude(),
                                                       puntoActual.longitude());
        if (terreno < 0) terreno = 0;
        double altitudPunto = calcularAltitudVuelo(terreno, drone);

        // Crear el punto
        E_Punto p;
        p.id = i - 1;
        p.nombre = QString("P%1").arg(i);
        p.modo = 0;
        p.pos = puntoActual;
        p.altura = altitudPunto;
        p.velocidad = defaultVelocidad;
        p.radio = defaultRadio;
        p.distanciaAnterior = distanciaOptima;  // será la distancia entre puntos consecutivos
        p.tasaAscenso = 0.0;  // se recalculará automáticamente al validar
        p.puntoCritico = false;

        modelo->agregarElemento(p);
    }

    // ------------------------------------------------------------
    // 6. Finalizar modo selección y actualizar interfaz
    // ------------------------------------------------------------
    m_modoSeleccionPuntoInicial = false;
    pB_CancelarSeleccion->setVisible(false);
    label_Estado->setText("Ruta de 10 puntos generada con altitud sobre terreno");
    label_Estado->setStyleSheet("color: green; font-weight: bold;");
    emit sI_cancelarSeleccionPuntoInicial();

    // Refrescar vista y habilitar botones
    updateUIForDroneConfigured(true);
    tw_puntosRuta->viewport()->update();
    tw_puntosRuta->resizeColumnsToContents();
    pb_Mostrar->setChecked(true);
}

/**
 * @brief Carga una ruta desde la base de datos y actualiza la interfaz.
 * @param routeName Nombre de la ruta a cargar.
 */
void Widget_Puntos::cargarRuta(const QString &routeName)
{
    if (routeName.isEmpty()) return;

    modelo->CargarRutaDesdeBaseDatos(routeName);
    updateUIForDroneConfigured(modelo->isDroneConfigured());
    mostrarMetricasRuta();

    if (modelo->rowCount() > 0) {
        if (!pb_Mostrar->isChecked()) {
            pb_Mostrar->setChecked(true);
        } else {
            emit sI_pintaRuta(true);
        }
    } else {
        pb_Mostrar->setChecked(false);
        emit sI_pintaRuta(false);
    }

    // Sincronizar el combo con el nombre real
    setCurrentRouteInCombo(routeName, true);

    emit sI_rutaSeleccionada(routeName);
    emit sI_visibilidadCambiada(routeName, pb_Mostrar->isChecked());
}

/**
 * @brief Establece la ruta actual en el combo box (por nombre real).
 * @param routeName Nombre real de la ruta.
 * @param blockSignals Si true, bloquea las señales del combo durante la operación.
 */
void Widget_Puntos::setCurrentRouteInCombo(const QString &routeName, bool blockSignals)
{
    if (blockSignals) cB_Rutas->blockSignals(true);

    bool found = false;
    for (int i = 0; i < cB_Rutas->count(); ++i) {
        if (cB_Rutas->itemData(i, Qt::UserRole).toString() == routeName) {
            cB_Rutas->setCurrentIndex(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qWarning() << "Ruta no encontrada en combo:" << routeName;
    }

    if (blockSignals) cB_Rutas->blockSignals(false);   // ← ¡siempre!
}

// ------------------------------------------------------------
// SLOTS PRIVADOS DE UI
// ------------------------------------------------------------

/**
 * @brief Guarda la ruta actual en un archivo CSV.
 */
void Widget_Puntos::on_pb_guardar_clicked()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "Configure una ruta primero");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Guardar ruta"),
                                                    QDir::currentPath(), tr("Archivos CSV (*.csv)"));
    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".csv")) fileName.append(".csv");
        QMessageBox msgBox;
        msgBox.setWindowTitle("Opciones de guardado");
        msgBox.setText("¿Cómo desea guardar la ruta?");
        QPushButton *btnBasico = msgBox.addButton("Solo puntos", QMessageBox::ActionRole);
        QPushButton *btnCompleto = msgBox.addButton("Con análisis", QMessageBox::ActionRole);
        QPushButton *btnCancelar = msgBox.addButton("Cancelar", QMessageBox::RejectRole);
        msgBox.exec();
        if (msgBox.clickedButton() == btnCancelar) return;

        bool guardadoExitoso = false;
        if (msgBox.clickedButton() == btnBasico) {
            guardadoExitoso = modelo->guardarCSV(fileName);
        } else if (msgBox.clickedButton() == btnCompleto) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                DroneCharacteristics drone = modelo->getDroneCharacteristics();
                stream << "# ANÁLISIS DE RUTA DE DRON\n";
                stream << "# Dron: " << drone.nombre << " (" << drone.tipoToString() << ")\n";
                stream << "# Ruta: " << modelo->getCurrentRouteName() << "\n";
                stream << "# Fecha: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n\n";
                auto analisis = modelo->analizarRutaDetallada();
                if (!analisis.contains("error")) {
                    stream << "# ESTADÍSTICAS:\n";
                    stream << "# Distancia total: " << analisis["distancia_total"].toDouble() << " m\n";
                    stream << "# Tiempo estimado: " << analisis["tiempo_vuelo"].toDouble() << " s\n";
                    stream << "# Waypoints: " << analisis["num_puntos"].toInt() << "\n";
                    stream << "# Velocidad promedio: " << analisis["velocidad_promedio"].toDouble() << " m/s\n";
                    stream << "# Eficiencia: " << analisis["eficiencia_ruta"].toDouble() << "%\n";
                    stream << "# Margen seguridad: " << analisis["margen_seguridad"].toDouble() << "%\n\n";
                }
                stream << "ID,Nombre,Modo,Latitud,Longitud,Distancia_Anterior(m),Altura,Velocidad,Radio,Tasa_Vertical,Advertencias\n";
                const QList<E_Punto> &puntos = modelo->obtenerLista();
                for (const E_Punto &punto : puntos) {
                    stream << punto.id
                           << "," << punto.nombre
                           << "," << punto.modo
                           << "," << QString::number(punto.pos.latitude(), 'f', 8)
                           << "," << QString::number(punto.pos.longitude(), 'f', 8)
                           << "," << QString::number(punto.distanciaAnterior, 'f', 2)
                           << "," << QString::number(punto.altura, 'f', 2)
                           << "," << QString::number(punto.velocidad, 'f', 2)
                           << "," << QString::number(punto.radio, 'f', 2)
                           << "," << QString::number(punto.tasaAscenso, 'f', 2)
                           << "," << punto.advertencias.join("; ").replace(",", ";")
                           << "\n";
                }
                file.close();
                guardadoExitoso = true;
            }
        }
        if (guardadoExitoso)
            QMessageBox::information(this, "Guardado", "Ruta guardada exitosamente en:\n" + fileName);
        else
            QMessageBox::warning(this, "Error", "No se pudo guardar la ruta");
    }
}

/**
 * @brief Crea una nueva ruta a través del diálogo de dron.
 */
void Widget_Puntos::on_pB_CrearRuta_clicked()
{
    DroneDialog dialog(DroneDialog::CREATE_ROUTE, this);
    if (dialog.exec() == QDialog::Accepted) {
        DroneCharacteristics drone = dialog.getSelectedDrone();
        QString routeName = dialog.getRouteName();
        if (routeName.isEmpty()) {
            QMessageBox::warning(this, "Error", "El nombre de la ruta no puede estar vacío.");
            return;
        }

        // Ocultar cualquier ruta visible
        emit sI_pintaRuta(false);
        pb_Mostrar->setChecked(false);
        pb_Mostrar->setEnabled(false);

        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?");
        checkQuery.addBindValue(routeName);
        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            QMessageBox::StandardButton respuesta = QMessageBox::question(
                        this, "Ruta existente",
                        QString("La ruta '%1' ya existe.\n¿Desea sobrescribirla?").arg(routeName),
                        QMessageBox::Yes | QMessageBox::No);
            if (respuesta != QMessageBox::Yes) return;
            modelo->limpiaRutaCompleta(routeName);
        }

        modelo->configurarRuta(drone, routeName);
        iniciarSeleccionPuntoInicial();

        mostrarMetricasRuta();
        mostrarInfoDronActual();
        updateUIForDroneConfigured(true);

        actualizarComboBoxRutas();
        cB_Rutas->blockSignals(true);
        for (int i = 0; i < cB_Rutas->count(); ++i) {
            if (cB_Rutas->itemData(i, Qt::UserRole).toString() == routeName) {
                cB_Rutas->setCurrentIndex(i);
                break;
            }
        }
        cB_Rutas->blockSignals(false);

        pb_Mostrar->setChecked(false);
        cargarRuta(routeName);
        emit sI_pintaRuta(false);
        emit sI_rutasModificadas();
        emit sI_rutaSeleccionada(routeName);
    }
}

/**
 * @brief Elimina la ruta seleccionada actualmente.
 */
void Widget_Puntos::on_pB_EliminaRuta_clicked()
{
    QString nombreRuta = cB_Rutas->currentText();
    if (nombreRuta.isEmpty()) {
        QMessageBox::warning(this, "Error", "No hay ruta seleccionada.");
        return;
    }

    QString nombreRutaLimpio = obtenerNombreTablaLimpio(nombreRuta);
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirmar eliminación",
                                  QString("¿Está seguro de eliminar la ruta '%1' permanentemente?\n"
                                          "Esta acción eliminará todos los puntos y la configuración de la ruta.")
                                  .arg(nombreRutaLimpio),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) return;

    if (cB_Rutas->currentText() == nombreRuta) {
        emit sI_pintaRuta(false);
    }

    if (modelo->eliminarTablaRuta(nombreRutaLimpio)) {
        QString csvFile = QDir::currentPath() + "/" + nombreRutaLimpio + ".csv";
        if (QFile::exists(csvFile)) QFile::remove(csvFile);

        actualizarComboBoxRutas();
        mostrarMetricasRuta();

        if (cB_Rutas->count() == 0) {
            updateUIForDroneConfigured(false);
            emit sI_pintaRuta(false);
        } else {
            cB_Rutas->setCurrentIndex(0);
            seleccionarRuta(cB_Rutas->currentText());
        }

        QMessageBox::information(this, "Ruta eliminada",
                                 QString("La ruta '%1' ha sido eliminada correctamente.").arg(nombreRutaLimpio));

        emit sI_rutasModificadas();

    } else {
        QMessageBox::warning(this, "Error", "No se pudo eliminar la ruta.");
    }
}

/**
 * @brief Restablece la ruta actual a los 10 puntos por defecto.
 */
void Widget_Puntos::on_pB_LimpiaRuta_clicked()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "No hay ruta configurada.");
        return;
    }

    QString nombreRuta = cB_Rutas->currentText();
    if (nombreRuta.isEmpty()) {
        QMessageBox::warning(this, "Error", "No hay ruta seleccionada.");
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Restablecer ruta",
                                  QString("¿Restablecer los 10 puntos por defecto en '%1'?\n"
                                          "Se perderán todas las modificaciones actuales.")
                                  .arg(obtenerNombreTablaLimpio(nombreRuta)),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) return;

    QString nombreRutaLimpio = obtenerNombreTablaLimpio(nombreRuta);
    modelo->limpiaRutaCompleta(nombreRutaLimpio);
    generarPuntosPorDefecto();
    modelo->revalidarRutaCompleta();

    emit sI_pintaRuta(pb_Mostrar->isChecked());
    QMessageBox::information(this, "Ruta restablecida",
                             "La ruta ha sido restablecida a los 10 puntos por defecto.");
}

/**
 * @brief Envía la ruta al dron (a través de la controladora).
 */
void Widget_Puntos::on_pB_EnviaRuta_clicked()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "Configure una ruta primero");
        return;
    }

    if (modelo->rowCount() != 10) {
        QMessageBox::critical(this, "Error",
                              QString("La ruta debe tener EXACTAMENTE 10 puntos.\n"
                                      "Puntos actuales: %1").arg(modelo->rowCount()));
        return;
    }

    auto validacion = modelo->validarRuta();
    if (!validacion.first) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Ruta no válida",
                                      QString("La ruta tiene problemas:\n%1\n\n¿Desea enviarla de todas formas?")
                                      .arg(validacion.second),
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::No) {
            on_pB_AnalisisDetallado_clicked();
            return;
        }
    }

    QList<QPair<double, double>> coordenadas;
    QList<E_Punto> puntos = modelo->obtenerLista();
    if (puntos.size() >= 2) {
        coordenadas = modelo->obtenerCoordenadasRuta();
        emit sI_Ruta(coordenadas);
        emit sI_Ruta2(puntos);
        emit sI_RutaValidada(validacion.first, validacion.second);
        on_AcuseRecibo();

        QString mensaje = QString("✓ Ruta enviada exitosamente\n\n");
        mensaje += QString("Puntos: %1\n").arg(puntos.size());
        auto metricas = modelo->obtenerMetricasRuta();
        mensaje += QString("Distancia: %1 m\n").arg(metricas["distancia_total"].toDouble(), 0, 'f', 1);
        auto analisis = modelo->analizarRutaDetallada();
        if (analisis.contains("tiempo_vuelo")) {
            int minutos = static_cast<int>(analisis["tiempo_vuelo"].toDouble() / 60);
            int segundos = static_cast<int>(analisis["tiempo_vuelo"].toDouble()) % 60;
            mensaje += QString("Tiempo estimado: %1:%2\n").arg(minutos).arg(segundos, 2, 10, QChar('0'));
        }
        mensaje += QString("Estado: %1\n").arg(validacion.first ? "✓ Válida" : "⚠ Con observaciones");
        if (!validacion.first) mensaje += QString("\nObservaciones:\n%1").arg(validacion.second);
        QMessageBox::information(this, "Ruta enviada", mensaje);
    } else {
        QMessageBox::critical(this, "Error", "La ruta debe tener al menos 2 puntos.");
    }
}

/**
 * @brief Alterna la visibilidad de la ruta en el mapa.
 * @param checked true para mostrar, false para ocultar.
 */
void Widget_Puntos::on_pb_Mostrar_toggled(bool checked)
{
    pb_Mostrar->setText(checked ? "Ocultar" : "Mostrar");
    pb_Mostrar->setToolTip(checked ? "Ocultar Ruta" : "Mostrar Ruta");
    emit sI_pintaRuta(checked);
    emit sI_visibilidadCambiada(modelo->getCurrentRouteName(), checked);
}

/**
 * @brief Muestra el análisis detallado de la ruta.
 */
void Widget_Puntos::on_pB_AnalisisDetallado_clicked()
{
    mostrarAnalisisDetallado();
}

/**
 * @brief Cierra el widget (lo oculta) y emite la señal closed().
 */
void Widget_Puntos::on_closeButtonClicked()
{
    hide();
    emit closed();
}

/**
 * @brief Muestra el widget de perfil de altitud.
 */
void Widget_Puntos::mostrarPerfil()
{
    if (!modelo->isDroneConfigured() || modelo->rowCount() < 2) {
        QMessageBox::warning(this, "Perfil", "No hay suficientes puntos para mostrar el perfil.");
        return;
    }

    m_perfilWidget->setRuta(modelo->obtenerLista());
    m_perfilWidget->show();
    m_perfilWidget->raise();
}

/**
 * @brief Muestra un cuadro de diálogo con una guía de planificación de rutas.
 */
void Widget_Puntos::mostrarGuia()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Guía de planificación de rutas");
    dialog->setMinimumSize(600, 500);
    dialog->setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);

    // Área de scroll con contenido de texto enriquecido
    QScrollArea *scrollArea = new QScrollArea(dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *contentWidget = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);

    QLabel *label = new QLabel(contentWidget);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setOpenExternalLinks(false);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);

    // Construir el texto de la guía
    QString guia = "<h2>Guía para establecer una ruta correcta</h2>";
    guia += "<p>Una ruta de vuelo está compuesta por una serie de waypoints (puntos) que el dron debe seguir. "
            "Cada waypoint tiene parámetros como posición, altitud, velocidad y radio de giro.</p>";

    guia += "<h3>Parámetros del dron</h3>";
    guia += "<ul>";
    guia += "<li><b>Velocidad máxima/mínima:</b> La velocidad en cada punto debe estar entre la mínima y máxima del dron. "
            "Para ala fija, no puede ser inferior a la velocidad de pérdida.</li>";
    guia += "<li><b>Altitud máxima/mínima:</b> La altitud (sobre el nivel del mar) debe estar dentro del rango permitido.</li>";
    guia += "<li><b>Tasa de ascenso/descenso:</b> Los cambios de altitud entre puntos no deben exceder la capacidad del dron. "
            "Se calcula como diferencia de altitud / tiempo del segmento.</li>";
    guia += "<li><b>Radio de giro mínimo:</b> Para ala fija, el radio de giro debe ser suficiente para la velocidad. "
            "Un radio demasiado pequeño puede provocar pérdida de control.</li>";
    guia += "<li><b>Distancia entre waypoints:</b> Debe estar entre la mínima y máxima configuradas. "
            "Distancias muy cortas pueden ser ineficientes; muy largas pueden exceder la autonomía.</li>";
    guia += "<li><b>Autonomía (distancia y tiempo):</b> La suma de distancias no debe superar el alcance máximo, "
            "y el tiempo estimado no debe exceder la duración de la batería.</li>";
    guia += "</ul>";

    guia += "<h3>Terreno y obstáculos</h3>";
    guia += "<p>La altitud de los waypoints es sobre el nivel del mar. El terreno se tiene en cuenta al generar puntos "
            "(se suma un margen de seguridad de 50 m sobre el terreno) y al analizar el perfil. "
            "Utilice el botón <b>Perfil</b> para visualizar la trayectoria de vuelo frente al terreno. "
            "Las cruces rojas indican puntos donde el terreno podría estar peligrosamente cerca.</p>";

    guia += "<h3>Interpretación de la tabla</h3>";
    guia += "<ul>";
    guia += "<li><b>✓</b> Punto válido, sin problemas.</li>";
    guia += "<li><b>⚠</b> Advertencia: el punto tiene algún aspecto mejorable (ej. radio algo pequeño, pendiente moderada).</li>";
    guia += "<li><b>🔴</b> Punto crítico: combina varias advertencias o presenta un riesgo potencial.</li>";
    guia += "<li><b>❌</b> Error: el punto incumple un límite y la ruta no es válida.</li>";
    guia += "</ul>";
    guia += "<p>Pase el ratón sobre la celda de estado para ver los detalles.</p>";

    guia += "<h3>Consejos prácticos</h3>";
    guia += "<ul>";
    guia += "<li>Comience con una ruta generada automáticamente (clic derecho en el mapa) y luego ajuste los puntos.</li>";
    guia += "<li>Evite cambios bruscos de altitud: distribuya el desnivel en varios segmentos.</li>";
    guia += "<li>Para ala fija, mantenga la velocidad constante y use radios de giro amplios.</li>";
    guia += "<li>Revise siempre el análisis detallado (botón <b>Análisis</b>) para ver recomendaciones.</li>";
    guia += "<li>Verifique la autonomía: si el porcentaje de uso supera el 80%, considere reducir la distancia.</li>";
    guia += "<li>En zonas montañosas, aumente el margen de seguridad y revise el perfil cuidadosamente.</li>";
    guia += "</ul>";

    guia += "<h3>Métricas en pantalla</h3>";
    guia += "<p>En la parte inferior se muestran: distancia total, porcentaje de autonomía restante, tiempo estimado y estado de la ruta. "
            "El estado puede ser: <span style='color:green;'>Válida</span>, <span style='color:orange;'>Válida con advertencias</span>, "
            "<span style='color:red;'>Inválida</span> o <span style='color:blue;'>Ruta vacía</span>.</p>";

    guia += "<p><b>Recuerde:</b> Siempre valide la ruta antes de enviarla al dron. Una ruta bien planificada aumenta la seguridad y eficiencia del vuelo.</p>";

    label->setText(guia);
    contentLayout->addWidget(label);
    contentLayout->addStretch();
    contentWidget->setLayout(contentLayout);
    scrollArea->setWidget(contentWidget);

    mainLayout->addWidget(scrollArea);

    // Botón de cerrar
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    mainLayout->addWidget(buttonBox);

    dialog->exec();
    delete dialog; // opcional, pero como tiene this como padre, se destruirá al cerrar
}

// ------------------------------------------------------------
// MÉTODOS PRIVADOS AUXILIARES (UI)
// ------------------------------------------------------------

/**
 * @brief Actualiza el estado de los botones de la interfaz según si hay dron configurado y puntos.
 * @param configured true si hay dron configurado.
 */
void Widget_Puntos::updateUIForDroneConfigured(bool configured)
{
    bool tienePuntos = (modelo->rowCount() > 0);
    QString nombreRuta = modelo->getCurrentRouteName();
    bool rutaCargada = !nombreRuta.isEmpty();
    int numPuntos = modelo->rowCount();
    bool esDiezPuntos = (numPuntos == 10);

    if (configured && rutaCargada) {
        pB_CrearRuta->setEnabled(true);
        pB_EnviaRuta->setEnabled(tienePuntos && esDiezPuntos);
        pb_guardar->setEnabled(tienePuntos && esDiezPuntos);
        pB_EliminaRuta->setEnabled(rutaCargada);
        pB_LimpiaRuta->setEnabled(rutaCargada);
        pB_AnalisisDetallado->setEnabled(rutaCargada && tienePuntos);
        pb_Mostrar->setEnabled(true);

        if (!tienePuntos) {
            label_Estado->setText("Ruta vacía");
            label_Estado->setStyleSheet("color: blue; font-weight: bold;");
        } else if (esDiezPuntos) {
            auto validacion = modelo->validarRuta();
            QVariantMap analisis = modelo->analizarRutaDetallada();
            bool tieneAdvertencias = !analisis.value("puntos_criticos").toList().isEmpty() ||
                    analisis.value("num_puntos_criticos").toInt() > 0 ||
                    !modelo->obtenerRecomendaciones().isEmpty();

            if (validacion.first) {
                if (tieneAdvertencias) {
                    label_Estado->setText("Válida (con advertencias)");
                    label_Estado->setStyleSheet("color: orange; font-weight: bold;");
                } else {
                    label_Estado->setText("Válida");
                    label_Estado->setStyleSheet("color: green; font-weight: bold;");
                }
            } else {
                label_Estado->setText("Inválida");
                label_Estado->setStyleSheet("color: red; font-weight: bold;");
            }
        } else {
            label_Estado->setText(QString("Inválida (%1/10 puntos)").arg(numPuntos));
            label_Estado->setStyleSheet("color: red; font-weight: bold;");
            pB_EnviaRuta->setEnabled(false);
        }
    } else {
        pB_CrearRuta->setEnabled(true);
        pB_EnviaRuta->setEnabled(false);
        pb_guardar->setEnabled(false);
        pB_EliminaRuta->setEnabled(false);
        pB_LimpiaRuta->setEnabled(false);
        pB_AnalisisDetallado->setEnabled(false);
        label_Estado->setText("No configurado");
        label_Estado->setStyleSheet("color: red; font-weight: bold;");
        pb_Mostrar->setChecked(false);
        pb_Mostrar->setEnabled(false);
    }

    mostrarInfoDronActual();
}

/**
 * @brief Muestra la información del dron actual en la etiqueta correspondiente.
 */
void Widget_Puntos::mostrarInfoDronActual()
{
    if (modelo->isDroneConfigured()) {
        DroneCharacteristics drone = modelo->getDroneCharacteristics();
        labelInfoDron->setText(
                    QString("%1 (%2) | Vmax: %3 m/s | Autonomía: %4 m")
                    .arg(drone.nombre)
                    .arg(drone.tipoToString())
                    .arg(drone.maxSpeed, 0, 'f', 1)
                    .arg(drone.maxRange, 0, 'f', 0));
        labelInfoDron->setToolTip(
                    QString("%1 (%2)\n"
                            "Velocidad: %3-%4 m/s\n"
                            "Autonomía: %5 m\n"
                            "Endurance: %6 s\n"
                            "Altitud: %7-%8 m")
                    .arg(drone.nombre)
                    .arg(drone.tipoToString())
                    .arg(drone.minSpeed, 0, 'f', 1)
                    .arg(drone.maxSpeed, 0, 'f', 1)
                    .arg(drone.maxRange, 0, 'f', 0)
                    .arg(drone.endurance, 0, 'f', 0)
                    .arg(drone.minAltitude, 0, 'f', 0)
                    .arg(drone.maxAltitude, 0, 'f', 0));
    } else {
        labelInfoDron->setText("No configurado");
        labelInfoDron->setToolTip("");
    }
}

/**
 * @brief Limpia todo (reinicia el modelo y la interfaz).
 */
void Widget_Puntos::limpiarTodo()
{
    modelo->resetearModelo();
    label_Estado->setText("No configurado");
    label_Estado->setStyleSheet("color: gray; font-weight: bold;");
    labelInfoDron->setText("No configurado");
    label_Distancia->setText("0 m");
    label_AutonomiaRest->setText("0%");
    label_Tiempo->setText("--:--");
    progressAutonomia->setValue(0);
    updateUIForDroneConfigured(false);
}

/**
 * @brief Habilita o deshabilita los botones de gestión de ruta según el parámetro.
 * @param habilitar true para habilitar, false para deshabilitar.
 */
void Widget_Puntos::habilitarBotonesRuta(bool habilitar)
{
    bool tienePuntos = (modelo->rowCount() > 0);
    bool tieneRuta = !modelo->getCurrentRouteName().isEmpty();
    bool esDiezPuntos = (modelo->rowCount() == 10);

    pB_EnviaRuta->setEnabled(habilitar && tienePuntos && tieneRuta && esDiezPuntos);
    pb_guardar->setEnabled(habilitar && tienePuntos && tieneRuta && esDiezPuntos);
    pB_EliminaRuta->setEnabled(habilitar && tieneRuta);
    pB_LimpiaRuta->setEnabled(habilitar && tieneRuta);
    pB_AnalisisDetallado->setEnabled(habilitar && tienePuntos && tieneRuta);
}

/**
 * @brief Obtiene el nombre real de la tabla de ruta a partir del texto mostrado en el combo.
 * @param nombreTablaConFormato Texto del combo.
 * @return Nombre real de la tabla.
 */
QString Widget_Puntos::obtenerNombreTablaLimpio(const QString &nombreTablaConFormato)
{
    int index = cB_Rutas->currentIndex();
    if (index >= 0) {
        QVariant nombreReal = cB_Rutas->itemData(index, Qt::UserRole);
        if (nombreReal.isValid() && !nombreReal.toString().isEmpty())
            return nombreReal.toString();
    }
    QString nombreLimpio = nombreTablaConFormato;
    nombreLimpio = nombreLimpio.replace("✓", "").trimmed()
            .replace("⚠", "").trimmed()
            .replace("✗", "").trimmed();
    if (nombreLimpio.contains('('))
        nombreLimpio = nombreLimpio.split('(').first().trimmed();
    nombreLimpio = nombreLimpio.replace("(", "").replace(")", "").trimmed();
    return nombreLimpio;
}

/**
 * @brief Muestra un cuadro de diálogo con las recomendaciones.
 * @param recomendaciones Lista de cadenas con recomendaciones.
 */
void Widget_Puntos::mostrarRecomendaciones(const QStringList &recomendaciones)
{
    if (recomendaciones.isEmpty()) return;
    QString mensaje = "💡 RECOMENDACIONES PARA SU RUTA:\n\n";
    for (const auto &rec : recomendaciones)
        mensaje += "• " + rec + "\n";
    bool mostrar = false;
    for (const auto &rec : recomendaciones) {
        if (rec.contains("⚠") || rec.contains("✗")) {
            mostrar = true;
            break;
        }
    }
    if (mostrar)
        QMessageBox::information(this, "Recomendaciones", mensaje);
}

/**
 * @brief Muestra un cuadro con los detalles de los puntos críticos.
 * @param puntosCriticos Lista de mapas con información de puntos críticos.
 */
void Widget_Puntos::mostrarPuntosCriticosDetalles(const QVariantList &puntosCriticos)
{
    if (puntosCriticos.isEmpty()) {
        QMessageBox::information(this, "Puntos Críticos", "No se detectaron puntos críticos.");
        return;
    }

    QString mensaje = "🔴 PUNTOS CRÍTICOS DETECTADOS:\n\n";
    for (const auto &punto : puntosCriticos) {
        QVariantMap datos = punto.toMap();
        mensaje += QString("📍 %1 (ID: %2)\n").arg(datos["punto"].toString()).arg(datos["id"].toInt());
        mensaje += QString("   Nivel: %1\n").arg(datos["nivel"].toString());
        mensaje += QString("   Problemas:\n");
        for (const QString &problema : datos["problemas"].toStringList())
            mensaje += QString("   • %1\n").arg(problema);
        mensaje += "\n";
    }
    QMessageBox::information(this, "Puntos Críticos", mensaje);
}

/**
 * @brief Actualiza el panel de análisis (solo emite señal, sin cambiar estilo de label_Estado).
 * @param analisis Mapa con el análisis.
 */
void Widget_Puntos::actualizarPanelAnalisis(const QVariantMap &analisis)
{
    if (analisis.contains("error")) return;

    emit sI_AnalisisActualizado(analisis);
}

// ------------------------------------------------------------
// EVENTOS
// ------------------------------------------------------------

/**
 * @brief Maneja el evento de cierre para emitir la señal closed().
 */
void Widget_Puntos::closeEvent(QCloseEvent *event)
{
    emit closed();
    QWidget::closeEvent(event);
}

/**
 * @brief Inicia el arrastre del widget al presionar el botón izquierdo.
 */
void Widget_Puntos::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

/**
 * @brief Mueve el widget durante el arrastre.
 */
void Widget_Puntos::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}

/**
 * @brief Finaliza el arrastre.
 */
void Widget_Puntos::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}

/**
 * @brief Filtro de eventos para la barra de título (permite arrastrar desde ella).
 */
bool Widget_Puntos::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragPosition = mouseEvent->globalPos() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_dragging && (mouseEvent->buttons() & Qt::LeftButton)) {
                move(mouseEvent->globalPos() - m_dragPosition);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragging = false;
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
