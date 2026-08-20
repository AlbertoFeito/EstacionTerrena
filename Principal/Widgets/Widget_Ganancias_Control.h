#ifndef WIDGET_GANANCIAS_CONTROL_H
#define WIDGET_GANANCIAS_CONTROL_H

#include <QTimer>
#include <QWidget>
#include <cprojection.h>
#include <QDebug>
#include <Estructuras/Estructuras.h>
#include <MyQSettings/myqsetting.h>

namespace Ui {
class Widget_Ganancias_Control;
}

class Widget_Ganancias_Control : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Ganancias_Control(CProjection *projection, sMANDOS *MANDOS,QWidget *parent = nullptr);
    ~Widget_Ganancias_Control();

signals:
    void enviaMandosDron(QByteArray);

private slots:
    void on_pB_Enviar_clicked();
public slots:
    void on_AcuseRecibo();
    void on_restoreStyle();
private:
    Ui::Widget_Ganancias_Control *ui;

    CProjection *m_projection;
    sMANDOS *mandos;
    QString Modo;
    unsigned char regimen;
    unsigned char comando;
    QTimer m_timer;
    QString m_originalStyle;

    quint16 calculateChecksum(const QByteArray &data);


    QString fileName;
    MyQSetting settings;
    void configuraUI();
};

#endif // WIDGET_GANANCIAS_CONTROL_H
