#ifndef USERMANAGEMENTPAGE_H
#define USERMANAGEMENTPAGE_H

#include <QWidget>

class AuthService;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

class UserManagementPage : public QWidget
{
    Q_OBJECT

public:
    explicit UserManagementPage(AuthService &authService, QWidget *parent = nullptr);

private:
    QWidget *buildCreateForm();
    void refreshUserTable();
    void handleCreateUser();
    void handleDeleteUser(const QString &loginId);

    AuthService &m_authService;
    QLineEdit *m_emailEdit;
    QLineEdit *m_passwordEdit;
    QComboBox *m_roleCombo;
    QLabel *m_statusLabel;
    QTableWidget *m_userTable;
};

#endif // USERMANAGEMENTPAGE_H
