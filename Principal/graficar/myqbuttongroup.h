#ifndef MYQBUTTONGROUP_H
#define MYQBUTTONGROUP_H

#include <QButtonGroup>
#include <QAbstractButton>



class MyQButtonGroup : public QButtonGroup
{
    Q_OBJECT
public:
    explicit MyQButtonGroup(QObject *parent = nullptr);

    bool bExclusive() const;
    void setBExclusive(bool bExclusive);
protected slots:
    void buttonClicked(QAbstractButton *button);
protected:
    bool _bExclusive;
};

#endif // MYQBUTTONGROUP_H
