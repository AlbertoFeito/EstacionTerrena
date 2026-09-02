#include "myqbuttongroup.h"
MyQButtonGroup::MyQButtonGroup(QObject *parent)
    : QButtonGroup (parent)
{
    _bExclusive = true;
    QButtonGroup::setExclusive (false);
    connect(this,SIGNAL(buttonClicked(QAbstractButton *)),SLOT (buttonClicked(QAbstractButton *)));
}

bool MyQButtonGroup::bExclusive() const
{
    return _bExclusive;
}

void MyQButtonGroup::setBExclusive(bool bExclusive)
{
    _bExclusive = bExclusive;
}

void MyQButtonGroup::buttonClicked(QAbstractButton *button)
{
    if(_bExclusive)
    {
        QList<QAbstractButton *> buttonList = buttons ();
        for(auto iBtn = buttonList.begin (); iBtn != buttonList.end (); ++iBtn)
        {
            QAbstractButton *pBtn = *iBtn;
            if(pBtn && pBtn != button && pBtn->isCheckable ())
            {
                pBtn->setChecked (false);
            }
        }
    }
}
