#include "missionopspage.h"

#include "../services/missioncontroller.h"
#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/proportionalbar.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QStyle>
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

void clearLayout(QLayout *layout)
{
    if (layout == nullptr) {
        return;
    }

    while (QLayoutItem *child = layout->takeAt(0)) {
        if (QLayout *nestedLayout = child->layout()) {
            // QLayout이 QLayoutItem을 상속하므로 nestedLayout과 child는 같은 객체다.
            // 아래 delete child가 이미 정리하므로 여기서 따로 delete하면 이중 해제.
            clearLayout(nestedLayout);
        }
        if (QWidget *widget = child->widget()) {
            delete widget;
        }
        delete child;
    }
}

QString badgeKindForQueueStatus(MissionQueueStatus status)
{
    switch (status) {
    case MissionQueueStatus::Waiting:
        return QStringLiteral("amber");
    case MissionQueueStatus::Active:
        return QStringLiteral("cyan");
    case MissionQueueStatus::Completed:
        return QStringLiteral("green");
    case MissionQueueStatus::Cancelled:
        return QStringLiteral("muted");
    case MissionQueueStatus::Diverted:
        return QStringLiteral("red");
    case MissionQueueStatus::Failed:
        return QStringLiteral("red");
    }
    return QStringLiteral("muted");
}

QString stateColor(MissionState state)
{
    if (state == MissionState::Failed || state == MissionState::Cancelled) {
        return DashboardPalette::Red.name();
    }
    if (state == MissionState::ReturningHome || state == MissionState::Diverting || state == MissionState::Cancelling) {
        return DashboardPalette::Amber.name();
    }
    return DashboardPalette::Cyan.name();
}

QString elapsedText(qint64 elapsedSeconds)
{
    const qint64 minutes = elapsedSeconds / 60;
    const qint64 seconds = elapsedSeconds % 60;
    return QStringLiteral("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

} // namespace

MissionOpsPage::MissionOpsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildAnomalyCard());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);
    m_queueTitleLabel = makeLabel(QStringLiteral("대기 호출 큐 · 0건"), QStringLiteral("metricCaption"));
    leftCol->addWidget(m_queueTitleLabel);
    leftCol->addWidget(buildWaitingQueue());
    leftCol->addWidget(makeLabel(QStringLiteral("진행 중 미션"), QStringLiteral("metricCaption")));
    leftCol->addWidget(buildInProgressMission(), 1);
    auto *leftWidget = new QWidget(canvas);
    leftWidget->setLayout(leftCol);
    bodyRow->addWidget(leftWidget, 5);

    bodyRow->addWidget(buildTodayHistory(), 6);
    layout->addLayout(bodyRow, 1);

    refreshAll();
}

void MissionOpsPage::setMissionController(MissionController *controller)
{
    if (m_controller == controller) {
        return;
    }
    m_controller = controller;
    if (m_controller == nullptr) {
        refreshAll();
        return;
    }

    connect(m_controller, &MissionController::missionChanged, this, &MissionOpsPage::refreshMission);
    connect(m_controller, &MissionController::queueChanged, this, &MissionOpsPage::refreshQueue);
    connect(m_controller, &MissionController::historyChanged, this, &MissionOpsPage::refreshHistory);
    connect(m_controller, &MissionController::connectionStateChanged, this, &MissionOpsPage::refreshAnomaly);
    connect(m_controller, &MissionController::retryStateChanged, this, &MissionOpsPage::refreshAnomaly);
    refreshAll();
}

