// ============================================================================
// Widget_Puntos.h
// ============================================================================

#ifndef WIDGET_PUNTOS_H
#define WIDGET_PUNTOS_H

#include "dronedialog.h"
#include "AlturaWorker.h"
#include "Estructuras/E_Punto.h"
#include "PerfilAltitudWidget.h"

#include <QGeoCoordinate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QtWidgets>
#include <cmath>
#include <QtMath>
#include <QAbstractTableModel>
#include <QList>
#include <QTimer>
#include <QPair>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QClipboard>
#include <QApplication>
#include <QProgressDialog>
#include <QSet>

#include "Planificacion/ModeloRuta.h"

// ============================================================================
// CLASE Widget_Puntos – INTERFAZ GRÁFICA PRINCIPAL (VERSIÓN COMPACTA, SIN .ui)
// ============================================================================

/**
 * @brief Widget principal para la planificación y gestión de rutas.
 *
 * Contiene la tabla de puntos, métricas, botones de acción y se comunica con el modelo
 * y el mapa. Permite crear, cargar, editar, validar y enviar rutas de 10 puntos.
 */
class VentanaInforme;

class Widget_Puntos : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param alturaWorker Worker de alturas (para obtener elevación del terreno).
     * @param parent Widget padre.
     */
    explicit Widget_Puntos(AlturaWorker *alturaWorker, QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~Widget_Puntos();

    /**
     * @brief Obtiene el puntero al modelo de datos.
     * @return MiModelo*.
     */
    MiModelo *getModelo() const;

    /**
     * @brief Obtiene el combo box de selección de rutas.
     * @return QComboBox*.
     */
    QComboBox *getBoxNombreRuta() const;

    // Control de visibilidad de la ruta en el mapa
    /**
     * @brief Indica si la ruta está actualmente visible en el mapa.
     * @return true si visible.
     */
    bool isRouteVisible() const;

    /**
     * @brief Dice si hay enlace con el dron; sin enlace, "Enviar" avisa en vez de enviar.
     * @param disponible true mientras la estación está conectada.
     */
    void setEnlaceDisponible(bool disponible);

    /**
     * @brief Establece la visibilidad de la ruta en el mapa.
     * @param visible true para mostrar, false para ocultar.
     */
    void setRouteVisible(bool visible);

    /**
     * @brief Establece la ruta actual en el combo box (por nombre real).
     * @param routeName Nombre real de la ruta.
     * @param blockSignals Si true, bloquea las señales del combo durante la operación.
     */
    void setCurrentRouteInCombo(const QString &routeName, bool blockSignals = true);

    // Actualización de interfaz
    /**
     * @brief Actualiza el combo box de rutas consultando las tablas existentes en la base de datos.
     */
    void actualizarComboBoxRutas();

    /**
     * @brief Actualiza las etiquetas de métricas y el estado de la ruta en la interfaz.
     */
    void mostrarMetricasRuta();

    /**
     * @brief Abre la ventana del análisis de la ruta (no modal, se rehace sola).
     */
    void mostrarAnalisisDetallado();

signals:
    // Señales para comunicación con otros módulos (principalmente CControladora)
    void sI_pintaRuta(bool visible);                            ///< Solicita pintar/ocultar la ruta en el mapa.
    void sI_Ruta(QList<QPair<double, double>>);                 ///< Emite la ruta como lista de pares (lat, lon).
    void sI_Ruta2(QList<E_Punto>);                              ///< Emite la ruta como lista de objetos E_Punto.
    void sI_RutaValidada(bool valida, const QString &mensaje); ///< Emite resultado de validación.
    void sI_DronConfigurado(const QString &tipoDron, const QString &nombreRuta); ///< Dron configurado.
    void sI_iniciarSeleccionPuntoInicial();                    ///< Inicia modo selección de punto inicial.
    void sI_cancelarSeleccionPuntoInicial();                   ///< Cancela modo selección.
    void sI_rutasModificadas();                                 ///< Lista de rutas modificada.
    void sI_rutaSeleccionada(const QString &routeName);        ///< Ruta seleccionada.
    void sI_visibilidadCambiada(const QString &routeName, bool visible); ///< Cambió visibilidad de una ruta.
    void sI_centrarEnPunto(const QGeoCoordinate &pos);          ///< Doble clic en un punto: centrar el mapa ahí.
    void sI_encuadrarRuta();                                    ///< Se pidió ver la ruta: encuadrarla en el mapa.

public slots:
    /**
     * @brief Agrega un nuevo punto a la ruta (llamado desde el mapa o desde otros módulos).
     * @param punto Punto a agregar.
     */
    void sL_agregarNuevoPunto(const E_Punto &punto);

    /**
     * @brief Actualiza puntos existentes en la ruta (llamado desde el mapa).
     * @param puntos Lista de puntos modificados.
     */
    void sL_actualizaPunto(const QList<E_Punto> &);

    /**
     * @brief Selecciona la ruta cuyo nombre real está en el combo (usado internamente).
     * @param nombreRuta No usado;
     */
    void seleccionarRuta(const QString &);

    /**
     * @brief Muestra un acuse de recibo visual en el botón de enviar.
     */
    void on_AcuseRecibo();

    /**
     * @brief Restaura el estilo original del botón de enviar.
     */
    void on_restoreStyle();

    /**
     * @brief Slot llamado cuando el modelo emite rutaConfigurada.
     * @param tipoDron Tipo de dron configurado.
     * @param nombreRuta Nombre de la ruta configurada.
     */
    void onRutaConfigurada(const QString &tipoDron, const QString &nombreRuta);

    /**
     * @brief Maneja el movimiento de un punto en el mapa, actualizando coordenadas y altitud.
     * @param index Índice del punto movido.
     * @param nuevaPos Posición donde lo ha soltado el operador.
     */
    void onPointMoved(int index, const QGeoCoordinate &nuevaPos);

    /**
     * @brief Inicia el modo de selección del punto inicial (P1) haciendo clic derecho en el mapa.
     */
    void iniciarSeleccionPuntoInicial();

    /**
     * @brief Cancela el modo de selección de punto inicial.
     */
    void cancelarSeleccionPuntoInicial();

    /**
     * @brief Agrega el punto inicial seleccionado y genera los 9 puntos restantes automáticamente.
     * @param coordenada Coordenada del punto P1.
     */
    void agregarPuntoInicialYGenerarResto(const QGeoCoordinate &coordenada);

    /**
     * @brief Carga una ruta desde la base de datos y actualiza la interfaz.
     * @param routeName Nombre de la ruta a cargar.
     */
    void cargarRuta(const QString &routeName);

private slots:
    // Slots de los botones de la interfaz
    void on_pb_guardar_clicked();
    void on_pB_CrearRuta_clicked();
    void on_pB_EliminaRuta_clicked();
    void on_pB_LimpiaRuta_clicked();
    void on_pB_EnviaRuta_clicked();
    void on_pb_Mostrar_toggled(bool checked);
    void on_pB_AnalisisDetallado_clicked();
    void mostrarPerfil();
    void mostrarGuia();

private:
    // Miembros de datos
    QSqlDatabase *mdb;                         ///< Conexión a la base de datos.
    MiModelo *modelo;                           ///< Modelo de datos de la tabla.
    QTimer m_timer;                             ///< Temporizador para el acuse de recibo.
    bool m_enlaceDisponible = false;            ///< Hay enlace con el dron (lo pone CControladora).
    bool m_rutaEnviable = false;                ///< La ruta tiene sus 10 puntos y se puede mandar.
    QString m_originalStyle;                     ///< Estilo original del botón enviar.
    bool m_modoSeleccionPuntoInicial;            ///< Indica si estamos en modo selección de P1.
    AlturaWorker *m_alturaWorker;                ///< Worker de alturas.
    PerfilAltitudWidget *m_perfilWidget;         ///< Widget de perfil de altitud.
    QPushButton *m_btnPerfil;                    ///< Botón "Perfil".

    // Widgets de la interfaz
    QComboBox *cB_Rutas;                         ///< Combo de selección de rutas.
    QTableView *tw_puntosRuta;                   ///< Tabla de puntos.
    QLabel *label_Distancia;                     ///< Etiqueta de distancia total.
    QLabel *label_AutonomiaRest;                  ///< Etiqueta de autonomía restante (%).
    QLabel *label_Tiempo;                         ///< Etiqueta de tiempo estimado.
    QProgressBar *progressAutonomia;              ///< Barra de progreso de autonomía.
    QLabel *label_Estado;                         ///< Etiqueta de estado de la ruta.
    QLabel *labelInfoDron;                        ///< Etiqueta de información del dron.
    QLabel *label_PuntosFijos;                    ///< Etiqueta informativa de puntos fijos.

    QPushButton *pB_CrearRuta;                    ///< Botón "Nueva".
    QPushButton *pB_EliminaRuta;                  ///< Botón "Eliminar".
    QPushButton *pB_LimpiaRuta;                   ///< Botón "Restablecer".
    QPushButton *pb_guardar;                      ///< Botón "Guardar".
    QPushButton *pB_EnviaRuta;                    ///< Botón "Enviar".
    QPushButton *pb_Mostrar;                      ///< Botón "Mostrar/Ocultar" (toggle).
    QPushButton *pB_AnalisisDetallado;            ///< Botón "Análisis".
    QPushButton *pB_CancelarSeleccion;            ///< Botón "Cancelar" (modo selección P1).

    // Botones ocultos (por compatibilidad con código existente)
    QPushButton *pB_EliminaPunto;
    QPushButton *pB_Cargar;
    QPushButton *pB_ExportarAnalisis;

    // Métodos auxiliares
    void generarRutaDesde(const QGeoCoordinate &p1);
    QSpinBox *m_spinSobreTerreno = nullptr;   // altura sobre el terreno de las rutas nuevas
    void updateUIForDroneConfigured(bool configured);
    void mostrarInfoDronActual();
    void limpiarTodo();
    void habilitarBotonesRuta(bool habilitar);
    void refrescarBotonEnviar();
    void refrescarPerfil();
    void ponerEstado(const QString &texto, const QColor &color, const QString &detalle);
    void ajustarAnchoTabla();
    QString obtenerNombreTablaLimpio(const QString &nombreTablaConFormato);
    void refrescarInforme(bool aunqueOculta = false);
    VentanaInforme *m_informe = nullptr;   // ventana del analisis (se crea al abrirla)

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

};

#endif // WIDGET_PUNTOS_H
