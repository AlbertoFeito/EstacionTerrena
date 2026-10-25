#include "PanelFlotante.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

// Cabecera con el titulo a la izquierda y la cruz de cerrar a la derecha.
PanelFlotante::PanelFlotante(const QString &titulo, QWidget *padre)
    : QFrame(padre)
{
    m_caja = new QVBoxLayout(this);
    m_caja->setContentsMargins(8, 4, 8, 6);
    m_caja->setSpacing(2);

    auto *cabecera = new QHBoxLayout();
    auto *etiqueta = new QLabel(titulo);
    QFont f = etiqueta->font();
    f.setBold(true);
    etiqueta->setFont(f);
    cabecera->addWidget(etiqueta);
    cabecera->addStretch();
    auto *cerrar = new QToolButton();
    cerrar->setText(QStringLiteral("✕"));
    cerrar->setAutoRaise(true);
    cerrar->setToolTip(tr("Cerrar"));
    connect(cerrar, &QToolButton::clicked, this, [this]() {
        hide();
        emit cerrado();
    });
    cabecera->addWidget(cerrar);
    m_caja->addLayout(cabecera);
}

// Sustituye el contenido (si habia otro, lo quita del panel sin borrarlo).
void PanelFlotante::setContenido(QWidget *contenido)
{
    if (m_contenido)
        m_caja->removeWidget(m_contenido);
    m_contenido = contenido;
    m_caja->addWidget(contenido, 1);
}

// Fondo del color de ventana del tema, casi opaco y con esquinas redondeadas,
// y un borde suave para separarlo del mapa.
void PanelFlotante::paintEvent(QPaintEvent *evento)
{
    Q_UNUSED(evento);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor fondo = palette().color(QPalette::Window);
    fondo.setAlpha(250);   // casi opaco: el mapa no se trasluce bajo el texto
    QColor borde = palette().color(QPalette::WindowText);
    borde.setAlpha(50);
    QPainterPath camino;
    camino.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    p.fillPath(camino, fondo);
    p.setPen(QPen(borde, 1));
    p.drawPath(camino);
}

// Clic en un hueco del panel: se queda aqui (no arrastra el mapa de debajo).
void PanelFlotante::mousePressEvent(QMouseEvent *evento)
{
    evento->accept();
}

// Arrastre sobre el panel: tampoco llega al mapa.
void PanelFlotante::mouseMoveEvent(QMouseEvent *evento)
{
    evento->accept();
}

// Soltar el boton sobre el panel.
void PanelFlotante::mouseReleaseEvent(QMouseEvent *evento)
{
    evento->accept();
}

// Doble clic en un hueco: el mapa no hace zoom.
void PanelFlotante::mouseDoubleClickEvent(QMouseEvent *evento)
{
    evento->accept();
}

// La rueda sobre el panel no acerca ni aleja el mapa.
void PanelFlotante::wheelEvent(QWheelEvent *evento)
{
    evento->accept();
}
