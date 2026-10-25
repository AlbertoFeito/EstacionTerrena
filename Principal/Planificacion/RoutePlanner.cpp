// Motor de validacion y analisis de rutas (antes dentro de Widget_Puntos.cpp).

#include "RoutePlanner.h"

#include <QtMath>
#include <algorithm>
#include <cmath>
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
    double cambioVelocidad = std::abs(siguiente.velocidad - actual.velocidad);
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
    if (std::abs(diferenciaAltura) < 0.1) return true;

    double pendiente = actual.calcularPendiente(siguiente);
    if (std::abs(pendiente) > 30.0) {
        errorMsg = QString("Pendiente entre %1 y %2 es %3°, máximo 30°")
                .arg(actual.nombre).arg(siguiente.nombre)
                .arg(pendiente, 0, 'f', 1);
        return false;
    }

    double velocidadPromedio = (actual.velocidad + siguiente.velocidad) / 2.0;
    if (velocidadPromedio < 0.01) return true;

    double tiempoEstimado = distancia / velocidadPromedio;
    if (tiempoEstimado < 0.01) return true;

    double tasaVertical = std::abs(diferenciaAltura) / tiempoEstimado;
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

        double cambio1 = std::abs(actual.velocidad - anterior.velocidad);
        double cambio2 = std::abs(siguiente.velocidad - actual.velocidad);
        if (cambio1 > droneChars.maxAcceleration * 0.7 && cambio2 > droneChars.maxAcceleration * 0.7)
            cambiosBruscosVelocidad++;

        double pendiente1 = anterior.calcularPendiente(actual);
        double pendiente2 = actual.calcularPendiente(siguiente);
        if (std::abs(pendiente1) > 20 && std::abs(pendiente2) > 20)
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
            variacionTotal += std::abs(punto.velocidad - velocidadPromedio);
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
            else descensoTotal += std::abs(diferencia);
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

        double cambioVelocidad = std::abs(siguiente.velocidad - previo.velocidad);
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
        if (std::abs(pendiente1) > 25 || std::abs(pendiente2) > 25) {
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
        if (std::abs(segmento["pendiente"].toDouble()) > 15) dificultad += 2;
        if (std::abs(inicio.velocidad - fin.velocidad) > droneChars.maxAcceleration * 0.5) dificultad += 1;
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
        for (const auto &punto : puntos) variacionTotal += std::abs(punto.velocidad - velocidadPromedio);
        double variacionPromedio = variacionTotal / puntos.size();
        if (variacionPromedio > velocidadPromedio * 0.3) margen -= 10.0;
    }

    int pendientesPronunciadas = 0;
    for (int i = 0; i < puntos.size() - 1; ++i) {
        double pendiente = puntos[i].calcularPendiente(puntos[i+1]);
        if (std::abs(pendiente) > 20) pendientesPronunciadas++;
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
        if (std::abs(pendiente) > 15) {
            double factorPendiente = 1.0 + std::abs(pendiente) / 45.0;
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
            double cambioVelocidad = std::abs(puntos[i].velocidad - puntos[i-1].velocidad);
            estresPunto += cambioVelocidad / droneChars.maxAcceleration * 0.2;
            double diferenciaAltura = puntos[i].altura - puntos[i-1].altura;
            double tasaMaxima = diferenciaAltura > 0 ? droneChars.maxClimbRate : droneChars.maxDescentRate;
            estresPunto += std::abs(diferenciaAltura) / (tasaMaxima * 10) * 0.2;
        }
        if (i > 0 && i < puntos.size() - 1) {
            double angulo = calcularAnguloGiro(puntos[i-1], puntos[i], puntos[i+1]);
            estresPunto += (angulo / 180.0) * 0.3;
        }
        estres += estresPunto;
    }
    return estres / puntos.size() * 100;
}
