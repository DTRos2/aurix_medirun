#ifndef SLAMMAPPINGPAGE_H
#define SLAMMAPPINGPAGE_H

#include <QImage>
#include <QPointF>
#include <QProcess>
#include <QTimer>
#include <QWidget>

#include <memory>

class MapViewWidget;
class QLabel;
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

private:
    QWidget *buildStatusBanner();
    QWidget *buildActionRow();
    QWidget *buildMappingStatusPanel();
    QWidget *buildDirectionPad();
    QWidget *buildSpeedLimitPanel();
    void updateSlamUiState();
    void setStatusMessage(const QString &message, const QString &kind = QStringLiteral("pageSubtitle"));
    void setLiveStatusMessage(const QString &message, const QString &kind = QStringLiteral("metricCaption"));
    QString mapsDirectoryPath() const;
    QString nextMapBasePath() const;
    bool finalizeSavedMap(const QString &basePath, QString *errorMessage) const;
    bool startMappingProcess();
    void stopMappingProcess();

private slots:
    void handleStartMapping();
    void handleResetMapping();
    void handleSaveMap();
    void handleSlamProcessStateChanged();
    void handleSlamProcessErrorOccurred();
    void handleSaveProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleSaveProcessErrorOccurred();
    void handleMapImageUpdated(const QImage &image);
    void handleRobotPoseUpdated(const QPointF &normalizedPos, qreal headingDegrees);
    void handleBridgeStatusUpdated(const QString &message);
    void handleManualDriveTick();

private:
    void setManualDriveCommand(double linearX, double angularZ, const QString &statusMessage);
    void stopManualDrive();
    bool consumeManualKey(int key);
    bool releaseManualKey(int key);
    MapViewWidget *m_mapView;
    QLabel *m_statusBannerTitleLabel;
    QLabel *m_statusBannerDetailLabel;
    QLabel *m_mappingStatusValueLabel;
    QLabel *m_saveFormatValueLabel;
    QLabel *m_statusMessageLabel;
    QLabel *m_liveStatusMessageLabel;
    QPushButton *m_startButton;
    QPushButton *m_saveButton;
    QPushButton *m_resetButton;
    QProcess *m_slamProcess;
    QProcess *m_saveProcess;
    QTimer *m_manualDriveTimer;
    std::unique_ptr<SlamMapBridge> m_mapBridge;
    QString m_lastSavedBasePath;
    bool m_mappingActive;
    double m_manualLinearX;
    double m_manualAngularZ;
};

#endif // SLAMMAPPINGPAGE_H
