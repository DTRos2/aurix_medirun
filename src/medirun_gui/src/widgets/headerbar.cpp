#include "headerbar.h"

#include "badgelabel.h"

#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>

namespace {

QLabel *makeBrandLogoLabel(QWidget *parent, const QSize &targetSize)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("brandLogo"));
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(targetSize);
    label->setMaximumSize(targetSize);

    const QString logoPath = QStringLiteral(":/images/aurix_logo_01.png");
    const QPixmap logo(logoPath);
    if (logo.isNull()) {
        qWarning() << "HeaderBar logo load failed:" << logoPath << "exists:" << QFile::exists(logoPath);
        label->setText(QStringLiteral("A"));
        return label;
    }

    label->setPixmap(logo.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    return label;
}

QFrame *verticalDivider(QWidget *parent)
{
    auto *divider = new QFrame(parent);
    divider->setObjectName(QStringLiteral("sectionDivider"));
    divider->setFixedWidth(1);
    return divider;
}

QString kindForConnectionState(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Connected:
        return QStringLiteral("green");
    case ConnectionState::Warning:
        return QStringLiteral("amber");
    case ConnectionState::Disconnected:
        return QStringLiteral("red");
    }
    return QStringLiteral("muted");
}

QString kindForBatterySnapshot(const TelemetrySnapshot &snapshot)
{
    if (!snapshot.batteryMessageReceived) {
        return snapshot.rosState == ConnectionState::Disconnected ? QStringLiteral("red") : QStringLiteral("muted");
    }

    if (snapshot.batteryPercent >= 50) {
        return QStringLiteral("green");
    }
    if (snapshot.batteryPercent >= 20) {
        return QStringLiteral("amber");
    }
    return QStringLiteral("red");
}

} // namespace

HeaderBar::HeaderBar(QWidget *parent)
    : QFrame(parent)
    , m_pageTitleLabel(nullptr)
    , m_rosChip(nullptr)
    , m_dbChip(nullptr)
    , m_camChip(nullptr)
    , m_batteryLabel(nullptr)
{
    setObjectName(QStringLiteral("headerBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(10);

    layout->addWidget(makeBrandLogoLabel(this, QSize(28, 28)));

    auto *brandTitle = new QLabel(QStringLiteral("AURIX"), this);
    brandTitle->setObjectName(QStringLiteral("brandTitle"));
    layout->addWidget(brandTitle);

    auto *divider = verticalDivider(this);
    divider->setFixedHeight(18);
    layout->addWidget(divider);

    m_pageTitleLabel = new QLabel(QStringLiteral("실시간 관제"), this);
    m_pageTitleLabel->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(m_pageTitleLabel);

    layout->addStretch(1);

    m_rosChip = makeBadgeLabel(QStringLiteral("ROS2"), QStringLiteral("green"), this);
    layout->addWidget(m_rosChip);
    m_dbChip = makeBadgeLabel(QStringLiteral("DB"), QStringLiteral("green"), this);
    layout->addWidget(m_dbChip);
    m_camChip = makeBadgeLabel(QStringLiteral("CAM"), QStringLiteral("red"), this);
    layout->addWidget(m_camChip);

    m_batteryLabel = new QLabel(QStringLiteral("--%"), this);
    m_batteryLabel->setObjectName(QStringLiteral("pageSubtitle"));
    layout->addWidget(m_batteryLabel);

    auto *estopDivider = verticalDivider(this);
    estopDivider->setFixedHeight(18);
    layout->addWidget(estopDivider);

    auto *estopButton = new QPushButton(QStringLiteral("■  긴급 정지"), this);
    estopButton->setObjectName(QStringLiteral("estopButton"));
    connect(estopButton, &QPushButton::clicked, this, &HeaderBar::estopRequested);
    layout->addWidget(estopButton);
}

void HeaderBar::setPageTitle(const QString &title)
{
    m_pageTitleLabel->setText(title);
}

void HeaderBar::applySnapshot(const TelemetrySnapshot &snapshot)
{
    m_rosChip->setProperty("badgeKind", kindForConnectionState(snapshot.rosState));
    m_rosChip->style()->unpolish(m_rosChip);
    m_rosChip->style()->polish(m_rosChip);

    m_dbChip->setProperty("badgeKind", kindForConnectionState(snapshot.dbState));
    m_dbChip->style()->unpolish(m_dbChip);
    m_dbChip->style()->polish(m_dbChip);

    m_batteryLabel->setText(snapshot.batteryPercentText.isEmpty() ? QStringLiteral("--%") : snapshot.batteryPercentText);
    m_batteryLabel->setProperty("batteryKind", kindForBatterySnapshot(snapshot));
    m_batteryLabel->style()->unpolish(m_batteryLabel);
    m_batteryLabel->style()->polish(m_batteryLabel);

    if (snapshot.batteryMessageReceived) {
        m_batteryLabel->setToolTip(QStringLiteral("배터리 전압 %1 V").arg(snapshot.batteryVoltage, 0, 'f', 2));
    } else if (snapshot.rosState == ConnectionState::Disconnected) {
        m_batteryLabel->setToolTip(QStringLiteral("배터리 상태 수신 끊김"));
    } else {
        m_batteryLabel->setToolTip(QStringLiteral("배터리 상태 수신 대기 중"));
    }
}

void HeaderBar::setCameraConnected(bool connected)
{
    m_camChip->setProperty("badgeKind", connected ? QStringLiteral("green") : QStringLiteral("red"));
    m_camChip->style()->unpolish(m_camChip);
    m_camChip->style()->polish(m_camChip);
}
