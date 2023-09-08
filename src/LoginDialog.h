#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <Qpushbutton>
#include <QtWidgets/qwidget.h>

class LoginDialog : public QDialog {
  Q_OBJECT

public:
  explicit LoginDialog(QWidget *parent = nullptr);

private:
  void onLoginButtonClicked();
  QLineEdit *usernameEdit;
  QLineEdit *passwordEdit;
};
