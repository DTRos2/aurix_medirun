#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "../app/appsession.h"

#include <QDialog>

class AuthService;
class QFrame;
class QLineEdit;
class QLabel;
class QPushButton;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(AuthService &authService, bool mockMode, QWidget *parent = nullptr);

    AppSession session() const;

private slots:
    void attemptLogin();
    void fillStaffCredentials();
    void fillAdminCredentials();
    void togglePasswordVisibility();

private:
    void buildUi(bool mockMode);
    UserRole selectedRole() const;

    AuthService &m_authService;
    AppSession m_session;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLabel *m_statusLabel;
    QPushButton *m_togglePasswordButton;
    QFrame *m_loginCard;
};

#endif // LOGINDIALOG_H
