#ifndef BADGELABEL_H
#define BADGELABEL_H

#include <QLabel>
#include <QString>
#include <QWidget>

// kind: "green" | "cyan" | "amber" | "red" | "muted" — consumed by
// QLabel[badgeKind="..."] rules in aurix_dark.qss.
inline QLabel *makeBadgeLabel(const QString &text, const QString &kind, QWidget *parent = nullptr)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("badgeKind", kind);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

#endif // BADGELABEL_H
