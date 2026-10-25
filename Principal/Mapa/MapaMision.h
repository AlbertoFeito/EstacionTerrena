#ifndef MAPAMISION_H
#define MAPAMISION_H

#include <QColor>
#include <QGeoCoordinate>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class QWidget;
namespace libmapa { class MapWidget; }

//! Una capa del paquete para el panel de capas: id, nombre y si se ve.
struct CapaMapa
{
    QString id;
    QString nombre;
    bool visible = false;
};

//! Un punto de la ruta tal como se dibuja: donde esta y su rotulo ("P1"...).
struct PuntoRutaMapa
{
    QGeoCoordinate pos;
    QString nombre;
};

/*!
 * \brief El mapa de la estacion: lo que la estacion entiende (dron, casa,
 *        ruta, rumbos) dibujado sobre libmapa::MapWidget.
 *
 * Es el unico sitio que habla con libmapa. CControladora y los widgets piden
 * "pon el dron aqui" o "pinta esta ruta" y no saben de objetivos, capas ni
 * marcaciones; si libmapa cambia, solo se toca esta clase.
 *
 * No guarda nada en la base de entidades del paquete (autosaveFeatures =
 * false, como los ejemplos que dibujan): la ruta vive en la base de rutas de
 * Widget_Puntos y el vuelo se rehace con la telemetria.
 */
class MapaMision : public QObject
{
    Q_OBJECT
public:
    enum class CapaBase { Satelital, OSM };

    //! Crea el mapa sobre el paquete de datos \a paquete (su `mapa.json` o la
    //! carpeta que lo tiene), como en los ejemplos de libmapa. El mapa hibrido
    //! y las capas fijas del paquete arrancan ocultos (se encienden desde el
    //! panel de capas). \a padre es el widget donde se incrusta.
    MapaMision(const QString &paquete, QWidget *padre);

    //! El widget del mapa, para meterlo en un layout o colgarle overlays.
    libmapa::MapWidget *mapa() const;

    //! true si el paquete abrio y hay al menos una capa base.
    bool listo() const;
    //! Motivo si no esta listo.
    QString error() const;
    //! Avisos del paquete y de la elevacion (ficheros que faltan...).
    QStringList avisos() const;

    //! Cambia la capa base. false si el paquete no la trae.
    bool ponerCapaBase(CapaBase capa);
    CapaBase capaBase() const;

    // --- Capas del paquete (solo lo que trae su mapa.json) ---------------
    //! Capas base del paquete; \c visible marca la activa.
    QVector<CapaMapa> capasBase() const;
    QString capaBaseId() const;
    //! Pone la capa base \a id. false si el paquete no la trae.
    bool ponerCapaBaseId(const QString &id);
    //! Grupos de una capa base vectorial (nombres de calles, edificios...);
    //! vacio si es de teselas raster.
    QVector<CapaMapa> gruposCapaBase(const QString &base) const;
    bool ponerGrupoCapaBaseVisible(const QString &base, const QString &grupo, bool visible);
    //! Mapa hibrido ("vectorOverlay" del paquete): calles y nombres encima de
    //! cualquier base. Arranca oculto.
    bool hayHibrido() const;
    QString nombreHibrido() const;
    bool hibridoVisible() const;
    void ponerHibridoVisible(bool visible);
    QVector<CapaMapa> gruposHibrido() const;
    bool ponerGrupoHibridoVisible(const QString &grupo, bool visible);
    //! Capas fijas del paquete ("overlays": aguas, corredores...), sin las de
    //! la estacion (ruta, mision, dibujos). Arrancan ocultas.
    QVector<CapaMapa> capasFijas() const;
    bool ponerCapaFijaVisible(const QString &id, bool visible);
    //! Como estan las capas, para guardarlo en conf.ini: "base", "hibrido",
    //! "hibrido_ocultos", "fijas" (las visibles) y "ocultos_<base>" (grupos
    //! apagados de cada base vectorial).
    QVariantMap estadoCapas() const;
    //! Lo contrario: aplica lo guardado; lo que el paquete ya no trae se ignora.
    void restaurarCapas(const QVariantMap &estado);

