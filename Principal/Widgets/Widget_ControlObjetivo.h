#ifndef WIDGET_CONTROLOBJETIVO_H
#define WIDGET_CONTROLOBJETIVO_H


#include <QtWidgets>
#include "Database/ConnectionManager.h"
#include "Database/AsyncQuery.h"
#include "Database/AsynqQueryModel.h"
#include "header/flexibleheaderview.h"

#include "Reporte/qecsreport.h"
#include "Widget_Graficas.h"

#define TIME_BASE 200 //ms

#include <QTimer>

struct s_datosVuelos
{
    int Id_Vuelos;
    int Dron_ID;
    double Lat;
    double Lon;
    double Realimentacion_Cabeceo;
    double Realimentacion_Banqueo;
    double Realimentacion_Velocidad;
    double Realimentacion_Altura;
    double Realimentacion_Curso;
    double Distancia_Chequeo;
    double Curso_Deseado;
    int Realimentacion_Regimen;
    int Realimentacion_ComandoMision;
    double Realimentacion_CursoIMO;
    int CantidadSatelites;

    double KPCabeceo;
    double KICabeceo;
    double KPBanqueo;
    double KIBanqueo;
    double KPVelocidad;
    double KIVelocidad;
    double KPAltura;
    double KIAltura;
    double KPCurso;
    double KPRumbo;
    double KIRumbo;

    double Control_Estabilizadores;
    double Control_Alerones;
    double Control_Motor;
    double Control_Rumbo;

    double Convergencia_Banqueo;
    double Convergencia_Cabeceo;

    QDateTime Fecha;

    s_datosVuelos( int sId_Vuelos,
                   int sDron_ID,
                   double sLat,
                   double sLon,
                   double sRealimentacion_Cabeceo,
                   double sRealimentacion_Banqueo,
                   double sRealimentacion_Velocidad,
                   double sRealimentacion_Altura,
                   double sRealimentacion_Curso,
                   double sDistancia_Chequeo,
                   double sCurso_Deseado,
                   int sRealimentacion_Regimen,
                   int sRealimentacion_ComandoMision,
                   double sRealimentacion_CursoIMO,
                   int sCantidadSatelites,

                   double sKPCabeceo,
                   double sKICabeceo,
                   double sKPBanqueo,
                   double sKIBanqueo,
                   double sKPVelocidad,
                   double sKIVelocidad,
                   double sKPAltura,
                   double sKIAltura,
                   double sKPCurso,
                   double sKPRumbo,
                   double sKIRumbo,

                   double sControl_Estabilizadores,
                   double sControl_Alerones,
                   double sControl_Motor,
                   double sControl_Rumbo,

                   double sConvergencia_Banqueo,
                   double sConvergencia_Cabeceo,
                   QDateTime sFecha)

    {
        Id_Vuelos =sId_Vuelos;
        Dron_ID =  sDron_ID;
        Lat=       sLat;
        Lon=       sLon;
        Realimentacion_Cabeceo      = sRealimentacion_Cabeceo;
        Realimentacion_Banqueo      = sRealimentacion_Banqueo;
        Realimentacion_Velocidad    = sRealimentacion_Velocidad;
        Realimentacion_Altura       = sRealimentacion_Altura;
        Realimentacion_Curso        = sRealimentacion_Curso;
        Distancia_Chequeo           = sDistancia_Chequeo;
        Curso_Deseado               = sCurso_Deseado;
        Realimentacion_Regimen      = sRealimentacion_Regimen;
        Realimentacion_ComandoMision= sRealimentacion_ComandoMision;
        Realimentacion_CursoIMO = sRealimentacion_CursoIMO;
        CantidadSatelites           = sCantidadSatelites;
        KPCabeceo                 = sKPCabeceo;
        KICabeceo                 = sKICabeceo;
        KPBanqueo                 = sKPBanqueo;
        KIBanqueo                 = sKIBanqueo;
        KPVelocidad                 = sKPVelocidad;
        KIVelocidad                 = sKIVelocidad;
        KPAltura                  = sKPAltura;
        KIAltura                  = sKIAltura;
        KPCurso                  = sKPCurso;
        KPRumbo                  = sKPRumbo;
        KIRumbo                  = sKIRumbo;
        Control_Estabilizadores     = sControl_Estabilizadores;
        Control_Alerones      = sControl_Alerones;
        Control_Motor      = sControl_Motor;
        Control_Rumbo      = sControl_Rumbo;
        Convergencia_Banqueo   = sConvergencia_Banqueo;
        Convergencia_Cabeceo   = sConvergencia_Cabeceo;
        Fecha=     sFecha;
    }
};



namespace Ui {
class Widget_ControlObjetivo;
}

class Widget_ControlObjetivo : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_ControlObjetivo(QWidget *parent = nullptr);
    ~Widget_ControlObjetivo();

    QStringList getDrones() const;
    void setDrones(const QStringList &value);

    QString getDronActivo() const;
    void setDronActivo(const QString &value);

    QString getVuelo() const;
    void setVuelo(const QString &value);

    float Map(float x, float in_min, float in_max, float out_min, float out_max);


    Widget_Graficas *getWGraficas() const;
    void setWGraficas(Widget_Graficas *value);

    QTime getControlObjetivoTime() const;
    void setControlObjetivoTime(const QTime &value);

private slots:

    void on_pB_Ver_ControlObjetivo_clicked();

    void on_pB_Graficar_clicked();

    void on_cB_Drones_currentIndexChanged(const QString &arg1);

    void on_cB_Vuelos_currentIndexChanged(const QString &arg1);

    void on_hS_Velocidad_valueChanged(int value);

    void on_pB_Stop_clicked();

    void on_pB_Play_toggled(bool checked);

    void on_hS_Avanzar_valueChanged(int value);

    void on_hS_Avanzar_sliderPressed();

    void on_hS_Avanzar_sliderReleased();

signals:
    void datoContolObjetivo(QByteArray);
    void limpiarDatoContolObjetivo(QString);
    void detenerDatoContolObjetivo();
    void iniciarDatoContolObjetivo();
private:
    Ui::Widget_ControlObjetivo *ui;
    QStringList drones;
    QString dronActivo;
    QString Vuelo;
    Database::AsyncQueryModel *_queryModel;
    QTableView *tV;
    QWidget *wDatos;
    QPushButton *pB_Imprimir;

    bool queryVuelos();
    bool queryDrones();
    Widget_Graficas *wGraficas;

    QList<s_datosVuelos> listDatosVuelos;
    QTimer *clkVelocidad;
    QTime controlObjetivoTime;
    int posDatoActual;
    void configuraComponentesPlay();
    void prepara_envia_datos(int pos);
    bool estadoSimulacion;
};

#endif // WIDGET_CONTROLOBJETIVO_H
