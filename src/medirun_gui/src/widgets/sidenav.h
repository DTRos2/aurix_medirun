#ifndef SIDENAV_H
#define SIDENAV_H

#include "../app/appsession.h"
#include "pageid.h"

#include <QFrame>
#include <QHash>
#include <QList>

class QPushButton;

class SideNav : public QFrame
{
    Q_OBJECT

public:
    explicit SideNav(const AppSession &session, QWidget *parent = nullptr);

    void setActivePage(PageId page);

signals:
    void pageSelected(PageId page);
    void logoutRequested();
    void exitRequested();

private:
    QWidget *buildAccountFooter(const AppSession &session);

    QHash<PageId, QPushButton *> m_buttons;
};

#endif // SIDENAV_H
