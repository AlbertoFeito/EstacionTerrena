#include "PanelVuelo.h"

#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>

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

// Numero con dos decimales y coma, o un guion si no se sabe.
QString numero(double v)
{
    if (std::isnan(v))
        return QStringLiteral("—");
    return QString::number(v, 'f', 2).replace('.', ',');
}

// Filas de ganancias: nombre, campo y si lleva I ademas de P.
struct FilaGanancia { const char *nombre; const char *campo; bool conI; };
const FilaGanancia kGanancias[] = {
    {"Cabeceo", "g_cabeceo", true}, {"Banqueo", "g_banqueo", true},
    {"Velocidad", "g_velocidad", true}, {"Altura", "g_altura", true},
    {"Curso", "g_curso", false}, {"Rumbo", "g_rumbo", true},
};
const char *kActuadoresAla[] = {"Estabilizadores", "Alerones", "Motor", "Rumbo"};
const char *kActuadoresCuadro[] = {"Motor IF", "Motor DF", "Motor IT", "Motor DT"};
}

// Todo sin dato.
PanelVuelo::Detalle::Detalle()
{
    for (double &g : ganancias)
        g = std::numeric_limits<double>::quiet_NaN();
    for (double &a : actuadores)
        a = std::numeric_limits<double>::quiet_NaN();
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
        "#PanelVuelo QLabel[rol=\"pie\"] { font-size: 11px; }"
        "#PanelVuelo QLabel[rol=\"nombre\"] { color: #bdbdbd; font-size: 11px; }"
        "#PanelVuelo QLabel[rol=\"dato\"] { color: white; font-size: 11px; font-weight: bold; }"
        "#PanelVuelo QToolButton { color: #9e9e9e; background: transparent; border: none; font-size: 11px; }"
        "#PanelVuelo QToolButton:hover { color: white; }"
        "#DetalleVuelo { border-top: 1px solid rgba(255, 255, 255, 40); }"));

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
        // Con la fuente que de verdad le toca (la de la hoja de estilo) y con
        // margen: con solo la teorica, "Estabilizado" salia cortado.
        valor->ensurePolished();
        const QString largo = QString::fromUtf8(kMasLargos[i]);
        valor->setMinimumWidth(qMax(QFontMetrics(negrita).horizontalAdvance(largo),
                                    QFontMetrics(valor->font()).horizontalAdvance(largo)) + 12);
        rejilla->addWidget(titulo, 0, i);
        rejilla->addWidget(valor, 1, i);
        m_valores.insert(QString::fromLatin1(kCampos[i]), valor);
    }
    auto *pie = new QLabel(this);
    pie->setProperty("rol", "pie");
    rejilla->addWidget(pie, 2, 0, 1, 5);
    m_valores.insert(QStringLiteral("posicion"), pie);

    m_botonDetalle = new QToolButton(this);
    m_botonDetalle->setCheckable(true);
    m_botonDetalle->setCursor(Qt::PointingHandCursor);
    m_botonDetalle->setToolTip(tr("Ganancias, actuadores y convergencias"));
    rejilla->addWidget(m_botonDetalle, 2, 5, Qt::AlignRight);
    connect(m_botonDetalle, &QToolButton::toggled, this, [this](bool abierto) {
        setDetalleVisible(abierto);
        emit detalleConmutado(abierto);
    });

    m_detalle = crearDetalle();
    rejilla->addWidget(m_detalle, 3, 0, 1, 6);
    setDetalleVisible(false);
    limpiar();
}

