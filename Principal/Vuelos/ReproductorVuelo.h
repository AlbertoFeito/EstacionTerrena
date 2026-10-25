#ifndef REPRODUCTORVUELO_H
#define REPRODUCTORVUELO_H

#include "GrabadorVuelo.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QVector>

/*!
 * \brief Un plan del vuelo grabado y desde cuando estaba en el dron (ms desde
 *        el principio del vuelo; negativo si ya lo tenia al empezar).
 */
struct PlanGrabado
{
    qint64 desdeMs = 0;
    QVector<PuntoPlan> puntos;
};

/*!
 * \brief Reproduce un vuelo grabado lo mas parecido posible a como fue.
 *
 * Antes la reproduccion mandaba una fila cada 200 ms (o menos, segun el
 * deslizador), fuera cual fuera el tiempo real entre muestras, y saltaba de
 * una a otra. Ahora:
 * - el tiempo es el de la grabacion (la fecha de cada muestra), multiplicado
 *   por la velocidad elegida (x0,5 ... x16);
 * - entre dos muestras se interpola (posicion, altura, velocidad, actitud y
 *   rumbos; estos por el lado corto): el dron se mueve suave, sin saltos de
 *   100 ms; si entre dos muestras hay mas de 2 s (un corte), no se inventa;
 * - con cada fotograma va el plan que el dron tenia en ese momento, para
 *   pintar la ruta, los cumplidos y la linea al proximo punto.
 *
 * Solo Qt Core + Sql: se prueba sin interfaz.
 */
class ReproductorVuelo : public QObject
{
    Q_OBJECT
public:
    explicit ReproductorVuelo(QObject *parent = nullptr);

    //! Carga el vuelo \a vuelo del dron \a dron. false si no tiene muestras.
    bool cargar(QSqlDatabase db, int dron, int vuelo);
    //! Carga muestras y planes ya leidos (las fechas de las muestras mandan).
    void cargar(const QVector<MuestraVuelo> &muestras, const QVector<PlanGrabado> &planes);

    const QVector<MuestraVuelo> &muestras() const { return m_muestras; }
    //! Milisegundos de cada muestra desde la primera.
    const QVector<qint64> &tiempos() const { return m_t; }
    qint64 duracionMs() const { return m_t.isEmpty() ? 0 : m_t.last(); }

    //! Muestra interpolada en el instante \a ms (desde la primera).
    MuestraVuelo muestraEn(qint64 ms) const;
    //! Indice de la ultima muestra en o antes de \a ms (-1 si no hay).
    int indiceEn(qint64 ms) const;
    //! Plan que tenia el dron en \a ms (-1 si ninguno).
    int planEn(qint64 ms) const;
    const QVector<PlanGrabado> &planes() const { return m_planes; }

    void reproducir();
    void pausar();
    //! Para y vuelve al principio.
    void parar();
    //! Salta al instante \a ms (y lo muestra).
    void irA(qint64 ms);
    //! Factor de velocidad (1 = tiempo real).
    void setVelocidad(double factor);
    double velocidad() const { return m_factor; }
    bool reproduciendo() const { return m_reloj.isActive(); }
    qint64 posicionMs() const;

signals:
    //! Lo que hay que mostrar en el instante \a ms.
    void fotograma(const MuestraVuelo &m, qint64 ms);
    //! Cambia el plan del dron (vacio: ninguno).
    void planCambiado(const QVector<PuntoPlan> &puntos);
    //! Se llego al final.
    void terminado();

private:
    void emitir(qint64 ms);

    QVector<MuestraVuelo> m_muestras;
    QVector<qint64> m_t;
    QVector<PlanGrabado> m_planes;
    QTimer m_reloj;
    QElapsedTimer m_desde;   // tiempo real desde el ultimo cambio
    qint64 m_baseMs = 0;     // posicion del vuelo en ese cambio
    double m_factor = 1.0;
    int m_planEmitido = -2;
};

#endif // REPRODUCTORVUELO_H
