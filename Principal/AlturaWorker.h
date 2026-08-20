#ifndef ALTURAWORKER_H
#define ALTURAWORKER_H

#include <QtCore/QObject>

#include <QDataStream>
#include <QFile>
#include <QDebug>
#include <cmath>
#include <QDir>
#include <utiles.h>

class AlturaWorker : public QObject
{
    Q_OBJECT
public:
    AlturaWorker(QObject *parent = nullptr);
    ~AlturaWorker();
public slots:
    qint16 obtenerAltura(double lat, double lon);

signals:
    void alturaActualizada(qint16 altura);

private:
    quint16 obtenerAlturaDesdeHGT(double lat, double lon);

    QString m_hgtFileName = "";
    int obtenerAlturaDesdeHGT2(double lat, double lon);
};

#endif // ALTURAWORKER_H
