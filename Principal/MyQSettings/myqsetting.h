#ifndef MYQSETTING_H
#define MYQSETTING_H

#include <QObject>
#include <QVariant>
#include <QVariantMap>
class MyQSetting : public QObject
{
    Q_OBJECT
public:
    explicit MyQSetting(QObject *parent = nullptr);

    void guardarSetting(const QString &fileName, const QString &llave, const QVariant &valor, const QString &grupo);
    //! Guarda varios valores de un grupo de una vez y en otro hilo (no espera
    //! al disco); si no ha cambiado nada desde la ultima vez, ni escribe.
    void guardarSettings(const QString &fileName, const QString &grupo, const QVariantMap &valores);
    QVariant cargarSetting(const QString &fileName, const QString &llave, const QString &grupo, const QVariant &valDefecto = QVariant ());
signals:

};

#endif // MYQSETTING_H
