#ifndef QCHECKBOXCOMBO_H
#define QCHECKBOXCOMBO_H

#include <QComboBox>
#include <QListView>
#include <QtWidgets>

/*
 * QCheckboxCombo is a combobox widget that contains items with checkboxes
 * User can select proper items by checking corresponding checkboxes.
 * Resulting text will contain list of selected items separated by delimiter ("," by default)
 * Note: Multiple selection works with property editable = true.
 */
class QCheckboxCombo : public QComboBox
{
  Q_OBJECT
public:
  explicit QCheckboxCombo(QWidget *parent = 0);
  bool eventFilter(QObject * watched, QEvent * event);
  void hidePopup();
  void showPopup();
  void setDelimiter(QString str){ _delimiter = str; }
  QList<int> getCheckedItems() const;
  void setCheckedItems(const QList<int> &value);

private:
  QString _delimiter = ",";
  QString displayText;
   QList<int> checkedItems;
signals:
  //can be used to update items dynamically
  void beforeOpen();
  void afterOpen();
public slots:

  void slotUpdateText();

  // QWidget interface
protected:
  virtual void keyPressEvent(QKeyEvent *event);
};

#endif // QCHECKBOXCOMBO_H
