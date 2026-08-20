#include "Ci_Principal.h"
#include "ui_Ci_Principal.h"

#include <QFile>

Ci_Principal::Ci_Principal(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Ci_Principal)
{
    ui->setupUi(this);

    claro = ":/qdarkstyle/light/lightstyle.qss";
    oscuro = ":/qdarkstyle/dark/darkstyle.qss";

    m_setStyleSheet(oscuro);
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

void Ci_Principal::m_setStyleSheet(QString &qss)
{
    QFile f(qss);
    if (!f.exists())
    {
    }
    else
    {
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);

        qApp->setStyleSheet(ts.readAll());
    }
}

void Ci_Principal::m_CurrentStyleS(bool style)
{
    if(style )
        m_setStyleSheet(claro);
    else
        m_setStyleSheet(oscuro);
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
