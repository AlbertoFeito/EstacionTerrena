#include "PanelVuelo.h"

#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>

#include <cmath>

namespace {
// Colores del panel: los del avion segun el regimen y los de alerta.
const QColor kBlanco(0xff, 0xff, 0xff);
const QColor kGris(0x9e, 0x9e, 0x9e);
const QColor kRojo(0xff, 0x52, 0x52);
const QColor kAmbar(0xff, 0xca, 0x28);
const QColor kVerde(0x69, 0xf0, 0xae);

const char *kCampos[] = {"regimen", "mision", "punto", "distancia", "rumbo", "satelites"};
const char *kTitulos[] = {"RÉGIMEN", "MISIÓN", "PUNTO", "DISTANCIA", "RUMBO → DESEADO", "SATÉLITES"};
// El texto mas largo de cada campo: su columna nace con ese ancho, asi el
// panel no cambia de tamaño (ni corta valores) cuando llegan los datos.
const char *kMasLargos[] = {"Estabilizado", "Repetir", "P10 / 10", "9,99 km", "000° → 000°", "99"};

// Rumbo con tres cifras ("095°"), o un guion si no se sabe.
QString rumboTexto(double grados)
{
    if (std::isnan(grados))
        return QStringLiteral("—");
    const int g = (int(std::lround(grados)) % 360 + 360) % 360;
    return QStringLiteral("%1°").arg(g, 3, 10, QLatin1Char('0'));
}
}

// Monta las tarjetas (titulo pequeño y valor grande) sobre un fondo oscuro
// semitransparente que deja ver el mapa, y la linea de la posicion debajo.
PanelVuelo::PanelVuelo(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("PanelVuelo"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "#PanelVuelo { background-color: rgba(20, 24, 30, 205); border-radius: 8px; }"
        "#PanelVuelo QLabel { background: transparent; }"
        "#PanelVuelo QLabel[rol=\"titulo\"] { color: #9e9e9e; font-size: 10px; font-weight: bold; }"
        "#PanelVuelo QLabel[rol=\"valor\"] { font-size: 18px; font-weight: bold; }"
        "#PanelVuelo QLabel[rol=\"pie\"] { font-size: 11px; }"));

    auto *rejilla = new QGridLayout(this);
    rejilla->setContentsMargins(12, 6, 12, 6);
    rejilla->setHorizontalSpacing(16);
    rejilla->setVerticalSpacing(0);
    for (int i = 0; i < 6; ++i) {
        auto *titulo = new QLabel(QString::fromUtf8(kTitulos[i]), this);
        titulo->setProperty("rol", "titulo");
        auto *valor = new QLabel(this);
        valor->setProperty("rol", "valor");
        QFont negrita = valor->font();
        negrita.setPixelSize(18);
        negrita.setBold(true);
        valor->setMinimumWidth(QFontMetrics(negrita).horizontalAdvance(QString::fromUtf8(kMasLargos[i])) + 6);
        rejilla->addWidget(titulo, 0, i);
        rejilla->addWidget(valor, 1, i);
        m_valores.insert(QString::fromLatin1(kCampos[i]), valor);
    }
    auto *pie = new QLabel(this);
    pie->setProperty("rol", "pie");
    rejilla->addWidget(pie, 2, 0, 1, 6);
    m_valores.insert(QStringLiteral("posicion"), pie);
    limpiar();
}

// Pone texto y color a un campo.
void PanelVuelo::poner(const QString &campo, const QString &texto, const QColor &color)
{
    QLabel *l = m_valores.value(campo);
    if (!l)
        return;
    l->setText(texto);
    l->setProperty("color", color);
    l->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
}

// Todo a guiones, en gris.
void PanelVuelo::limpiar()
{
    for (auto it = m_valores.cbegin(); it != m_valores.cend(); ++it)
        poner(it.key(), QStringLiteral("—"), kGris);
    m_textoPosicion = tr("Sin datos del dron");
    const QString aviso = m_aviso;
    m_aviso.clear();
    setAviso(aviso);   // repone el aviso que hubiera, o la posicion
}

// Aviso en rojo en lugar de la posicion (enlace perdido); vacio, la posicion.
void PanelVuelo::setAviso(const QString &aviso)
{
    m_aviso = aviso;
    poner(QStringLiteral("posicion"), m_aviso.isEmpty() ? m_textoPosicion : QStringLiteral("⚠ ") + m_aviso,
          m_aviso.isEmpty() ? kGris : kRojo);
}

// Rellena cada campo con su formato y su color de alerta.
void PanelVuelo::mostrar(const Datos &d)
{
    static const char *regimenes[] = {"Manual", "Estabilizado", "Automático"};
    static const QColor coloresRegimen[] = {kRojo, kAmbar, kVerde};
    if (d.regimen >= 0 && d.regimen <= 2)
        poner(QStringLiteral("regimen"), QString::fromUtf8(regimenes[d.regimen]),
              coloresRegimen[d.regimen]);

    static const char *misiones[] = {"Puntos", "Circular", "Repetir", "RTL"};
    if (d.mision >= 0 && d.mision <= 3)
        poner(QStringLiteral("mision"), QString::fromUtf8(misiones[d.mision]), kBlanco);

    if (d.totalPuntos > 0 && d.puntosCumplidos >= d.totalPuntos)
        poner(QStringLiteral("punto"), QStringLiteral("✓ %1/%1").arg(d.totalPuntos), kVerde);
    else if (d.totalPuntos > 0 && d.puntoActual >= 0)
        poner(QStringLiteral("punto"), QStringLiteral("P%1 / %2").arg(d.puntoActual + 1).arg(d.totalPuntos),
              kBlanco);
    else
        poner(QStringLiteral("punto"), QStringLiteral("—"), kGris);

    if (!std::isnan(d.distanciaPunto)) {
        const QString t = d.distanciaPunto < 1000.0
            ? QStringLiteral("%1 m").arg(d.distanciaPunto, 0, 'f', 0)
            : QStringLiteral("%1 km").arg(QString::number(d.distanciaPunto / 1000.0, 'f', 2)
                                              .replace('.', ','));
        poner(QStringLiteral("distancia"), t, kBlanco);
    }

    if (!std::isnan(d.rumbo) || !std::isnan(d.rumboDeseado))
        poner(QStringLiteral("rumbo"),
              QStringLiteral("%1 → %2").arg(rumboTexto(d.rumbo), rumboTexto(d.rumboDeseado)), kBlanco);

    if (d.satelites >= 0)
        poner(QStringLiteral("satelites"), QString::number(d.satelites),
              d.satelites < 6 ? kRojo : d.satelites < 8 ? kAmbar : kBlanco);

    if (d.posicion.isValid()) {
        m_textoPosicion = tr("Dron: %1, %2").arg(d.posicion.latitude(), 0, 'f', 5)
                                            .arg(d.posicion.longitude(), 0, 'f', 5);
        if (m_aviso.isEmpty())
            poner(QStringLiteral("posicion"), m_textoPosicion, kGris);
    }
}

// Texto de un campo.
QString PanelVuelo::valor(const QString &campo) const
{
    const QLabel *l = m_valores.value(campo);
    return l ? l->text() : QString();
}

// Color de un campo.
QColor PanelVuelo::color(const QString &campo) const
{
    const QLabel *l = m_valores.value(campo);
    return l ? l->property("color").value<QColor>() : QColor();
}