QWidget *MissionOpsPage::buildAnomalyCard()
{
    auto *panel = panelFrame(QStringLiteral("anomalyCard"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(13, 11, 13, 11);
    layout->setSpacing(8);

    auto *headerRow = new QHBoxLayout;
    m_anomalyTitleLabel = makeLabel(QStringLiteral("⚠ 이상 상황 · 모니터링 중"));
    m_anomalyTitleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Red.name()));
    headerRow->addWidget(m_anomalyTitleLabel);
    headerRow->addStretch();
    m_anomalyMetaLabel = makeLabel(QStringLiteral("-"), QStringLiteral("metricCaption"));
    headerRow->addWidget(m_anomalyMetaLabel);
    layout->addLayout(headerRow);

    m_anomalyDetailLabel = makeLabel(QStringLiteral("상태를 불러오는 중입니다."));
    layout->addWidget(m_anomalyDetailLabel);
    layout->addWidget(makeLabel(QStringLiteral("ROS2/Nav2 상태와 미션 흐름을 기준으로 갱신됩니다."), QStringLiteral("metricCaption")));

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(6);
    m_retryButton = new QPushButton(QStringLiteral("재시도"));
    m_retryButton->setObjectName(QStringLiteral("primaryCtaButton"));
    auto *homeButton = new QPushButton(QStringLiteral("홈 복귀"));
    auto *cancelButton = new QPushButton(QStringLiteral("미션 취소"));
    auto *remoteButton = new QPushButton(QStringLiteral("원격 제어 →"));
    for (QPushButton *button : {m_retryButton, homeButton, cancelButton, remoteButton}) {
        buttonRow->addWidget(button);
    }
    layout->addLayout(buttonRow);

    connect(m_retryButton, &QPushButton::clicked, this, &MissionOpsPage::retryRequested);
    connect(homeButton, &QPushButton::clicked, this, &MissionOpsPage::returnHomeRequested);
    connect(cancelButton, &QPushButton::clicked, this, &MissionOpsPage::missionCancelRequested);
    connect(remoteButton, &QPushButton::clicked, this, &MissionOpsPage::remoteControlRequested);
    return panel;
}

QWidget *MissionOpsPage::buildWaitingQueue()
{
    auto *panel = panelFrame(QStringLiteral("callQueueCard"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);
    m_queueListLayout = layout;
    return panel;
}

QWidget *MissionOpsPage::buildInProgressMission()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    auto *titleRow = new QHBoxLayout;
    m_missionTitleLabel = makeLabel(QStringLiteral("대기 중"));
    titleRow->addWidget(m_missionTitleLabel);
    titleRow->addStretch();
    m_missionStateLabel = makeLabel(QStringLiteral("IDLE"));
    m_missionStateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Cyan.name()));
    titleRow->addWidget(m_missionStateLabel);
    layout->addLayout(titleRow);

    m_missionProgressBar = makeProportionalBar(0, DashboardPalette::Cyan, panel);
    layout->addWidget(m_missionProgressBar);
    m_missionDetailLabel = makeLabel(QStringLiteral("미션이 없습니다."), QStringLiteral("metricCaption"));
    layout->addWidget(m_missionDetailLabel);
    m_missionMetaLabel = makeLabel(QString(), QStringLiteral("metricCaption"));
    layout->addWidget(m_missionMetaLabel);
    layout->addStretch();
    return panel;
}

QWidget *MissionOpsPage::buildTodayHistory()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(QStringLiteral("오늘 처리 이력"), QStringLiteral("metricCaption")));

    auto *historyContainer = new QWidget(panel);
    m_historyListLayout = new QVBoxLayout(historyContainer);
    m_historyListLayout->setContentsMargins(0, 0, 0, 0);
    m_historyListLayout->setSpacing(8);
    layout->addWidget(historyContainer, 1);

    layout->addWidget(makeLabel(QStringLiteral("모든 판단은 사용자 ID와 함께 저장소에 기록됩니다."), QStringLiteral("metricCaption")));
    return panel;
}

