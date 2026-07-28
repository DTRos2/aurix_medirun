#ifndef CALLHISTORYPAGE_H
#define CALLHISTORYPAGE_H

#include <QWidget>

class CallHistoryPage : public QWidget
{
    Q_OBJECT

public:
    explicit CallHistoryPage(QWidget *parent = nullptr);

signals:
    void replayRequested();

private:
    QWidget *buildHeader();
    QWidget *buildTable();
};

#endif // CALLHISTORYPAGE_H
