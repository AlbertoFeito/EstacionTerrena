#include "qcheckboxcombo.h"
#include <QMouseEvent>
#include <QDebug>

QCheckboxCombo::QCheckboxCombo(QWidget *parent) :
  QComboBox(parent)
{
  // set event filter item view
  view()->viewport()->installEventFilter(this);
}

// Event filter prevents combobox from collapsing by mouse click
// and changes "checked" property of items
bool QCheckboxCombo::eventFilter(QObject * watched, QEvent * event)
{
  // filtering only "MouseButtonRelease" event
  if (event->type() == QEvent::MouseButtonRelease)
  {

    // find index of selected item
    QModelIndex ind = view()->indexAt((static_cast<QMouseEvent*>(event))->pos());
    // get current item status
    bool checked = view()->model()->data(ind,Qt::CheckStateRole).toBool();
    // invert item status
    view()->model()->setData(ind,!checked,Qt::CheckStateRole);
    // block spreading of event
    return true;
  } 
  return QObject::eventFilter(watched, event);
}

void QCheckboxCombo::hidePopup()
{

  QStringList values;
  //Building resulting text as a list of checked items
  for (int i=0; i < count(); i++){
    if (itemData(i, Qt::CheckStateRole).toBool()){
      values << itemText(i);
    }
  }
  setCurrentText(values.join(_delimiter));
  displayText = values.join(_delimiter);
//  lineEdit ()->setText(values.join(_delimiter));
//  slotUpdateText ();
  emit afterOpen();
  QComboBox::hidePopup();

}

void QCheckboxCombo::showPopup()
{
  //Setting "checked" property of items that contained in resulting text
  QStringList values = currentText().split(_delimiter);

  emit beforeOpen();

  for (int i=0; i<count(); i++){
    setItemData(i, values.contains(itemText(i)), Qt::CheckStateRole);
  }
  QComboBox::showPopup();
}

void QCheckboxCombo::slotUpdateText()
{
    lineEdit()->setText(displayText);
}

void QCheckboxCombo::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED (event);
}

QList<int> QCheckboxCombo::getCheckedItems() const
{
    QList<int> checkedItems;

    for (int i=0; i < count(); i++){
      if (itemData(i, Qt::CheckStateRole).toBool()){
        checkedItems << i+1;
      }
    }

    return checkedItems;
}