// El detalle tecnico en tres bloques: ganancias (P / I), actuadores y otros
// (rumbo de la IMU y convergencias). Letra pequena: es para consultar.
QFrame *PanelVuelo::crearDetalle()
{
    auto *marco = new QFrame(this);
    marco->setObjectName(QStringLiteral("DetalleVuelo"));
    auto *rejilla = new QGridLayout(marco);
    rejilla->setContentsMargins(0, 6, 0, 2);
    rejilla->setHorizontalSpacing(10);
    rejilla->setVerticalSpacing(1);

    auto titulo = [&](const QString &texto, int columna) {
        auto *l = new QLabel(texto, marco);
        l->setProperty("rol", "titulo");
        rejilla->addWidget(l, 0, columna, 1, 2);
    };
    auto fila = [&](const QString &nombre, const QString &campo, int f, int columna) {
        auto *n = new QLabel(nombre, marco);
        n->setProperty("rol", "nombre");
        auto *v = new QLabel(marco);
        v->setProperty("rol", "dato");
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rejilla->addWidget(n, f, columna);
        rejilla->addWidget(v, f, columna + 1);
        m_valores.insert(campo, v);
        return n;
    };

    titulo(tr("GANANCIAS  P / I"), 0);
    int f = 1;
    for (const FilaGanancia &g : kGanancias)
        fila(QString::fromUtf8(g.nombre), QString::fromLatin1(g.campo), f++, 0);

    titulo(tr("ACTUADORES"), 2);
    for (int i = 0; i < 4; ++i)
        m_nombresActuador[i] = fila(QString::fromUtf8(kActuadoresAla[i]),
                                    QStringLiteral("act%1").arg(i), i + 1, 2);

    titulo(tr("OTROS"), 4);
    fila(tr("Rumbo IMU"), QStringLiteral("rumboImu"), 1, 4);
    fila(tr("Conv. banqueo"), QStringLiteral("convBanqueo"), 2, 4);
    fila(tr("Conv. cabeceo"), QStringLiteral("convCabeceo"), 3, 4);
    rejilla->setRowStretch(7, 1);
    return marco;
}

// Pone texto y color a un campo. Solo si cambian: la hoja de estilo de cada
// etiqueta cuesta rehacerla y llegan 10 tramas por segundo.
void PanelVuelo::poner(const QString &campo, const QString &texto, const QColor &color)
{
    QLabel *l = m_valores.value(campo);
    if (!l)
        return;
    if (l->text() != texto)
        l->setText(texto);
    if (l->property("color").value<QColor>() != color) {
        l->setProperty("color", color);
        l->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
    }
}

// Abre o pliega el detalle; el panel cambia de alto (quien lo coloca lo
// recoloca al recibir detalleConmutado o tras llamar a esto).
void PanelVuelo::setDetalleVisible(bool visible)
{
    m_detalle->setVisible(visible);
    {
        const QSignalBlocker sinSenal(m_botonDetalle);
        m_botonDetalle->setChecked(visible);
    }
    m_botonDetalle->setText(visible ? tr("Detalle ▴") : tr("Detalle ▾"));
    adjustSize();
}

// El detalle esta abierto.
bool PanelVuelo::detalleVisible() const
{
    return !m_detalle->isHidden();
}

// Rellena el detalle: ganancias "P / I", actuadores con su nombre segun el
// tipo de dron, rumbo de la IMU y convergencias.
void PanelVuelo::mostrarDetalle(const Detalle &d)
{
    int k = 0;
    for (const FilaGanancia &g : kGanancias) {
        QString t = numero(d.ganancias[k++]);
        if (g.conI)
            t += QStringLiteral(" / ") + numero(d.ganancias[k++]);
        poner(QString::fromLatin1(g.campo), t, kBlanco);
    }
    const char **nombres = d.tipoDron == 0 ? kActuadoresAla : kActuadoresCuadro;
    for (int i = 0; i < 4; ++i) {
        m_nombresActuador[i]->setText(QString::fromUtf8(nombres[i]));
        poner(QStringLiteral("act%1").arg(i), numero(d.actuadores[i]), kBlanco);
    }
    poner(QStringLiteral("rumboImu"), rumboTexto(d.rumboImu), kBlanco);
    poner(QStringLiteral("convBanqueo"), numero(d.convergenciaBanqueo), kBlanco);
    poner(QStringLiteral("convCabeceo"), numero(d.convergenciaCabeceo), kBlanco);
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

// Nombre del actuador i.
QString PanelVuelo::nombreActuador(int i) const
{
    return i >= 0 && i < 4 ? m_nombresActuador[i]->text() : QString();
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
