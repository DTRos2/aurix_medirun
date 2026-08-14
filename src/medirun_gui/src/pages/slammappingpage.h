#ifndef SLAMMAPPINGPAGE_H
#define SLAMMAPPINGPAGE_H

#include <QImage>
#include <QFocusEvent>
#include <QHideEvent>
#include <QPointF>
#include <QProcess>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <memory>

class CameraFeedProvider;
class MapViewWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class SlamMapBridge;

class SlamMappingPage : public QWidget
{
    Q_OBJECT

public:
    explicit SlamMappingPage(QWidget *parent = nullptr);
    ~SlamMappingPage() override;

    bool handleManualKeyPress(int key);
    bool handleManualKeyRelease(int key);
    void setCameraFeedProvider(CameraFeedProvider *provider);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class MappingUiState {
        Idle,
        CheckingBringup,
        SensorsReady,
        Starting,
        WaitingForMap,
        Running,
        Error,
    };

    QWidget *buildStatusBanner();
    QWidget *buildActionRow();
    QWidget *buildMappingStatusPanel();
    QWidget *buildLogPanel();
    QWidget *buildDirectionPad();
    QWidget *buildSpeedLimitPanel();
    QWidget *buildCameraOverlay();
    void positionCameraOverlay();
    void attachMapBridgeCallbacks();
    void resetMappingView(const QString &caption, bool mappingRunning);
    void updateSlamUiState();
    void appendUiLog(const QString &message);
    void setStatusMessage(const QString &message, const QString &kind = QStringLiteral("pageSubtitle"));
    void setLiveStatusMessage(const QString &message, const QString &kind = QStringLiteral("metricCaption"));
    QString mapsDirectoryPath() const;
    QString nextMapBasePath() const;
    bool startMappingProcess();
    void stopMappingProcess();
    void beginCartographerLaunch();
    void handleNav2PauseCompleted(bool localizationPaused);
    void logNav2ResumeResult(const QStringList &failedManagers);
    void setMappingUiState(MappingUiState state, const QString &detail = QString());
    void refreshSensorStatus();
    void handleSlamProcessStdout();
    void handleSlamProcessStderr();
    void handleSaveProcessStdout();
    void handleSaveProcessStderr();

private slots:
    void handleStartMapping();
    void handleResetMapping();
    void handleAboutToQuit();
    void handleSaveMap();
    void handleSlamProcessStateChanged();
    void handleSlamProcessErrorOccurred();
    void handleSlamProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleSaveProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleSaveProcessErrorOccurred();
    void handleMapImageUpdated(const QImage &image);
    void handleRobotPoseUpdated(const QPointF &normalizedPos, qreal headingDegrees);
    void handleBridgeStatusUpdated(const QString &message);
    void handleManualDriveTick();
    void handleMapRefreshTick();
    void handleMapTimeout();
    void handleCameraFrameReady(const QImage &frame);

private:
    void applyManualKeyState();
    void setManualDriveCommand(double linearX, double angularZ, const QString &statusMessage);
    void stopManualDrive();
    bool consumeManualKey(int key);
    bool releaseManualKey(int key);
    MapViewWidget *m_mapView;
    QLabel *m_cameraViewLabel {nullptr};
    CameraFeedProvider *m_cameraFeedProvider {nullptr};
    // 오버레이 폭(220, 고정)에 맞춰 이 프레임 크기 기준으로 계산한 높이를
    // 한 번 적용했는지 추적 — 실제 카메라 프레임 비율(해상도)을 알기 전엔
    // buildCameraOverlay()의 임시 높이를 쓰다가, 첫 프레임이 오면 정확한
    // 비율로 다시 계산한다. 매 프레임 setFixedSize를 반복 호출하지 않도록
    // 비율이 바뀔 때만 갱신한다.
    QSize m_lastCameraFrameSize;
    QLabel *m_statusBannerTitleLabel;
    QLabel *m_statusBannerDetailLabel;
    QLabel *m_mappingStatusValueLabel;
    QLabel *m_saveFormatValueLabel;
    QLabel *m_statusMessageLabel;
    QLabel *m_liveStatusMessageLabel;
    QPlainTextEdit *m_logView;
    QPushButton *m_startButton;
    QPushButton *m_saveButton;
    QPushButton *m_resetButton;
    QProcess *m_slamProcess;
    QProcess *m_saveProcess;
    QTimer *m_manualDriveTimer;
    QTimer *m_mapRefreshTimer;
    QTimer *m_mapTimeoutTimer;
    std::unique_ptr<SlamMapBridge> m_mapBridge;
    QString m_lastSavedBasePath;
    QString m_pendingSaveBasePath;
    QString m_lastSaveStdout;
    QString m_lastSaveStderr;
    bool m_mappingActive;
    bool m_stoppingSlamProcess {false};
    bool m_hasReceivedRealMap {false};
    double m_manualLinearX;
    double m_manualAngularZ;
    bool m_upPressed {false};
    bool m_downPressed {false};
    bool m_leftPressed {false};
    bool m_rightPressed {false};
    MappingUiState m_mappingUiState {MappingUiState::Idle};
    QString m_mappingUiDetail;
};

#endif // SLAMMAPPINGPAGE_H
