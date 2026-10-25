#include "Ci_Principal.h"
#include "ui_Ci_Principal.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>

Ci_Principal::Ci_Principal(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Ci_Principal)
{
    ui->setupUi(this);

    ponerTema(false);
}

Ci_Principal::~Ci_Principal()
{
    delete ui;
}


QWidget *Ci_Principal::wMapa() const
{
    return m_wMapa;
}

void Ci_Principal::setWMapa(QWidget *wMapa)
{
    m_wMapa = wMapa;
}

// Tema claro u oscuro con la paleta de Fusion (antes, hojas de estilo qss de
// una carpeta de recursos que ya no esta). Los paneles sobre el mapa llevan su
// propio estilo y no cambian.
void Ci_Principal::ponerTema(bool claro)
{
    // Los widgets con hoja de estilo propia (p. ej. la pestaña de Mandos) no
    // recogen la paleta nueva hasta que se repulen.
    auto repulir = []() {
        for (QWidget *w : QApplication::allWidgets()) {
            w->style()->unpolish(w);
            w->style()->polish(w);
            w->update();
        }
    };
    if (claro) {
        qApp->setPalette(qApp->style()->standardPalette());
        repulir();
        return;
    }
    QPalette p;
    const QColor fondo(0x26, 0x2b, 0x33), base(0x1b, 0x1f, 0x25), texto(0xe0, 0xe0, 0xe0);
    const QColor apagado(0x7a, 0x80, 0x88), resalte(0x2f, 0x80, 0xed);
    p.setColor(QPalette::Window, fondo);
    p.setColor(QPalette::WindowText, texto);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, fondo);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, texto);
    p.setColor(QPalette::Text, texto);
    p.setColor(QPalette::Button, QColor(0x33, 0x3a, 0x44));
    p.setColor(QPalette::ButtonText, texto);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Link, resalte);
    p.setColor(QPalette::Highlight, resalte);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Light, QColor(0x3d, 0x45, 0x50));
    p.setColor(QPalette::Mid, QColor(0x20, 0x24, 0x2a));
    p.setColor(QPalette::Dark, QColor(0x15, 0x18, 0x1d));
    p.setColor(QPalette::Shadow, Qt::black);
    for (auto rol : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, rol, apagado);
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x44, 0x4a, 0x52));
    qApp->setPalette(p);
    repulir();
}

void Ci_Principal::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    emit resize();
}

void Ci_Principal::timerEvent(QTimerEvent *event)
{
    QMainWindow::timerEvent (event);
}

void Ci_Principal::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    emit move();

}
