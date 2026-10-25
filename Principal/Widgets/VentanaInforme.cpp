#include "VentanaInforme.h"

#include "Planificacion/InformeRuta.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

// La ventana con el informe y los botones Exportar y Cerrar. Los enlaces
// "punto:N" no se siguen: avisan con puntoElegido(N).
VentanaInforme::VentanaInforme(QWidget *padre)
    : QDialog(padre)
{
    setWindowTitle(tr("Análisis de la ruta"));
    setModal(false);
    resize(640, 720);

    m_texto = new QTextBrowser(this);
    m_texto->setOpenLinks(false);
    m_texto->setOpenExternalLinks(false);
    connect(m_texto, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (url.scheme() == QLatin1String("punto"))
            emit puntoElegido(url.path().toInt());
    });

    auto *botones = new QDialogButtonBox(this);
    QPushButton *exportar = botones->addButton(tr("Exportar…"), QDialogButtonBox::ActionRole);
    exportar->setToolTip(tr("Guarda este informe en un fichero de texto"));
    connect(exportar, &QPushButton::clicked, this, &VentanaInforme::exportar);
    QPushButton *cerrar = botones->addButton(tr("Cerrar"), QDialogButtonBox::RejectRole);
    connect(cerrar, &QPushButton::clicked, this, &QDialog::close);

    auto *capa = new QVBoxLayout(this);
    capa->addWidget(m_texto);
    capa->addWidget(botones);
}

// Rehace el informe y deja la barra donde estaba (se corrige mirandolo).
void VentanaInforme::mostrar(const RevisionRuta &r, const DroneCharacteristics &dron, const QString &ruta,
                             const QList<E_Punto> &puntos)
{
    m_ruta = ruta;
    m_plano = InformeRuta::texto(r, dron, ruta, puntos);
    const int barra = m_texto->verticalScrollBar()->value();
    m_texto->setHtml(InformeRuta::html(r, dron, ruta, puntos));
    m_texto->verticalScrollBar()->setValue(barra);
}

// Guarda el informe en texto donde diga el operador.
void VentanaInforme::exportar()
{
    const QString nombre = QFileDialog::getSaveFileName(
        this, tr("Guardar el análisis"), QDir::current().filePath(m_ruta + QStringLiteral("_analisis.txt")),
        tr("Texto (*.txt)"));
    if (nombre.isEmpty())
        return;
    QFile f(nombre);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Guardar el análisis"), tr("No se pudo escribir %1.").arg(nombre));
        return;
    }
    QTextStream(&f) << m_plano;
}