void MissionOpsPage::refreshQueue()
{
    if (m_queueTitleLabel == nullptr || m_queueListLayout == nullptr) {
        return;
    }

    m_queueTitleLabel->setText(m_controller != nullptr ? m_controller->queueSummaryText() : QStringLiteral("대기 호출 큐 · 0건"));

    clearLayout(m_queueListLayout);

    if (m_controller == nullptr) {
        m_queueListLayout->addWidget(makeLabel(QStringLiteral("대기 중인 호출이 없습니다."), QStringLiteral("metricCaption")));
        return;
    }

    const QVector<MissionQueueItem> items = m_controller->waitingQueue();
    bool anyVisible = false;
    for (const MissionQueueItem &item : items) {
        if (item.status != MissionQueueStatus::Waiting && item.status != MissionQueueStatus::Active) {
            continue;
        }

        anyVisible = true;
        auto *card = new QWidget;
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        auto *titleRow = new QHBoxLayout;
        titleRow->addWidget(makeLabel(QStringLiteral("%1 · %2").arg(item.waypointId, item.waypointName)));
        titleRow->addWidget(makeBadgeLabel(missionQueueStatusText(item.status), badgeKindForQueueStatus(item.status), card));
        titleRow->addStretch();
        layout->addLayout(titleRow);

        const QString subtitle = QStringLiteral("%1 접수 · 순서 %2 · source=%3 · queue_id=%4")
                                     .arg(item.requestedAt.toString(QStringLiteral("HH:mm:ss")))
                                     .arg(item.enqueueOrder)
                                     .arg(item.requestSource)
                                     .arg(item.queueId);
        layout->addWidget(makeLabel(subtitle, QStringLiteral("metricCaption")));

        auto *buttonRow = new QHBoxLayout;
        buttonRow->setSpacing(6);
        auto *acceptButton = new QPushButton(QStringLiteral("확인·전환"));
        acceptButton->setObjectName(QStringLiteral("primaryCtaButton"));
        auto *cancelButton = new QPushButton(QStringLiteral("취소"));
        buttonRow->addWidget(acceptButton);
        buttonRow->addStretch();
        buttonRow->addWidget(cancelButton);
        layout->addLayout(buttonRow);

        connect(acceptButton, &QPushButton::clicked, this, [this, queueId = item.queueId]() { emit queueDivertRequested(queueId); });
        connect(cancelButton, &QPushButton::clicked, this, [this, queueId = item.queueId]() { emit queueCancelRequested(queueId); });
        if (item.status == MissionQueueStatus::Active) {
            acceptButton->setEnabled(false);
        }

        m_queueListLayout->addWidget(card);
    }

    if (!anyVisible) {
        m_queueListLayout->addWidget(makeLabel(QStringLiteral("대기 중인 호출이 없습니다."), QStringLiteral("metricCaption")));
    }
    m_queueListLayout->addStretch();
}

void MissionOpsPage::refreshMission()
{
    if (m_missionTitleLabel == nullptr) {
        return;
    }

    if (m_controller == nullptr) {
        m_missionTitleLabel->setText(QStringLiteral("대기 중"));
        m_missionStateLabel->setText(QStringLiteral("IDLE"));
        m_missionDetailLabel->setText(QStringLiteral("미션이 없습니다."));
        m_missionMetaLabel->clear();
        return;
    }

    const MissionSnapshot snapshot = m_controller->missionSnapshot();
    if (snapshot.missionId.isEmpty()) {
        m_missionTitleLabel->setText(QStringLiteral("대기 중"));
        m_missionStateLabel->setText(QStringLiteral("IDLE"));
        m_missionDetailLabel->setText(QStringLiteral("미션이 없습니다."));
        m_missionMetaLabel->clear();
        return;
    }

    m_missionTitleLabel->setText(QStringLiteral("%1 · %2").arg(snapshot.missionId, snapshot.destinationName));
    m_missionStateLabel->setText(missionStateText(snapshot.state));
    m_missionStateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(stateColor(snapshot.state)));

    const int progress = snapshot.progressPercent < 0 ? 0 : snapshot.progressPercent;
    auto *parentFrame = qobject_cast<QFrame *>(m_missionProgressBar->parentWidget());
    auto *newBar = makeProportionalBar(progress, snapshot.state == MissionState::Failed ? DashboardPalette::Red : DashboardPalette::Cyan, parentFrame);
    auto *layout = qobject_cast<QVBoxLayout *>(parentFrame->layout());
    layout->replaceWidget(m_missionProgressBar, newBar);
    delete m_missionProgressBar;
    m_missionProgressBar = newBar;

    m_missionDetailLabel->setText(
        QStringLiteral("%1 · 남은 %2m · 경과 %3")
            .arg(snapshot.currentWaypointName)
            .arg(snapshot.distanceRemainingMeters < 0.0 ? QStringLiteral("--")
                                                        : QString::number(snapshot.distanceRemainingMeters, 'f', 1))
            .arg(elapsedText(snapshot.elapsedSeconds)));

    const QString meta = QStringLiteral("from=%1 · next=%2 · queue=%3 · home=%4%5")
                             .arg(snapshot.originName, snapshot.nextWaypointName)
                             .arg(snapshot.waitingCount)
                             .arg(snapshot.returningHome ? QStringLiteral("Y") : QStringLiteral("N"))
                             .arg(snapshot.failureReason.isEmpty() ? QString()
                                                                   : QStringLiteral(" · reason=%1").arg(snapshot.failureReason));
    m_missionMetaLabel->setText(meta);
}

