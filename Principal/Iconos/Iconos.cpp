#include "Iconos.h"

#include <QPainter>
#include <QPainterPath>

#include <functional>

namespace Iconos {

namespace {
// Color de los iconos de la barra: gris azulado, legible en claro y en oscuro.
const QColor kTrazo(0x78, 0x90, 0x9c);

// Lienzo transparente de lado x lado con antialiasing, en coordenadas 0..24
// (como los iconos de 24 px) para dibujar igual a cualquier tamano.
QPixmap lienzo(int lado, const std::function<void(QPainter &)> &dibujar)
{
    QPixmap px(lado, lado);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(lado / 24.0, lado / 24.0);
    dibujar(p);
    return px;
}

// Icono de barra a 16, 24 y 32 px con el mismo dibujo.
QIcon icono(const std::function<void(QPainter &)> &dibujar)
{
    QIcon i;
    for (int lado : {16, 24, 32})
        i.addPixmap(lienzo(lado, dibujar));
    return i;
}

// Pluma de trazo de los iconos de barra.
QPen pluma(double ancho = 2.0)
{
    return QPen(kTrazo, ancho, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

// Lupa: lente y mango; \a signo: 0 nada, +1 mas, -1 menos.
void dibujarLupa(QPainter &p, int signo)
{
    p.setPen(pluma());
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(10, 10), 6.5, 6.5);
    p.setPen(pluma(3.0));
    p.drawLine(QPointF(15, 15), QPointF(21, 21));
    p.setPen(pluma(1.8));
    if (signo != 0)
        p.drawLine(QPointF(7, 10), QPointF(13, 10));
    if (signo > 0)
        p.drawLine(QPointF(10, 7), QPointF(10, 13));
}
}

// Silueta de avion: fuselaje, alas en flecha y cola, rellena del color y con
// borde casi negro.
QPixmap avion(const QColor &color, int lado)
{
    return lienzo(lado, [&](QPainter &p) {
        QPainterPath a;
        a.moveTo(12, 1.5);            // morro
        a.cubicTo(13.2, 2.5, 13.2, 6, 13.2, 8);
        a.lineTo(22.5, 13);           // ala derecha
        a.lineTo(22.5, 15);
        a.lineTo(13.2, 12.5);
        a.lineTo(13, 18.5);
        a.lineTo(16.5, 21);           // cola derecha
        a.lineTo(16.5, 22.5);
        a.lineTo(12, 21.3);
        a.lineTo(7.5, 22.5);          // cola izquierda
        a.lineTo(7.5, 21);
        a.lineTo(11, 18.5);
        a.lineTo(10.8, 12.5);
        a.lineTo(1.5, 15);            // ala izquierda
        a.lineTo(1.5, 13);
        a.lineTo(10.8, 8);
        a.cubicTo(10.8, 6, 10.8, 2.5, 12, 1.5);
        a.closeSubpath();
        p.setPen(QPen(QColor(20, 20, 20), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(color);
        p.drawPath(a);
    });
}

// Cuadroptero: dos brazos en X, cuatro rotores y el cuerpo; un triangulo marca
// el frente.
QPixmap cuadroptero(const QColor &color, int lado)
{
    return lienzo(lado, [&](QPainter &p) {
        const QColor borde(20, 20, 20);
        p.setPen(QPen(borde, 3.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(5, 5), QPointF(19, 19));
        p.drawLine(QPointF(19, 5), QPointF(5, 19));
        p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(5, 5), QPointF(19, 19));
        p.drawLine(QPointF(19, 5), QPointF(5, 19));
        p.setPen(QPen(borde, 1.0));
        QColor rotor = color;
        rotor.setAlpha(170);
        p.setBrush(rotor);
        for (const QPointF &c : {QPointF(5, 5), QPointF(19, 5), QPointF(5, 19), QPointF(19, 19)})
            p.drawEllipse(c, 4.0, 4.0);
        p.setBrush(color);
        p.drawEllipse(QPointF(12, 12), 3.5, 3.5);
        p.drawPolygon(QPolygonF({QPointF(12, 5.5), QPointF(10, 9), QPointF(14, 9)}));
    });
}

// Regla en diagonal con sus marcas.
QIcon regla()
{
    return icono([](QPainter &p) {
        p.translate(12, 12);
        p.rotate(-45);
        p.setPen(pluma());
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(-10, -3.5, 20, 7), 1.5, 1.5);
        p.setPen(pluma(1.4));
        for (int i = -6; i <= 6; i += 3)
            p.drawLine(QPointF(i, -3.5), QPointF(i, i % 2 == 0 ? 0.5 : -0.8));
    });
}

// Lupa con un recuadro punteado al lado.
QIcon lupaArea()
{
    return icono([](QPainter &p) {
        p.setPen(QPen(kTrazo, 1.3, Qt::DashLine));
        p.drawRect(QRectF(2, 2, 9, 9));
        p.translate(3, 3);
        p.scale(0.85, 0.85);
        dibujarLupa(p, 0);
    });
}

// Lupa con "+".
QIcon lupaMas()
{
    return icono([](QPainter &p) { dibujarLupa(p, +1); });
}

// Lupa con "-".
QIcon lupaMenos()
{
    return icono([](QPainter &p) { dibujarLupa(p, -1); });
}

// Pentagono con sus vertices marcados.
QIcon poligono()
{
    return icono([](QPainter &p) {
        const QPolygonF v({QPointF(12, 3), QPointF(21, 9.5), QPointF(17.5, 20),
                           QPointF(6.5, 20), QPointF(3, 9.5)});
        p.setPen(pluma());
        QColor relleno = kTrazo;
        relleno.setAlpha(60);
        p.setBrush(relleno);
        p.drawPolygon(v);
        p.setBrush(kTrazo);
        p.setPen(Qt::NoPen);
        for (const QPointF &q : v)
            p.drawEllipse(q, 2.0, 2.0);
    });
}

// Triangulo de "play" y, para el estado pulsado, las dos barras de "pausa".
QIcon reproducir()
{
    QIcon i;
    for (int lado : {16, 24, 32}) {
        i.addPixmap(lienzo(lado, [](QPainter &p) {
            p.setPen(Qt::NoPen);
            p.setBrush(kTrazo);
            p.drawPolygon(QPolygonF({QPointF(7, 4), QPointF(20, 12), QPointF(7, 20)}));
        }), QIcon::Normal, QIcon::Off);
        i.addPixmap(lienzo(lado, [](QPainter &p) {
            p.setPen(Qt::NoPen);
            p.setBrush(kTrazo);
            p.drawRoundedRect(QRectF(6, 4, 4.5, 16), 1, 1);
            p.drawRoundedRect(QRectF(13.5, 4, 4.5, 16), 1, 1);
        }), QIcon::Normal, QIcon::On);
    }
    return i;
}

// Cuadrado de "stop".
QIcon parar()
{
    return icono([](QPainter &p) {
        p.setPen(Qt::NoPen);
        p.setBrush(kTrazo);
        p.drawRoundedRect(QRectF(5, 5, 14, 14), 2, 2);
    });
}

} // namespace Iconos
