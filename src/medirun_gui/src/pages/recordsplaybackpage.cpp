#include "recordsplaybackpage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/mapviewwidget.h"
#include "../widgets/proportionalbar.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QLabel *makeLabel(const QString &text, const QString &objectName = {})
{
    auto *label = new QLabel(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    label->setWordWrap(true);
    return label;
}

QFrame *panelFrame(const QString &objectName)
{
    auto *frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

class SparklineWidget : public QWidget
{
public:
    explicit SparklineWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(20);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const QVector<qreal> samples = {0.7, 0.4, 0.2, 0.55, 0.05, 0.35, 0.75};
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        const qreal stepX = width() / qreal(samples.size() - 1);
        for (int i = 0; i < samples.size(); ++i) {
            const QPointF point(i * stepX, samples.at(i) * height());
            if (i == 0) {
                path.moveTo(point);
            } else {
                path.lineTo(point);
            }
        }
        painter.setPen(QPen(DashboardPalette::Cyan, 1.5));
        painter.drawPath(path);
    }
};

QLabel *legendDot(const QString &text, const QColor &color)
{
    auto *label = new QLabel(QStringLiteral("● %1").arg(text));
    label->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(color.name()));
    return label;
}
} // namespace

RecordsPlaybackPage::RecordsPlaybackPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildHeader());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(9);

    auto *mapView = new MapViewWidget(canvas);
    mapView->setMinimumHeight(165);
    mapView->setMode(MapViewMode::Playback);
    mapView->setCaption(QStringLiteral("시안=지나온 경로 · 회색=남은 경로"));
    mapView->setRobotPose(QPointF(0.39, 0.41), 0.0);
    mapView->setRobotState(RobotMarkerState::Moving);
    mapView->setPlaybackPath({QPointF(0.21, 0.76), QPointF(0.39, 0.76), QPointF(0.39, 0.41)},
                              {QPointF(0.39, 0.41), QPointF(0.71, 0.41)});
    mapView->setDestination(QPointF(0.71, 0.41));
    bodyRow->addWidget(mapView, 15);
    bodyRow->addWidget(buildSidePanel(), 10);

    layout->addLayout(bodyRow);
    layout->addWidget(buildTimeline());
}

QWidget *RecordsPlaybackPage::buildHeader()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(9);
    layout->addWidget(makeLabel(QStringLiteral("주행 기록")));
    layout->addWidget(makeBadgeLabel(QStringLiteral("재생 모드 · 로봇에 명령 전송 없음"), QStringLiteral("amber"), row));
    layout->addStretch();
    layout->addWidget(makeLabel(QStringLiteral("미션 #142 · 07-28 14:02 · 4:12"), QStringLiteral("metricCaption")));
    return row;
}

QWidget *RecordsPlaybackPage::buildSidePanel()
{
    auto *col = new QWidget;
    auto *layout = new QVBoxLayout(col);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    auto *cameraPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *cameraLayout = new QVBoxLayout(cameraPanel);
    auto *cameraLabel = makeLabel(QStringLiteral("🎥\n카메라 동기 재생"));
    cameraLabel->setAlignment(Qt::AlignCenter);
    cameraLabel->setStyleSheet(QStringLiteral("color: %1;").arg(DashboardPalette::TextTertiary.name()));
    cameraLayout->addWidget(cameraLabel);
    layout->addWidget(cameraPanel, 1);

    auto *speedPanel = panelFrame(QStringLiteral("contentPanel"));
    speedPanel->setFixedHeight(48);
    auto *speedLayout = new QVBoxLayout(speedPanel);
    speedLayout->setContentsMargins(9, 5, 9, 5);
    speedLayout->setSpacing(3);
    auto *speedRow = new QHBoxLayout;
    speedRow->addWidget(makeLabel(QStringLiteral("속도 m/s"), QStringLiteral("metricCaption")));
    speedRow->addStretch();
    auto *speedValue = makeLabel(QStringLiteral("0.18"));
    speedValue->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Cyan.name()));
    speedRow->addWidget(speedValue);
    speedLayout->addLayout(speedRow);
    speedLayout->addWidget(new SparklineWidget(speedPanel));
    layout->addWidget(speedPanel);
    return col;
}

QWidget *RecordsPlaybackPage::buildTimeline()
{
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    layout->addWidget(makeProportionalBar(45, DashboardPalette::Cyan, panel));

    auto *controlsRow = new QHBoxLayout;
    controlsRow->setSpacing(9);
    controlsRow->addWidget(makeLabel(QStringLiteral("◀◀")));
    auto *playButton = new QPushButton(QStringLiteral("⏸"));
    playButton->setFixedSize(28, 28);
    controlsRow->addWidget(playButton);
    controlsRow->addWidget(makeLabel(QStringLiteral("▶▶")));
    controlsRow->addWidget(makeLabel(QStringLiteral("01:53 / 04:12"), QStringLiteral("metricCaption")));
    controlsRow->addStretch();
    controlsRow->addWidget(legendDot(QStringLiteral("재계획"), DashboardPalette::Amber));
    controlsRow->addWidget(legendDot(QStringLiteral("급정지"), DashboardPalette::Red));
    controlsRow->addWidget(legendDot(QStringLiteral("반출"), DashboardPalette::Green));
    controlsRow->addWidget(makeBadgeLabel(QStringLiteral("1×"), QStringLiteral("muted"), panel));
    layout->addLayout(controlsRow);
    return panel;
}
