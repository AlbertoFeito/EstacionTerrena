#ifndef CCONTROLADORA_H
#define CCONTROLADORA_H

#include <QObject>
#include "LibMapaStatic.h"
#include "Ci_Principal.h"
#include "qfi/qfi_EADI.h"
#include <QPixmap>
#include <QGeoCoordinate>
#include "Widgets/Widget_Drones.h"
#include "Widgets/Widget_ControlObjetivo.h"
#include "Widgets/widget_mandos.h"
#include "Widgets/Widget_Conexion.h"
#include "Widgets/Widget_Graficas.h"
#include "Widgets/Widget_Puntos.h"
#include <stddef.h>
#include <stdio.h>
#include "autopiloto/CoordGPSaPosicion.h"
#include "autopiloto/rtwtypes.h"
#include "cgooglemercator.h"

#include <QTime>

#include <Database/AsyncQuery.h>

#include "AlturaWorker.h"
#include "LocationValidator.h"

#include "Widgets/Widget_Mandos_Cuadroptero.h"
#include "Widgets/Widget_Estado_Cuadroptero.h"

#include <Widgets/Widget_Ganancias_Control.h>
#include <Widgets/Widget_Parametros.h>

#include "Estructuras/Estructuras.h"

class CControladora : public QObject
{
    Q_OBJECT
public:
    explicit CControladora(QObject *parent = nullptr);

    void initPrincipal();
    void initTabOpciones();

    void conecciones();
    void preparaDataBase();

signals:
    void rutasActualizadas(); // <-- NUEVA

public slots:
    void Conectado(bool conectado);
    void recibirDatos(QByteArray byteArray);

    QList<QPointF> sL_cambiaProyeccion();

    void onNewTramaConstanteReceived(const char a, const char b, const char c, const sTRAMA1 &s_28, const sTRAMA2 &s_34);
    void onNewPlanificacionReceived(sPUNTOXYZ *puntosRecividos, int NumParametros);
    void onEnviaPlan();

    void onNewCuadropteros(const sCUADROPTEROS &s_cuadropteros);

    QPointF sL_cambiaProyeccionPunto(QGeoCoordinate pos);
    void updateParameters(const QVector<double> &values);

private slots:
    void on_EliminarPunto(QString s);
    void on_DetenerCO();
    void on_IniciarCO();
    void actualizarMenuRutas();          // <-- NUEVO
    void onRouteActionTriggered(QAction *action); // <-- NUEVO

private:
    Ci_Principal *mPrincipal;
    QDir RecursosDir;
    CGoogleMercator proj;
    QMenu *mMenu;
    QAction *aCerrar;

    QMenu *mTema;
    QActionGroup *aG_Tema;
    QAction *aClaro, *aOscuro;

    QMenu *mCapas;
    QActionGroup *aG_Capas;
    QAction *aSatelital, *aOSM;

    QMenu *mTrayectoria;
    QAction *a10, *a20, *a100, *a500, *a1000, *aTodas;

    QMenu *mRuta;
    QAction *aRuta,*aGestionarDrones;


    QMenu *mMenuMostrarRutas;            // <-- NUEVO
    QActionGroup *m_rutasActionGroup;           // <-- NUEVO
    QMap<QAction*, QString> m_actionToRoute; // <-- NUEVO
    QAction *a_ocultarRutaAction;

    QMenu *mEstadoDrone;
    QAction *aEstadoDrone;
    QAction *aVerGraficaDrone;
    QActionGroup *aG_EstadoDrone;

    QMenu *mLog;
    QAction *aLogger;

    QToolBar * toolBar;
    QAction *aDistancia,*aLupa,*aLupaMas,*aLupaMen,*aPoligono;

    void createMenus();
    void createActions();
    void createToolBar();

    LibMapaStatic *libMapa;

    qfi_EADI *indicador;
    QTabWidget *tabOpciones;

    Widget_Drones *m_wDrones;

    Widget_Estado_Cuadroptero *m_wEstadoCuadroptero;
    Widget_ControlObjetivo *m_wControlObjetivo;
    Widget_Mandos *m_wMandos;
    Widget_Mandos_Cuadroptero *m_wMandosCuadroptero;
    Widget_Conexion *m_wConexion;
    Widget_Graficas *m_wGraficas;
    Widget_Puntos *m_wPuntos;
    Widget_Ganancias_Control *m_wGanancias;

    Widget_Parametros *m_wParametros;

    bool m_ControlObjetivo = false;
    bool m_Conectado = false;
    int TimerId;
    QTime start_time_;
    int vueloID;
    int dronID;
    int tipoDrone = 0;
    QString DronID = "";

    //dataBase
    QString query1;
    Database::AsyncQuery *_aQuery;

    QGeoCoordinate geoPosOld;
    QGeoCoordinate geoPos;

//    bool bWidgetRuta = false;
    bool primeraCoordenada = false;

    QList<E_Punto> m_ListPuntos{};
    QList<QPointF> listPuntoF{};
    int indexAnterior = -1;

    // QObject interface
    void guardaDatosDron();

    // Puntos de ruta a enviar
    sPUNTOXYZ puntosEnviar[10];

    //       AlturaWorker alturaWorker;
    std::unique_ptr<AlturaWorker> alturaWorkerPTR;

    QTimer timer;

    //widget de parametros en Mapa
    QWidget* wParametros = nullptr;
    QList<QWidget*> listWidgetParametros;
    QFont fontParametros;

    // Componentes
    QWidget *leftPanel;
    QWidget *mainArea;
    // Widget overlay
    QWidget *overlayWidget;
    QLabel *overlayLabel;
    QPushButton *overlayButton;
    // Posicionamiento inicial
    void updateOverlayPosition();
    void updateRutaPosition();

    //rumboProximo punto
    float rumboProxPunto = 0;

    QVector<double> m_ParametrosValues;
    QVector<double> m_currentValues;
    QStringList listaParametros;
    void loadInitialData();
    QGeoCoordinate m_proximoPunto;

    sMANDOS mandos;

    LocationValidator validator;
    void updateStatus(double lat, double lon, bool isValid);
    bool eventFilter(QObject *obj, QEvent *event) override;

    // ===== EN LA SECCIÓN private =====
    bool m_esperandoPuntoInicial;

protected:
    void timerEvent(QTimerEvent *event) override;
};

#endif // CCONTROLADORA_H
