cmake_minimum_required(VERSION 3.19)
project(medirun_gui LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

qt_add_executable(medirun_gui
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
    src/widgets/logocarouselwidget.cpp
    src/widgets/logocarouselwidget.h
    src/widgets/splashassetresolver.cpp
    src/widgets/splashassetresolver.h
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
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET medirun_gui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
