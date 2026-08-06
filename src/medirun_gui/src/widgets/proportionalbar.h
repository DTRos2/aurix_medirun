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

    // fill/remainder는 실제로 레이아웃에 addWidget()되는 쪽만 만든다. 둘 다
    // 미리 만들어두고 한쪽만 addWidget()하면, 레이아웃에 안 들어간 나머지
    // QFrame이 부모(track)의 관리되지 않는 자식으로 남아 (0,0) 위치에 작은
    // 크기로 계속 그려져 다른 쪽 색 위에 겹쳐 보이는 버그가 있었다.
    if (percent <= 0) {
        auto *remainder = new QFrame(track);
        remainder->setObjectName(QStringLiteral("trackBase"));
        layout->addWidget(remainder, 1);
    } else if (percent >= 100) {
        auto *fill = new QFrame(track);
        fill->setObjectName(QStringLiteral("trackFillCyan"));
        fill->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 2px;").arg(fillColor.name()));
        layout->addWidget(fill, 1);
    } else {
        auto *fill = new QFrame(track);
        fill->setObjectName(QStringLiteral("trackFillCyan"));
        fill->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 2px;").arg(fillColor.name()));
        layout->addWidget(fill, percent);

        auto *remainder = new QFrame(track);
        remainder->setObjectName(QStringLiteral("trackBase"));
        layout->addWidget(remainder, 100 - percent);
    }

    return track;
}

#endif // PROPORTIONALBAR_H
