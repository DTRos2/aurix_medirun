#ifndef WAYPOINTMANAGEMENTPAGE_H
#define WAYPOINTMANAGEMENTPAGE_H

#include "../widgets/waypointmapwidget.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

struct WaypointRecord
{
    QString id;
    QString name;
    double x {0.0};
    double y {0.0};
    double theta {0.0};
    WaypointMarkerKind kind {WaypointMarkerKind::Waypoint};
    QPointF normalizedPos;
};

struct SavedRoute
{
    QString name;
    int pointCount {0};
};

class WaypointManagementPage : public QWidget
{
    Q_OBJECT

public:
    explicit WaypointManagementPage(QWidget *parent = nullptr);

private:
    QWidget *buildHeader();
    QWidget *buildWarningBanner();
    QWidget *buildWaypointTab();
    QWidget *buildTrajectoryTab();

    QWidget *buildWaypointListPanel();
    QWidget *buildWaypointEditPanel();
    QWidget *buildRouteAddPanel();
    QWidget *buildRouteOrderPanel();
    QWidget *buildRouteNamePanel();
    QWidget *buildSavedRoutesBar();

    void selectTab(int index);
    void refreshWaypointList();
    void refreshMapMarkers();
    void refreshRouteCombo();
    void refreshSavedRoutesBar();
    void renumberRouteOrderItems();

    void selectWaypoint(const QString &id);
    void loadWaypointIntoForm(const WaypointRecord &record);
    void handleNewWaypointRequested();
    void handleSaveWaypoint();
    void handleDeleteWaypoint();
    QString nextIdForKind(WaypointMarkerKind kind) const;
    QPointF normalizedFromMeters(double x, double y) const;

    void handleAddToRoute();
    void handleRemoveSelectedFromRoute();
    void handleClearRoute();
    void handleSaveTrajectory();
    void handleSavedRouteChipClicked(const QString &name);
    void handleDeleteSelectedSavedRoute();
    void handleLoadMap();
    void handleClearMap();

    QVector<WaypointRecord> m_waypoints;
    QString m_selectedWaypointId;
    bool m_creatingNew;

    QVector<SavedRoute> m_savedRoutes;
    QString m_selectedSavedRoute;

    // shell
    QPushButton *m_waypointTabButton;
    QPushButton *m_trajectoryTabButton;
    QStackedWidget *m_tabStack;
    QLabel *m_mapStatusLabel;

    // waypoint tab
    WaypointMapWidget *m_waypointMap;
    QListWidget *m_waypointList;
    QLabel *m_waypointListCountLabel;
    QLabel *m_editPanelTitle;
    QLineEdit *m_nameEdit;
    QLineEdit *m_xEdit;
    QLineEdit *m_yEdit;
    QLineEdit *m_thetaEdit;
    QComboBox *m_kindCombo;
    QPushButton *m_saveWaypointButton;
    QPushButton *m_deleteWaypointButton;
    QLabel *m_waypointStatusLabel;

    // trajectory tab
    WaypointMapWidget *m_routeMap;
    QComboBox *m_routeAddCombo;
    QListWidget *m_routeOrderList;
    QLabel *m_routeOrderCountLabel;
    QLineEdit *m_routeNameEdit;
    QWidget *m_savedRoutesBar;
    QLabel *m_trajectoryStatusLabel;
};

#endif // WAYPOINTMANAGEMENTPAGE_H
