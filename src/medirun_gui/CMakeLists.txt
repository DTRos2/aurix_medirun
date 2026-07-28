cmake_minimum_required(VERSION 3.19)
project(medirun_gui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(ament_cmake REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

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
    src/services/authservice.h
    src/services/localauthservice.cpp
    src/services/localauthservice.h
    src/services/eventrepository.h
    src/services/mocktelemetryprovider.cpp
    src/services/mocktelemetryprovider.h
    src/services/telemetryprovider.h
    src/theme/dashboardpalette.h
    src/widgets/badgelabel.h
    src/widgets/dashboardcentralwidget.cpp
    src/widgets/dashboardcentralwidget.h
    src/widgets/headerbar.cpp
    src/widgets/headerbar.h
    src/widgets/mapviewwidget.cpp
    src/widgets/mapviewwidget.h
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
    src/pages/remotecontrolpage.cpp
    src/pages/remotecontrolpage.h
    src/pages/slammappingpage.cpp
    src/pages/slammappingpage.h
    src/pages/systemsettingspage.cpp
    src/pages/systemsettingspage.h
    src/pages/usermanagementpage.cpp
    src/pages/usermanagementpage.h
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
