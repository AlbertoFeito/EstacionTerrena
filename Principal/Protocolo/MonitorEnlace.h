#ifndef MONITORENLACE_H
#define MONITORENLACE_H

#include <QQueue>
#include <QString>

/*!
 * \brief Estado del enlace con el dron, a partir de cuando llega cada trama.
 *
 * Sin esto, si se perdia la telemetria el dron se quedaba quieto en el mapa sin
 * que nada lo dijera. La estacion le avisa al conectar y desconectar y con cada
 * trama de telemetria; el indicador de la barra de estado le pregunta el estado
 * cada poco. Solo Qt Core; la hora se le da (en ms) para poder probarlo sin
 * esperar.
 *
 * - Desconectado: no hay enlace abierto.
 * - Esperando:    enlace abierto, aun no ha llegado ninguna trama.
 * - Bien:         la ultima trama llego hace como mucho kLento ms.
 * - Lento:        hace mas de kLento ms (aviso).
 * - Perdido:      hace mas de kPerdido ms (alarma).
 */
class MonitorEnlace
{
public:
    enum class Estado { Desconectado, Esperando, Bien, Lento, Perdido };

    static constexpr qint64 kLento = 1000;     //!< ms sin tramas para "lento"
    static constexpr qint64 kPerdido = 3000;   //!< ms sin tramas para "perdido"
    static constexpr qint64 kVentana = 2000;   //!< ms para contar tramas/s

    //! Enlace abierto (\a descripcion: "tcp 127.0.0.1:5760", "COM5"...) o cerrado.
    //! Al cerrarlo, se queda lo que duro (duracionUltima()).
    void setConectado(bool conectado, const QString &descripcion, qint64 ahoraMs)
    {
        if (m_conectado && !conectado)
            m_duracionUltima = ahoraMs - m_desde;
        m_conectado = conectado;
        m_descripcion = conectado ? descripcion : QString();
        m_desde = ahoraMs;
        m_ultima = -1;
        m_tramas.clear();
    }

    //! Ha llegado una trama de telemetria.
    void registrarTrama(qint64 ahoraMs)
    {
        if (!m_conectado)
            return;
        m_ultima = ahoraMs;
        m_tramas.enqueue(ahoraMs);
        podar(ahoraMs);
    }

    //! Estado ahora.
    Estado estado(qint64 ahoraMs) const
    {
        if (!m_conectado)
            return Estado::Desconectado;
        if (m_ultima < 0)
            return ahoraMs - m_desde > kPerdido ? Estado::Perdido : Estado::Esperando;
        const qint64 hace = ahoraMs - m_ultima;
        if (hace > kPerdido)
            return Estado::Perdido;
        if (hace > kLento)
            return Estado::Lento;
        return Estado::Bien;
    }

    //! Tramas por segundo en los ultimos kVentana ms.
    double tramasPorSegundo(qint64 ahoraMs)
    {
        podar(ahoraMs);
        return m_tramas.size() * 1000.0 / kVentana;
    }

    //! Ms desde la ultima trama (o desde que se conecto, si aun no hubo), -1 sin enlace.
    qint64 msDesdeUltima(qint64 ahoraMs) const
    {
        if (!m_conectado)
            return -1;
        return ahoraMs - (m_ultima >= 0 ? m_ultima : m_desde);
    }

    //! Ms que lleva abierto el enlace, -1 sin enlace.
    qint64 msConectado(qint64 ahoraMs) const
    {
        return m_conectado ? ahoraMs - m_desde : -1;
    }

    //! Ms que duro la ultima conexion ya cerrada, -1 si aun no hubo ninguna.
    qint64 duracionUltima() const { return m_duracionUltima; }

    //! Una duracion como "00:12:34" (horas, minutos y segundos).
    static QString formatoDuracion(qint64 ms)
    {
        const qint64 s = qMax<qint64>(0, ms) / 1000;
        return QStringLiteral("%1:%2:%3")
            .arg(s / 3600, 2, 10, QLatin1Char('0'))
            .arg(s / 60 % 60, 2, 10, QLatin1Char('0'))
            .arg(s % 60, 2, 10, QLatin1Char('0'));
    }

    //! Rotulo del tiempo de conexion: "Conectado 00:12:34" con enlace,
    //! "Última conexión 00:12:34" despues, vacio si no se ha conectado nunca.
    QString textoTiempo(qint64 ahoraMs) const
    {
        if (m_conectado)
            return QStringLiteral("Conectado %1").arg(formatoDuracion(msConectado(ahoraMs)));
        if (m_duracionUltima >= 0)
            return QStringLiteral("Última conexión %1").arg(formatoDuracion(m_duracionUltima));
        return QString();
    }

    //! Ya ha llegado alguna trama desde que se conecto.
    bool hayTramas() const { return m_ultima >= 0; }
    //! Por donde se habla ("tcp 127.0.0.1:5760", "COM5"...), vacia sin enlace.
    QString descripcion() const { return m_descripcion; }

    //! Texto del indicador: "tcp 127.0.0.1:5760 · 10 tramas/s · última hace 0,1 s".
    QString texto(qint64 ahoraMs)
    {
        const Estado e = estado(ahoraMs);
        if (e == Estado::Desconectado)
            return QStringLiteral("Sin enlace");
        const double segundos = msDesdeUltima(ahoraMs) / 1000.0;
        if (!hayTramas()) {
            if (e == Estado::Perdido)
                return QStringLiteral("%1 · SIN DATOS desde que se conectó (%2 s)")
                    .arg(m_descripcion).arg(segundos, 0, 'f', 0);
            return QStringLiteral("%1 · esperando datos…").arg(m_descripcion);
        }
        if (e == Estado::Perdido)
            return QStringLiteral("%1 · SIN DATOS hace %2 s").arg(m_descripcion).arg(segundos, 0, 'f', 0);
        return QStringLiteral("%1 · %2 tramas/s · última hace %3 s")
            .arg(m_descripcion)
            .arg(tramasPorSegundo(ahoraMs), 0, 'f', 0)
            .arg(QString::number(segundos, 'f', 1).replace('.', ','));
    }

private:
    // Olvida las tramas fuera de la ventana.
    void podar(qint64 ahoraMs)
    {
        while (!m_tramas.isEmpty() && ahoraMs - m_tramas.head() > kVentana)
            m_tramas.dequeue();
    }

    bool m_conectado = false;
    QString m_descripcion;
    qint64 m_desde = 0;
    qint64 m_ultima = -1;
    qint64 m_duracionUltima = -1;
    QQueue<qint64> m_tramas;
};

#endif // MONITORENLACE_H
