#include "LoginDialog.h"
#include <QApplication>

LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent) {
  usernameEdit = new QLineEdit;
  passwordEdit = new QLineEdit;

  QPushButton *loginButton = new QPushButton("OK");
  connect(loginButton, &QPushButton::clicked, this,
          &LoginDialog::onLoginButtonClicked);
}

void LoginDialog::onLoginButtonClicked() {
  QString username = usernameEdit->text();
  QString password = passwordEdit->text();

  if (username == "admin" && password == "admin") {
    accept();
  } else {
    QMessageBox::warning(this, "Warning", "Wrong username or password!",
                         QMessageBox::Ok);
  }
}
