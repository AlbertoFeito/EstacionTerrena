#ifndef WIDGET_MANDOS_H
#define WIDGET_MANDOS_H

#include <QWidget>
#include <QDebug>
#include <QGeoCoordinate>
#include <cprojection.h>
#include <QDoubleSpinBox>
#include <QTimer>
#include "Estructuras/Estructuras.h"
#include "MyQSettings/myqsetting.h"

namespace Ui {
class Widget_Mandos;
}

class Widget_Mandos : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Mandos(CProjection *projection, sMANDOS *MANDOS, QWidget *parent = nullptr);
    ~Widget_Mandos();

    quint16 calculateChecksum(const QByteArray &data);



    QDoubleSpinBox *getDsB_Lon() const;

    QDoubleSpinBox *getDsB_Lat() const;
public slots:
    void on_AcuseRecibo();
    void on_restoreStyle();
private slots:
    void on_pB_Enviar_clicked();

    void on_rB_M_toggled(bool checked);

    void on_rB_SA_toggled(bool checked);

    void on_rB_A_toggled(bool checked);

    void on_rB_Defoult_toggled(bool checked);

    void on_rB_Circular_toggled(bool checked);

    void on_rB_Repetir_Mision_toggled(bool checked);

    void on_rB_RTL_toggled(bool checked);

    void on_chB_Paracaidas_toggled(bool checked);

private:
    Ui::Widget_Mandos *ui;
    unsigned char regimen = 0;
    unsigned char comando = 0;
    unsigned char paracaidas = 0;
        /* Modo
        M - manual
        S - semi auto
        A - auto
        */
    sMANDOS *mandos;

    void configuraUI();

    CProjection *m_projection;
    QDoubleSpinBox *m_dsB_Lat,*m_dsB_Lon;

    QTimer m_timer;
    QString m_originalStyle;

    QString fileName;
MyQSetting settings;

signals:
    void enviaMandosDron(QByteArray);
};

#endif // WIDGET_MANDOS_H
