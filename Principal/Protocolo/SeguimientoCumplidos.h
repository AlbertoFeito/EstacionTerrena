#ifndef SEGUIMIENTOCUMPLIDOS_H
#define SEGUIMIENTOCUMPLIDOS_H

#include <QList>
#include <QPointF>
#include <QVector>

#include <algorithm>
#include <cmath>

/*!
 * \brief Que puntos del plan ha cumplido ya el dron, a partir de la telemetria.
 *
 * Con cada trama en automatico se le da el punto a cumplir que manda el dron
 * (Cumplir_Punto: norte, este en metros Mercator, tal como se le mando en el
 * plan) y la posicion del dron. Un punto queda cumplido:
 * - si el dron ya va hacia uno POSTERIOR (la regla de siempre: todos los
 *   anteriores al que va), o
 * - si el dron ha entrado en el radio de acercamiento del punto al que va. Sin
 *   esto el ULTIMO nunca se marcaba: tras el no hay "siguiente" al que ir.
 *
 * Solo Qt Core y sin estado de la interfaz: lo usa CControladora para pintar
 * los cumplidos y se prueba con las tramas del simulador.
 */
class SeguimientoCumplidos
{
public:
    //! Plan nuevo (confirmado por el dron): puntos (norte, este) en metros
    //! Mercator, como se mandaron. Olvida los cumplidos anteriores.
    void setPlan(const QList<QPointF> &planNorteEste)
    {
        m_plan = planNorteEste;
        m_cumplidos.fill(false, m_plan.size());
    }

    /*!
     * \brief Actualiza con una trama: \a cumplir es Cumplir_Punto (norte,
     *        este), \a dronNorteEste la posicion del dron en metros Mercator y
     *        \a radio el radio de acercamiento (metros Mercator).
     * \return los indices que acaban de quedar cumplidos, en orden.
     */
    QVector<int> actualizar(const QPointF &cumplir, const QPointF &dronNorteEste, double radio)
    {
        QVector<int> nuevos;
        const int indice = m_plan.lastIndexOf(cumplir);
        if (indice < 0)
            return nuevos;   // no va a ningun punto del plan (p. ej. vuelve a casa)
        for (int i = 0; i < indice; ++i)
            marcar(i, nuevos);
        const QPointF p = m_plan.at(indice);
        // Un margen de 5 m: la posicion llega en grados float y el plan en
        // metros enteros.
        if (std::hypot(dronNorteEste.x() - p.x(), dronNorteEste.y() - p.y()) <= radio + 5.0)
            marcar(indice, nuevos);
        return nuevos;
    }

    //! El punto \a i ya esta cumplido.
    bool cumplido(int i) const { return i >= 0 && i < m_cumplidos.size() && m_cumplidos.at(i); }
    //! Cuantos puntos van cumplidos.
    int cuantos() const { return int(std::count(m_cumplidos.cbegin(), m_cumplidos.cend(), true)); }

private:
    // Marca \a i si no lo estaba y lo anota en \a nuevos.
    void marcar(int i, QVector<int> &nuevos)
    {
        if (!m_cumplidos.at(i)) {
            m_cumplidos[i] = true;
            nuevos.append(i);
        }
    }

    QList<QPointF> m_plan;
    QVector<bool> m_cumplidos;
};

#endif // SEGUIMIENTOCUMPLIDOS_H
