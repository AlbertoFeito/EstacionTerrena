#ifndef MYQSETTING_H
#define MYQSETTING_H

#include <QObject>
#include <QVariant>
class MyQSetting : public QObject
{
    Q_OBJECT
public:
    explicit MyQSetting(QObject *parent = nullptr);

    void guardarSetting(const QString &fileName, const QString &llave, const QVariant &valor, const QString &grupo);
    QVariant cargarSetting(const QString &fileName, const QString &llave, const QString &grupo, const QVariant &valDefecto = QVariant ());
signals:

};

#endif // MYQSETTING_H
