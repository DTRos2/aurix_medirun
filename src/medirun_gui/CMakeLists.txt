cmake_minimum_required(VERSION 3.19)
project(medirun_gui LANGUAGES CXX)

# medirun_gui는 시스템 Qt가 6.5 미만이면 사용자가 직접 빌드한 Qt를
# LD_LIBRARY_PATH로 지정해 실행한다(README 참고). 그 값을 통째로 덮어써도
# (ROS2 lib 경로가 날아가도) 이 바이너리가 rclcpp/nav2 계열 공유 라이브러리를
# 계속 찾도록, LD_LIBRARY_PATH에 의존하지 않고 링크 시점 라이브러리 경로를
# RPATH로 실행 파일에 직접 새겨 넣는다.
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(ament_cmake REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(action_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(yaml-cpp REQUIRED)


qt_standard_project_setup()

qt_add_executable(medirun_gui
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
    src/app/appsession.h
    src/app/guiapplicationcontroller.cpp
    src/app/guiapplicationcontroller.h
    src/dialogs/logindialog.cpp
    src/dialogs/logindialog.h
    src/models/telemetrymodel.h
    src/models/missiontypes.h
    src/models/waypointtypes.h
    src/ros/navigationservice.h
    src/ros/nav2navigationservice.cpp
    src/ros/nav2navigationservice.h
    src/services/authservice.h
    src/services/dbauthservice.cpp
    src/services/dbauthservice.h
    src/services/dbconnection.cpp
    src/services/dbconnection.h
    src/services/dboperationsoverviewservice.cpp
    src/services/dboperationsoverviewservice.h
    src/services/operationsoverviewservice.h
    src/services/dbrobotservice.cpp
    src/services/dbrobotservice.h
    src/services/robotservice.h
    src/services/telemetrylogrecorder.cpp
    src/services/telemetrylogrecorder.h
    src/services/navgoallogrecorder.cpp
    src/services/navgoallogrecorder.h
    src/services/missioncontroller.cpp
    src/services/missioncontroller.h
    src/services/missionrosinterface.cpp
    src/services/missionrosinterface.h
    src/services/missionstorageservice.cpp
    src/services/missionstorageservice.h
    src/services/localauthservice.cpp
    src/services/localauthservice.h
    src/services/camerafeedprovider.h
    src/services/eventrepository.h
    src/services/mapfile.cpp
    src/services/mapfile.h
    src/services/mockcamerafeedprovider.cpp
    src/services/mockcamerafeedprovider.h
    src/services/mocktelemetryprovider.cpp
    src/services/mocktelemetryprovider.h
    src/services/roscamerafeedprovider.cpp
    src/services/roscamerafeedprovider.h
    src/services/rosbatterytelemetryprovider.cpp
    src/services/rosbatterytelemetryprovider.h
    src/services/telemetryprovider.h
    src/services/waypointfile.cpp
    src/services/waypointfile.h
    src/services/waypointrepository.cpp
    src/services/waypointrepository.h
    src/theme/dashboardpalette.h
    src/widgets/badgelabel.h
    src/widgets/dashboardcentralwidget.cpp
    src/widgets/dashboardcentralwidget.h
    src/widgets/headerbar.cpp
    src/widgets/headerbar.h
    src/widgets/livesearchlineedit.cpp
    src/widgets/livesearchlineedit.h
    src/widgets/mapviewwidget.cpp
    src/widgets/mapviewwidget.h
    src/widgets/waypointmapwidget.cpp
    src/widgets/waypointmapwidget.h
    src/widgets/pageid.h
    src/widgets/proportionalbar.h
    src/widgets/sidenav.cpp
    src/widgets/sidenav.h
    src/pages/callhistorypage.cpp
    src/pages/callhistorypage.h
    src/pages/livecontrolpage.cpp
    src/pages/livecontrolpage.h
    src/pages/missionopspage.cpp
    src/pages/missionopspage.h
    src/pages/operationsoverviewpage.cpp
    src/pages/operationsoverviewpage.h
    src/pages/recordsplaybackpage.cpp
    src/pages/recordsplaybackpage.h
    src/pages/robotmanagementpage.cpp
    src/pages/robotmanagementpage.h
    src/pages/remotecontrolpage.cpp
    src/pages/remotecontrolpage.h
    src/pages/slammappingpage.cpp
    src/pages/slammappingpage.h
    src/pages/systemsettingspage.cpp
    src/pages/systemsettingspage.h
    src/pages/usermanagementpage.cpp
    src/pages/usermanagementpage.h
    src/pages/waypointmanagementpage.cpp
    src/pages/waypointmanagementpage.h
    src/windows/mainwindowstyler.cpp
    src/windows/mainwindowstyler.h
    src/widgets/logocarouselwidget.cpp
    src/widgets/logocarouselwidget.h
    src/widgets/splashassetresolver.cpp
    src/widgets/splashassetresolver.h
    src/widgets/splashscreencontentwidget.cpp
    src/widgets/splashscreencontentwidget.h
    src/widgets/splashscreenwidget.cpp
    src/widgets/splashscreenwidget.h
)

set_target_properties(medirun_gui PROPERTIES
    OUTPUT_NAME medirun_gui_app
)

target_link_libraries(medirun_gui
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Sql
        yaml-cpp
        crypt
)

ament_target_dependencies(medirun_gui
    PUBLIC
        rclcpp
        rclcpp_action
        nav2_msgs
        lifecycle_msgs
        geometry_msgs
        nav_msgs
        sensor_msgs
        action_msgs
        std_msgs
        cv_bridge
        image_transport
        tf2
        tf2_msgs
        tf2_ros
)

install(TARGETS medirun_gui
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)

qt_generate_deploy_app_script(
    TARGET medirun_gui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

ament_package()