    // --- Vuelo -----------------------------------------------------------
    //! Coloca (o crea) el dron y alarga su traza. \a simbolo se gira con el
    //! rumbo; \a rotulo va junto al simbolo ("V: ... km/h\nH: ... m").
    void actualizarVehiculo(const QGeoCoordinate &pos, double rumboDeg,
                            const QColor &colorTraza, const QPixmap &simbolo,
                            const QString &rotulo);
    /*!
     * \brief Las tres lineas de rumbo que salen del dron.
     *
     * - azul: \a rumboVehiculoDeg (solo en automatico),
     * - morada: hacia \a proximoPunto, con su rumbo y distancia (solo en automatico),
     * - verde: \a rumboDeseadoDeg (siempre).
     *
     * Las de rumbo llegan hasta el borde de la vista, como en la libreria
     * antigua. Si aun no hay dron no hace nada.
     */
    void ponerRumbos(double rumboVehiculoDeg, double rumboDeseadoDeg,
                     const QGeoCoordinate &proximoPunto, bool automatico);
    //! Marca la casa (circulo verde con una "H").
    void ponerHome(const QGeoCoordinate &pos);
    //! Quita el dron, su traza, sus rumbos y la casa (al desconectar).
    void limpiarVuelo();

    // --- Ruta ------------------------------------------------------------
    //! Dibuja la ruta (linea, puntos arrastrables y circulos de acercamiento).
    //! Con \a visible = false la quita del mapa.
    void pintarRuta(const QVector<PuntoRutaMapa> &puntos, bool visible);
    //! Ajusta la vista para que quepa la ruta entera en la parte que se ve.
    void encuadrarRuta();
    //! Pinta de verde el punto \a indice, ya cumplido por el dron.
    void marcarPuntoCumplido(int indice);
    //! Radio del circulo de acercamiento de cada punto, en metros Mercator
    //! (las mismas unidades que el radio que se manda al autopiloto).
    void ponerRadioAcercamiento(double metrosMercator);
    double radioAcercamiento() const;

    // --- Herramientas ----------------------------------------------------
    void medirDistancia(bool activa);
    void zoomArea(bool activa);
    void zoomMas();
    void zoomMenos();
    void dibujarPoligono();

    //! Lleva \a pos al centro de la parte que se ve, sin cambiar el zoom.
    void centrarEn(const QGeoCoordinate &pos);
    //! Lo que tapan los paneles flotantes (rutas abajo, capas a la derecha), en
    //! pixeles: encuadrar y centrar usan solo la parte del mapa que se ve.
    void ponerZonaTapada(int abajoPx, int derechaPx);

    //! Cota del terreno en metros, o NaN si no hay dato.
    double alturaTerreno(const QGeoCoordinate &pos) const;

signals:
    //! El operador ha arrastrado el punto \a indice de la ruta a \a pos.
    void puntoRutaMovido(int indice, const QGeoCoordinate &pos);
    //! Clic derecho en el mapa (fuera de una herramienta).
    void clicDerecho(const QGeoCoordinate &pos);
    void posicionRaton(const QGeoCoordinate &pos);
    void zoomAreaTerminado();
    void medidaTerminada(double rumboDeg, double distanciaM);
    //! Ha cambiado la capa base, el hibrido o alguna capa fija (para que el
    //! panel y el menu de capas se pongan al dia).
    void capasCambiadas();

private:
    void ocultarCapasDelPaquete();
    static bool esCapaDeLaEstacion(const QString &id);
    void crearCapas();
    void refrescarRumbos();
    void refrescarAnillos();
    void refrescarRotulosRuta();
    double metrosEnPantalla(double px) const;
    static QPixmap simboloHome();
    static QString idCapa(CapaBase capa);

    libmapa::MapWidget *m_mapa = nullptr;
    int m_tapadoAbajo = 0;     // pixeles que tapan los paneles flotantes
    int m_tapadoDerecha = 0;

    qint64 m_vehiculo = -1;
    QPixmap m_simbolo;
    qint64 m_home = -1;

    // Rumbos: ids de las tres marcaciones y lo ultimo pedido, para rehacerlas
    // al cambiar de zoom (el largo de los vectores va en pixeles).
    qint64 m_lineaVehiculo = -1;
    qint64 m_lineaProximo = -1;
    qint64 m_lineaDeseado = -1;
    bool m_hayRumbos = false;
    double m_rumboVehiculo = 0.0;
    double m_rumboDeseado = 0.0;
    QGeoCoordinate m_proximo;
    bool m_automatico = false;

    // Ruta: la linea, un punto por vertice y su anillo de acercamiento.
    qint64 m_linea = -1;
    QVector<qint64> m_puntos;
    QVector<qint64> m_anillos;
    QVector<bool> m_cumplidos;
    double m_radio = 20.0;   // el de la libreria antigua hasta que llegan los mandos
};

#endif // MAPAMISION_H
