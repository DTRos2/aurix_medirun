#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "../app/appsession.h"

#include <QDialog>

class AuthService;
class QFrame;
class QLineEdit;
class QLabel;
class QPushButton;
class QToolButton;

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi(bool mockMode);
    UserRole selectedRole() const;
    void setFormBusy(bool busy);
    void repositionPasswordVisibilityButton();

    static constexpr int kMaxLoginAttempts = 5;

    AuthService &m_authService;
    AppSession m_session;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLabel *m_statusLabel;
    QLabel *m_errorLabel;
    QToolButton *m_passwordVisibilityButton;
    QPushButton *m_loginButton;
    QFrame *m_loginCard;
    int m_failedAttempts;
};

#endif // LOGINDIALOG_H
