// El informe de la revision: lo mismo que ve la tabla, ordenado y con las
// cifras de la ruta y de cada tramo, y la lista de lo que se comprueba.

#include "InformeRuta.h"

#include <QStringList>
#include <cmath>

namespace {
using Gravedad = Hallazgo::Gravedad;

// "1 error", "2 errores".
QString cuenta(int n, const QString &uno, const QString &varios)
{
    return QStringLiteral("%1 %2").arg(n).arg(n == 1 ? uno : varios);
}

// Duracion como 4:05.
QString duracion(double segundos)
{
    const int s = int(std::lround(segundos));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

// Metros, en km con un decimal a partir de 1 km.
QString distancia(double m)
{
    return m >= 1000.0 ? QStringLiteral("%1 km").arg(m / 1000.0, 0, 'f', 1)
                       : QStringLiteral("%1 m").arg(m, 0, 'f', 0);
}

// Lo que pide un tramo en vertical: "sube 2.5 m/s", "baja 1.0 m/s" o "—".
QString vertical(double tasa)
{
    if (std::fabs(tasa) < 0.05)
        return QStringLiteral("—");
    return QStringLiteral("%1 %2 m/s").arg(tasa > 0 ? QStringLiteral("sube") : QStringLiteral("baja"))
        .arg(std::fabs(tasa), 0, 'f', 1);
}

// "16.3 m/s (59 km/h)" o "de 12.0 a 18.0 m/s (43 a 65 km/h)".
QString velocidades(const RevisionRuta &r)
{
    if (std::fabs(r.velocidadMax - r.velocidadMin) < 0.05)
        return QStringLiteral("%1 m/s (%2 km/h) en todos los puntos").arg(r.velocidadMin, 0, 'f', 1)
            .arg(r.velocidadMin * 3.6, 0, 'f', 0);
    return QStringLiteral("de %1 a %2 m/s (%3 a %4 km/h)").arg(r.velocidadMin, 0, 'f', 1).arg(r.velocidadMax, 0, 'f', 1)
        .arg(r.velocidadMin * 3.6, 0, 'f', 0).arg(r.velocidadMax * 3.6, 0, 'f', 0);
}
}

namespace InformeRuta {

// Lo que se comprueba y lo que se supone, para que se sepa de donde sale cada cosa.
QStringList comoSeRevisa()
{
    return {
        QStringLiteral("La altura de cada punto sobre el terreno de debajo (la altura sobre el mar menos la "
                       "cota), dentro de la mínima y la máxima del dron; sin datos de elevación, se cuenta desde el "
                       "nivel del mar."),
        QStringLiteral("La velocidad de cada punto, dentro de los límites del dron (los de su configuración)."),
        QStringLiteral("La distancia entre puntos, entre la mínima y la máxima del dron."),
        QStringLiteral("La velocidad: si le da para acelerar o frenar de un punto al siguiente con su aceleración "
                       "máxima (v² = v₀² ± 2·a·d)."),
        QStringLiteral("Las subidas y bajadas, con su tasa máxima: si no le da, se sigue desde la altura a la que "
                       "llega de verdad, no desde la del plan."),
        QStringLiteral("El terreno, cada %1 m de cada tramo y con esa altura real: a menos de %2 m es error y a "
                       "menos de %3 m, aviso.")
            .arg(RoutePlanner::kPasoTerreno, 0, 'f', 0)
            .arg(RoutePlanner::kHolguraMinima, 0, 'f', 0)
            .arg(RoutePlanner::kHolguraAviso, 0, 'f', 0),
        QStringLiteral("Si el choque viene de que el dron no llega a la altura del plan (no le da para subir), el "
                       "arreglo no es subir los puntos: volar más despacio, empezar la subida más alto o hacerla más "
                       "larga."),
        QStringLiteral("La ladera: si el terreno sube más empinado de lo que sube el dron a esa velocidad (tasa de "
                       "subida ÷ velocidad), aviso, aunque los puntos estén altos."),
        QStringLiteral("La trayectoria probable (discontinua en el mapa y en el perfil): en planta, con el radio de "
                       "giro de cada velocidad y cambiando de punto al entrar en el radio de acercamiento de Mandos; "
                       "en altura, con sus tasas de subida y bajada. Es una estimación: sin viento ni la ley de "
                       "guiado real del autopiloto."),
        QStringLiteral("Los giros del ala fija: el radio sale de la velocidad con %1° de alabeo; si el punto "
                       "siguiente queda dentro del círculo de giro, no puede ir directo.")
            .arg(RoutePlanner::kAlabeoGrados, 0, 'f', 0),
        QStringLiteral("La autonomía, en distancia y en tiempo: pasarse es error; usar más del %1 %, aviso (hay "
                       "que ir hasta el primer punto y volver).")
            .arg(RoutePlanner::kUsoAutonomiaAviso * 100.0, 0, 'f', 0),
        QStringLiteral("Error: no se debería enviar. Aviso: el dron la vuela, pero no exactamente como está "
                       "planeada. Nota: algo que no se ha podido comprobar."),
        QStringLiteral("La columna Radio no se comprueba: no se manda al dron (la trama del plan no la lleva). "
                       "El radio de acercamiento es el de Mandos, el mismo para todos los puntos."),
    };
}

// Nombre de la gravedad, como se lee en la tabla y en el informe.
QString nombreGravedad(Hallazgo::Gravedad g)
{
    switch (g) {
    case Gravedad::Error: return QStringLiteral("Error");
    case Gravedad::Aviso: return QStringLiteral("Aviso");
    default: return QStringLiteral("Nota");
    }
}

// Rojo, ambar o gris azulado.
QString colorGravedad(Hallazgo::Gravedad g)
{
    switch (g) {
    case Gravedad::Error: return QStringLiteral("#c62828");
    case Gravedad::Aviso: return QStringLiteral("#b26a00");
    default: return QStringLiteral("#546e7a");
    }
}

// El estado en una frase, con cuantos errores y avisos.
QString estado(const RevisionRuta &r)
{
    const int e = r.cuantos(Gravedad::Error), a = r.cuantos(Gravedad::Aviso);
    if (e > 0)
        return QStringLiteral("No válida: %1%2").arg(cuenta(e, QStringLiteral("error"), QStringLiteral("errores")),
                                                     a > 0 ? QStringLiteral(" y ") + cuenta(a, QStringLiteral("aviso"),
                                                                                            QStringLiteral("avisos"))
                                                           : QString());
    if (a > 0)
        return QStringLiteral("Válida, con %1").arg(cuenta(a, QStringLiteral("aviso"), QStringLiteral("avisos")));
    return QStringLiteral("Válida");
}

// El informe en HTML para un QTextBrowser (tablas sencillas: es lo que pinta bien).
QString html(const RevisionRuta &r, const DroneCharacteristics &dron, const QString &ruta,
             const QList<E_Punto> &puntos)
{
    auto esc = [](const QString &s) { return s.toHtmlEscaped(); };
    const QString colorEstado = !r.valida() ? colorGravedad(Gravedad::Error)
                                : r.cuantos(Gravedad::Aviso) > 0 ? colorGravedad(Gravedad::Aviso)
                                                                 : QStringLiteral("#2e7d32");
    QString h;
    h += QStringLiteral("<h2 style='margin-bottom:0'>Ruta «%1»</h2>").arg(esc(ruta));
    h += QStringLiteral("<p style='color:#888; margin-top:2px'>%1 (%2) · %3 puntos</p>")
             .arg(esc(dron.nombre), esc(dron.tipoToString())).arg(puntos.size());
    h += QStringLiteral("<p style='font-size:large; color:%1'><b>%2</b></p>").arg(colorEstado, esc(estado(r)));

    // Cifras
    QString terreno;
    if (r.hayTerreno)
        terreno = QStringLiteral("pasa a %1 m del terreno como mínimo (cada %2 m de cada tramo)")
                      .arg(r.holguraTerreno, 0, 'f', 0).arg(RoutePlanner::kPasoTerreno, 0, 'f', 0);
    else
        terreno = QStringLiteral("sin datos de elevación: no comprobado");
    const QList<QPair<QString, QString>> cifras{
        {QStringLiteral("Distancia"), QStringLiteral("%1 (el %2 % de la autonomía)")
                                          .arg(distancia(r.distancia)).arg(r.usoDistancia * 100.0, 0, 'f', 0)},
        {QStringLiteral("Tiempo"), QStringLiteral("%1 min (el %2 % de la batería)")
                                       .arg(duracion(r.tiempo)).arg(r.usoTiempo * 100.0, 0, 'f', 0)},
        {QStringLiteral("Alturas"), QStringLiteral("de %1 a %2 m sobre el mar (sube %3 m y baja %4 m en total)")
                                        .arg(r.alturaMin, 0, 'f', 0).arg(r.alturaMax, 0, 'f', 0)
                                        .arg(r.subida, 0, 'f', 0).arg(r.bajada, 0, 'f', 0)},
        {QStringLiteral("Sobre el terreno"), std::isfinite(r.sobreTerrenoMin)
             ? QStringLiteral("los puntos, de %1 a %2 m (el dron admite de %3 a %4 m)")
                   .arg(r.sobreTerrenoMin, 0, 'f', 0).arg(r.sobreTerrenoMax, 0, 'f', 0)
                   .arg(dron.minAltitude, 0, 'f', 0).arg(dron.maxAltitude, 0, 'f', 0)
             : QStringLiteral("sin datos de elevación")},
        {QStringLiteral("Velocidades"), velocidades(r)},
        {QStringLiteral("Entre puntos"), terreno},
    };
    h += QStringLiteral("<table cellspacing='0' cellpadding='2'>");
    for (const auto &c : cifras)
        h += QStringLiteral("<tr><td style='color:#888; padding-right:12px'>%1</td><td>%2</td></tr>")
                 .arg(c.first, esc(c.second));
    h += QStringLiteral("</table>");

    // Que revisar
    h += QStringLiteral("<h3>Qué revisar</h3>");
    if (r.hallazgos.isEmpty())
        h += QStringLiteral("<p style='color:#2e7d32'>Nada: la ruta está dentro de lo que puede hacer el dron.</p>");
    for (const Hallazgo &x : r.hallazgos) {
        const QString color = colorGravedad(x.gravedad);
        const QString lugar = x.desde >= 0
                ? QStringLiteral("<a href='punto:%1'>%2</a>").arg(x.desde).arg(esc(x.lugar))
                : esc(x.lugar);
        h += QStringLiteral("<table width='100%' cellspacing='0' cellpadding='4' style='margin-bottom:6px'><tr>"
                            "<td width='5' bgcolor='%1'></td><td>"
                            "<b style='color:%1'>%2</b> · %3 — <b>%4</b><br>%5<br>"
                            "<b>Cómo arreglarlo:</b> %6</td></tr></table>")
                 .arg(color, nombreGravedad(x.gravedad), lugar, esc(x.titulo), esc(x.explicacion), esc(x.solucion));
    }

    // Tramos
    if (!r.tramos.isEmpty()) {
        h += QStringLiteral("<h3>Tramos</h3><table border='1' cellspacing='0' cellpadding='3' "
                            "style='border-color:#888'><tr>"
                            "<th>Tramo</th><th>Distancia</th><th>Tiempo</th><th>Pide</th>"
                            "<th>Llega a</th><th>Sobre el terreno</th></tr>");
        for (const TramoRuta &t : r.tramos) {
            const int b = t.desde + 1;
            if (b >= puntos.size())
                break;
            const bool noLlega = std::fabs(t.alturaLlegada - puntos[b].altura) > 1.0;
            const QString llega = noLlega
                    ? QStringLiteral("<span style='color:%1'>%2 m (plan: %3)</span>")
                          .arg(colorGravedad(Gravedad::Aviso))
                          .arg(t.alturaLlegada, 0, 'f', 0).arg(puntos[b].altura, 0, 'f', 0)
                    : QStringLiteral("%1 m").arg(t.alturaLlegada, 0, 'f', 0);
            QString suelo = QStringLiteral("—");
            if (std::isfinite(t.holguraTerreno)) {
                const QString c = t.holguraTerreno < RoutePlanner::kHolguraMinima ? colorGravedad(Gravedad::Error)
                                  : t.holguraTerreno < RoutePlanner::kHolguraAviso ? colorGravedad(Gravedad::Aviso)
                                                                                   : QString();
                suelo = c.isEmpty() ? QStringLiteral("%1 m").arg(t.holguraTerreno, 0, 'f', 0)
                                    : QStringLiteral("<span style='color:%1'>%2 m</span>").arg(c).arg(t.holguraTerreno, 0, 'f', 0);
            }
            h += QStringLiteral("<tr><td><a href='punto:%1'>%2 → %3</a></td><td align='right'>%4</td>"
                                "<td align='right'>%5</td><td align='right'>%6</td><td align='right'>%7</td>"
                                "<td align='right'>%8</td></tr>")
                     .arg(t.desde).arg(esc(puntos[t.desde].nombre), esc(puntos[b].nombre), distancia(t.distancia),
                                       t.tiempo > 0 ? duracion(t.tiempo) : QStringLiteral("—"),
                                       vertical(t.tasaVertical), llega, suelo);
        }
        h += QStringLiteral("</table>");
    }

    // Como se revisa
    h += QStringLiteral("<h3>Cómo se revisa</h3><ul>");
    for (const QString &linea : comoSeRevisa())
        h += QStringLiteral("<li>%1</li>").arg(esc(linea));
    h += QStringLiteral("</ul>");
    return h;
}

// El mismo informe en texto, para el fichero que se exporta.
QString texto(const RevisionRuta &r, const DroneCharacteristics &dron, const QString &ruta,
              const QList<E_Punto> &puntos)
{
    QStringList t;
    t << QStringLiteral("REVISIÓN DE LA RUTA «%1»").arg(ruta)
      << QStringLiteral("%1 (%2) · %3 puntos").arg(dron.nombre, dron.tipoToString()).arg(puntos.size())
      << QString()
      << estado(r)
      << QString()
      << QStringLiteral("Distancia:   %1 (el %2 % de la autonomía)").arg(distancia(r.distancia))
             .arg(r.usoDistancia * 100.0, 0, 'f', 0)
      << QStringLiteral("Tiempo:      %1 min (el %2 % de la batería)").arg(duracion(r.tiempo))
             .arg(r.usoTiempo * 100.0, 0, 'f', 0)
      << QStringLiteral("Alturas:     de %1 a %2 m sobre el mar").arg(r.alturaMin, 0, 'f', 0).arg(r.alturaMax, 0, 'f', 0)
      << QStringLiteral("Velocidades: %1").arg(velocidades(r))
      << (r.hayTerreno ? QStringLiteral("Terreno:     a %1 m como mínimo").arg(r.holguraTerreno, 0, 'f', 0)
                       : QStringLiteral("Terreno:     sin datos, no comprobado"))
      << QString()
      << QStringLiteral("QUÉ REVISAR");
    if (r.hallazgos.isEmpty())
        t << QStringLiteral("Nada.");
    for (const Hallazgo &x : r.hallazgos)
        t << QString()
          << QStringLiteral("[%1] %2: %3").arg(nombreGravedad(x.gravedad), x.lugar, x.titulo)
          << QStringLiteral("    %1").arg(x.explicacion)
          << QStringLiteral("    Cómo arreglarlo: %1").arg(x.solucion);
    t << QString() << QStringLiteral("CÓMO SE REVISA");
    for (const QString &linea : comoSeRevisa())
        t << QStringLiteral("- %1").arg(linea);
    return t.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace InformeRuta
