#ifndef WIDGET_CONTROLOBJETIVO_H
#define WIDGET_CONTROLOBJETIVO_H

#include <QtWidgets>
#include "header/flexibleheaderview.h"
#include "Reporte/qecsreport.h"
#include "Vuelos/ReproductorVuelo.h"
#include "Widget_Graficas.h"

class QSqlQueryModel;

namespace Ui {
class Widget_ControlObjetivo;
}

/*!
 * \brief Pestaña de Control de objetivo: vuelos grabados.
 *
 * Se elige dron y vuelo (sale el mas reciente) y se reproduce con
 * ReproductorVuelo, a tiempo real o mas rapido, pudiendo saltar a cualquier
 * momento; CControladora pinta cada fotograma como un vuelo en vivo (ruta del
 * plan, cumplidos, rumbos, panel e instrumentos). Ver Datos y Graficar
 * muestran el vuelo entero.
 */
class Widget_ControlObjetivo : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_ControlObjetivo(QWidget *parent = nullptr);
    ~Widget_ControlObjetivo();

    void setDrones(const QStringList &value);
    QString getDronActivo() const;
    QString getVuelo() const;
    int tipoDron() const;
    ReproductorVuelo *reproductor() const;
    Widget_Graficas *getWGraficas() const;

private slots:
    void on_pB_Ver_ControlObjetivo_clicked();
    void on_pB_Graficar_clicked();
    void on_cB_Drones_currentTextChanged(const QString &arg1);
    void on_cB_Vuelos_currentTextChanged(const QString &arg1);
    void on_hS_Velocidad_valueChanged(int value);
    void on_pB_Stop_clicked();
    void on_pB_Play_toggled(bool checked);
    void on_hS_Avanzar_valueChanged(int value);

signals:
    void limpiarDatoContolObjetivo(QString);
    void detenerDatoContolObjetivo();
    void iniciarDatoContolObjetivo();

private:
    void refrescarVuelos();
    void cargarVuelo(const QString &vuelo);
    void actualizarReloj(qint64 ms);

    Ui::Widget_ControlObjetivo *ui;
    ReproductorVuelo *m_reproductor;
    QString dronActivo;
    QString Vuelo;
    int m_idDron = -1;
    int m_tipoDron = 0;
    QSqlQueryModel *m_modeloDatos = nullptr;
    QTableView *tV;
    QWidget *wDatos;
    QPushButton *pB_Imprimir;
    Widget_Graficas *wGraficas;
    bool m_enReproduccion = false;   // de Play a Stop, tambien en pausa
};

#endif // WIDGET_CONTROLOBJETIVO_H