void MissionOpsPage::refreshHistory()
{
    if (m_historyListLayout == nullptr) {
        return;
    }

    clearLayout(m_historyListLayout);

    if (m_controller == nullptr) {
        m_historyListLayout->addWidget(makeLabel(QStringLiteral("오늘 기록이 없습니다."), QStringLiteral("metricCaption")));
        return;
    }

    const QVector<MissionHistoryEntry> entries = m_controller->todayHistory();
    if (entries.isEmpty()) {
        m_historyListLayout->addWidget(makeLabel(QStringLiteral("오늘 기록이 없습니다."), QStringLiteral("metricCaption")));
        return;
    }

    for (const MissionHistoryEntry &entry : entries) {
        auto *rowLayout = new QHBoxLayout;
        rowLayout->setSpacing(7);
        auto *timeLabel = makeLabel(entry.occurredAt.toString(QStringLiteral("HH:mm")), QStringLiteral("metricCaption"));
        timeLabel->setFixedWidth(36);
        rowLayout->addWidget(timeLabel);
        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(1);
        textCol->addWidget(makeLabel(QStringLiteral("%1 · %2").arg(entry.eventType, entry.result)));
        const QString detail = QStringLiteral("%1 · mission=%2 · queue=%3 · %4")
                                   .arg(entry.waypointName.isEmpty() ? QStringLiteral("-") : entry.waypointName,
                                        entry.missionId.isEmpty() ? QStringLiteral("-") : entry.missionId,
                                        entry.queueId.isEmpty() ? QStringLiteral("-") : entry.queueId,
                                        entry.detailMessage);
        textCol->addWidget(makeLabel(detail, QStringLiteral("metricCaption")));
        rowLayout->addLayout(textCol, 1);
        m_historyListLayout->addLayout(rowLayout);
    }
    m_historyListLayout->addStretch();
}

void MissionOpsPage::refreshAnomaly()
{
    if (m_anomalyTitleLabel == nullptr) {
        return;
    }

    if (m_controller == nullptr) {
        m_anomalyTitleLabel->setText(QStringLiteral("⚠ 이상 상황 · 모니터링 중"));
        m_anomalyMetaLabel->setText(QStringLiteral("-"));
        m_anomalyDetailLabel->setText(QStringLiteral("상태를 불러오는 중입니다."));
        if (m_retryButton != nullptr) {
            m_retryButton->setEnabled(false);
        }
        return;
    }

    m_anomalyTitleLabel->setText(m_controller->anomalyTitle());
    m_anomalyMetaLabel->setText(m_controller->anomalyMeta());
    m_anomalyDetailLabel->setText(m_controller->anomalyDetail());
    if (m_retryButton != nullptr) {
        m_retryButton->setEnabled(!m_controller->retryInProgress());
        m_retryButton->setText(m_controller->retryInProgress() ? QStringLiteral("재시도 중...") : QStringLiteral("재시도"));
    }
}

void MissionOpsPage::refreshAll()
{
    refreshAnomaly();
    refreshQueue();
    refreshMission();
    refreshHistory();
}
