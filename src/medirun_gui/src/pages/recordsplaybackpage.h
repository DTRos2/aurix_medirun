#ifndef RECORDSPLAYBACKPAGE_H
#define RECORDSPLAYBACKPAGE_H

#include <QWidget>

class RecordsPlaybackPage : public QWidget
{
    Q_OBJECT

public:
    explicit RecordsPlaybackPage(QWidget *parent = nullptr);

private:
    QWidget *buildHeader();
    QWidget *buildSidePanel();
    QWidget *buildTimeline();
};

#endif // RECORDSPLAYBACKPAGE_H
