#ifndef CCONTROLADORA_H
#define CCONTROLADORA_H

#include <QObject>
#include "Mapa/MapaMision.h"
#include "Mapa/PanelCapas.h"
#include "Widgets/PanelFlotante.h"
class VistaTelemetria;
#include "Protocolo/ProyeccionMercator.h"
#include "Protocolo/Tramas.h"
#include "Protocolo/SeguimientoCumplidos.h"
#include "Protocolo/MonitorEnlace.h"
#include "Widgets/PanelVuelo.h"
#include <QElapsedTimer>
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

#include <QTime>


#include "AlturaWorker.h"
#include "LocationValidator.h"

#include "Widgets/Widget_Mandos_Cuadroptero.h"
#include "Widgets/Widget_Estado_Cuadroptero.h"

#include <Widgets/Widget_Ganancias_Control.h>

#include "Estructuras/Estructuras.h"

#include "Vuelos/GrabadorVuelo.h"

class GrabadorVuelo;

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
    void mostrarMuestraReproducida(const MuestraVuelo &m);
    void pintarPlanReproducido(const QVector<PuntoPlan> &plan);

    QList<QPointF> sL_cambiaProyeccion();

    void onNewTramaConstanteReceived(const char a, const char b, const char c, const sTRAMA1 &s_28, const sTRAMA2 &s_34);
    void onNewPlanificacionReceived(sPUNTOXYZ *puntosRecividos, int NumParametros);
    void onEnviaPlan();

    void onNewCuadropteros(const sCUADROPTEROS &s_cuadropteros);

    QPointF sL_cambiaProyeccionPunto(QGeoCoordinate pos);

private slots:
    void on_EliminarPunto(QString s);
    void on_DetenerCO();
    void on_IniciarCO();
    void actualizarMenuRutas();          // <-- NUEVO
    void onRouteActionTriggered(QAction *action); // <-- NUEVO

private:
    Ci_Principal *mPrincipal;
    QMenu *mMenu;
    QAction *aCerrar;

    QMenu *mTema;
    QActionGroup *aG_Tema;
    QAction *aClaro, *aOscuro;

    // Menu Capas: una accion por capa base del paquete (se rehace con
    // actualizarMenuCapas) y el interruptor del panel de capas.
    QMenu *mCapas;
    QActionGroup *aG_Capas;
    PanelFlotante *m_flotanteCapas = nullptr;
    QAction *aPanelCapas = nullptr;
    PanelCapas *m_panelCapas = nullptr;
    void crearPanelCapas();
    void actualizarMenuCapas();
    void guardarCapas();

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

    MapaMision *m_mapa = nullptr;
    QLabel *m_lPosicionRaton = nullptr;
    QTimer *m_timerCota = nullptr;       // la cota se pide al parar el raton
    QGeoCoordinate m_posRaton;
    QString dirPaqueteMapa() const;
    void crearMapa();
    void pintarRutaEnMapa(bool visible);

    // Acuses del dron: el plan y los mandos se dan por recibidos solo cuando
    // el dron contesta; si no contesta a tiempo se avisa, y si rechaza el plan
    // se reenvia como mucho kMaxReintentosPlan veces.
    void enviarMandos();
    void crearEsperasDeAcuse();
    QTimer *m_esperaAcusePlan = nullptr;
    QTimer *m_esperaAcuseMandos = nullptr;
    int m_reintentosPlan = 0;
    static constexpr int kMaxReintentosPlan = 3;
    static constexpr int kEsperaAcuseMs = 3000;
    void limpiarVueloEnMapa();

    qfi_EADI *indicador;
    QTabWidget *tabOpciones;

    Widget_Drones *m_wDrones;

    Widget_Estado_Cuadroptero *m_wEstadoCuadroptero;
    Widget_ControlObjetivo *m_wControlObjetivo = nullptr;
    Widget_Mandos *m_wMandos;
    Widget_Mandos_Cuadroptero *m_wMandosCuadroptero;
    Widget_Conexion *m_wConexion;
    Widget_Graficas *m_wGraficas = nullptr;
    Widget_Puntos *m_wPuntos;
    Widget_Ganancias_Control *m_wGanancias;

    bool m_estadoDronAdoptado = false;   // la pestaña de Mandos ya tomo el regimen del dron
    int m_posicionValida = -1;   // ultima validez avisada en el log (-1: ninguna)
    GrabadorVuelo *m_grabador = nullptr;   // graba el vuelo (casilla "Guardar")
    QList<QPointF> m_planEnDron;           // ultimo plan confirmado por el dron (norte, este)
    // Reproduccion: plan del vuelo grabado en ese momento y sus cumplidos.
    QList<QPointF> m_planReproducido;
    SeguimientoCumplidos m_cumplidosReproducidos;
    int m_puntoReproducido = -1;

    bool m_ControlObjetivo = false;
    bool m_Conectado = false;
    int TimerId;
    QTime start_time_;
    int vueloID;
    int dronID;
    int tipoDrone = 0;
    QString DronID = "";

    //dataBase

    QGeoCoordinate geoPosOld;
    QGeoCoordinate geoPos;

//    bool bWidgetRuta = false;
    bool primeraCoordenada = false;

    QList<E_Punto> m_ListPuntos{};
    QList<QPointF> listPuntoF{};
    SeguimientoCumplidos m_cumplidos;      // que puntos del plan ha cumplido el dron
    void marcarCumplidos(const QPointF &cumplir, const QGeoCoordinate &posDron);
    int m_puntoActual = -1;                // punto del plan al que va el dron

    // Panel de vuelo (sobre el mapa) y estado del enlace (barra de estado).
    PanelVuelo *m_panelVuelo = nullptr;
    QAction *aDetalleTecnico = nullptr;
    // Lo que dice la telemetria se pinta en VistaTelemetria; aqui solo lo que
    // no es pintar (validar, casa, cumplidos, grabar).
    VistaTelemetria *m_vista = nullptr;
    void procesarTelemetria(const MuestraVuelo &m, int tipoDron, bool rumbosComoLibreriaAntigua);
    MonitorEnlace m_monitor;
    QElapsedTimer m_reloj;
    QLabel *m_lEnlace = nullptr;
    QLabel *m_lTiempoConexion = nullptr;   //!< "Conectado 00:12:34" en la barra de estado
    QTimer *m_timerEnlace = nullptr;
    MonitorEnlace::Estado m_estadoEnlace = MonitorEnlace::Estado::Desconectado;
    qint64 m_ultimoPitido = 0;
    void crearIndicadorEnlace();
    void refrescarIndicadorEnlace();

    // QObject interface
    void refrescarControlObjetivo();
    QVector<PuntoPlan> planConfirmado() const;

    // Puntos de ruta a enviar
    sPUNTOXYZ puntosEnviar[Tramas::kPuntosPlan];

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
    // Panel de rutas (Widget_Puntos) flotando abajo, encima del mapa.
    PanelFlotante *m_flotanteRuta = nullptr;
    void crearPanelRutas();
    void colocarPaneles();
    // Botones de enviar fijos debajo de su pestaña, por el widget del que salieron.
    QHash<QObject *, QPushButton *> m_botonesFijos;


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
