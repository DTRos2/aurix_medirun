#ifndef PROPORTIONALBAR_H
#define PROPORTIONALBAR_H

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QWidget>

// Thin horizontal bar whose fill/remainder widths are driven by layout
// stretch factors (percent : 100-percent) rather than pixel math, so it
// stays correct across resizes without a resizeEvent override.
inline QWidget *makeProportionalBar(int percent, const QColor &fillColor, QWidget *parent = nullptr)
{
    percent = qBound(0, percent, 100);

    auto *track = new QWidget(parent);
    track->setFixedHeight(4);
    auto *layout = new QHBoxLayout(track);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *fill = new QFrame(track);
    fill->setObjectName(QStringLiteral("trackFillCyan"));
    fill->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 2px;").arg(fillColor.name()));

    auto *remainder = new QFrame(track);
    remainder->setObjectName(QStringLiteral("trackBase"));

    if (percent <= 0) {
        layout->addWidget(remainder, 1);
    } else if (percent >= 100) {
        layout->addWidget(fill, 1);
    } else {
        layout->addWidget(fill, percent);
        layout->addWidget(remainder, 100 - percent);
    }

    return track;
}

#endif // PROPORTIONALBAR_H
