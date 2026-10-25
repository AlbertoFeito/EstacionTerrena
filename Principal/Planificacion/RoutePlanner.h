#ifndef ROUTEPLANNER_H
#define ROUTEPLANNER_H

// Revision de rutas: comprueba una ruta contra lo que puede hacer el dron y
// devuelve lo que ha visto, cada cosa con su porque y como arreglarla.
// Solo Qt Core y Positioning: se prueba sin pantalla (test_planificador).

#include "Estructuras/E_Punto.h"
#include "Planificacion/CaracteristicasDron.h"

#include <QGeoCoordinate>
#include <QList>
#include <QString>
#include <QVector>

#include <functional>
#include <limits>

//! Algo que la revision ha visto en la ruta: que pasa, por que importa (con
//! los numeros y el limite del dron) y que cambiar para arreglarlo.
struct Hallazgo
{
    //! Error: incumple un limite del dron o pasa por el terreno; la ruta no
    //! deberia enviarse. Aviso: el dron la vuela, pero no como esta planeada
    //! (llega mas bajo, da una vuelta, no alcanza la velocidad...). Nota: algo
    //! que no se ha podido comprobar.
    enum class Gravedad { Error, Aviso, Nota };

    Gravedad gravedad = Gravedad::Aviso;
    QString codigo;        //!< "altura_baja", "giro_cerrado"... (para las pruebas)
    int desde = -1;        //!< punto (indice) o primero del tramo; -1: la ruta entera
    int hasta = -1;        //!< ultimo punto del tramo; -1 si es de un solo punto
    QString lugar;         //!< "P3", "P3 → P4" o "Ruta"
    QString titulo;        //!< corto: "Sube más rápido de lo que puede"
    QString explicacion;   //!< que pasa, con los numeros
    QString solucion;      //!< que cambiar, con valores

    //! true si toca al punto \a fila (es suyo o del tramo que empieza o acaba en el).
    bool tocaA(int fila) const { return fila >= 0 && (fila == desde || fila == hasta); }
};

//! Un tramo de la ruta (del punto \c desde al siguiente), para el informe.
struct TramoRuta
{
    int desde = 0;
    double distancia = 0.0;      //!< m
    double tiempo = 0.0;         //!< s (0 si el tramo va a velocidad cero)
    double tasaVertical = 0.0;   //!< m/s que pide el plan (+ sube, - baja)
    double alturaLlegada = 0.0;  //!< m a los que llega de verdad (limitado por la tasa del dron)
    double holguraTerreno = std::numeric_limits<double>::quiet_NaN();  //!< m minimos sobre el terreno
};

//! Lo que sale de revisar una ruta: los hallazgos y las cifras del informe.
struct RevisionRuta
{
    QVector<Hallazgo> hallazgos;   //!< errores primero, luego avisos y notas
    QVector<TramoRuta> tramos;

    double distancia = 0.0;        //!< m
    double tiempo = 0.0;           //!< s
    double usoDistancia = 0.0;     //!< fraccion de la autonomia en distancia (1 = toda)
    double usoTiempo = 0.0;        //!< fraccion de la autonomia en tiempo
    double velocidadMin = 0.0, velocidadMax = 0.0;   //!< m/s
    double alturaMin = 0.0, alturaMax = 0.0;         //!< m
    double subida = 0.0, bajada = 0.0;               //!< m acumulados
    double holguraTerreno = std::numeric_limits<double>::quiet_NaN();  //!< m, la menor de la ruta
    bool hayTerreno = false;       //!< se ha podido comprobar el terreno (algun dato)

    //! true si no hay errores (los avisos no impiden enviarla).
    bool valida() const { return cuantos(Hallazgo::Gravedad::Error) == 0; }
    int cuantos(Hallazgo::Gravedad g) const;
    //! Los hallazgos que tocan al punto \a fila.
    QVector<Hallazgo> delPunto(int fila) const;
    //! Los errores en una linea cada uno ("P3: Por debajo de la altura mínima"), o
    //! "Ruta válida" si no hay.
    QString resumenErrores() const;
};

/**
 * @brief Revisa rutas segun las caracteristicas del dron.
 *
 * Mira todo de una vez (no se para en el primer problema) y simula lo que hara
 * el dron de verdad: si no le da para subir, el tramo siguiente empieza mas
 * bajo, y el terreno se compara con esa altura, no con la del plan.
 */
class RoutePlanner {
public:
    //! Cota del terreno en metros o NaN si no hay dato.
    using FuncionTerreno = std::function<double(const QGeoCoordinate &)>;

    //! Alabeo maximo supuesto en los giros del ala fija (el mismo de siempre).
    static constexpr double kAlabeoGrados = 30.0;
    //! Por debajo de esto sobre el terreno es error; por debajo de kHolguraAviso, aviso.
    static constexpr double kHolguraMinima = 10.0;
    static constexpr double kHolguraAviso = 30.0;
    //! Cada cuantos metros se mira el terreno a lo largo de un tramo.
    static constexpr double kPasoTerreno = 20.0;
    //! Usar mas de esta fraccion de la autonomia es aviso (hay que ir y volver).
    static constexpr double kUsoAutonomiaAviso = 0.8;

    explicit RoutePlanner(const DroneCharacteristics &chars);

    /**
     * @brief Revisa la ruta \a puntos.
     * @param terreno Cota del terreno; si no se da, no se mira el terreno.
     */
    RevisionRuta revisar(const QList<E_Punto> &puntos, const FuncionTerreno &terreno = {}) const;

    //! Radio de giro del ala fija a \a velocidad m/s: el de un giro coordinado
    //! con kAlabeoGrados de alabeo, r = v² / (g·tan φ), y nunca menos que el
    //! radio minimo configurado del dron.
    double radioDeGiro(double velocidad) const;

    //! Cuanto cambia el rumbo en \a p2 al ir de \a p1 a \a p3, en grados (0-180).
    double anguloGiro(const E_Punto &p1, const E_Punto &p2, const E_Punto &p3) const;

    //! Suma de las distancias entre puntos consecutivos, en metros.
    double calcularDistanciaTotal(const QList<E_Punto> &puntos) const;

    //! Tiempo de vuelo en segundos, cada tramo a la media de las velocidades de
    //! sus puntos (los tramos a velocidad cero no cuentan).
    double calcularTiempoVuelo(const QList<E_Punto> &puntos) const;

private:
    DroneCharacteristics droneChars; ///< Características del dron.
    double g = 9.81;                 ///< Aceleración de la gravedad (m/s²).
};

#endif // ROUTEPLANNER_H
