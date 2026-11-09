cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME lingmo-dock)
project(${PROJECT_NAME})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 CONFIG REQUIRED Widgets DBus Gui GuiPrivate Concurrent LinguistTools QuickControls2)
find_package(XCB REQUIRED COMPONENTS XCB XFIXES COMPOSITE RENDER SHAPE)

find_package(KF6WindowSystem REQUIRED)
# Wayland session: layer-shell panel and KWin's window list
find_package(LayerShellQt REQUIRED)
find_package(KWayland REQUIRED)

set(SRCS
    src/applicationitem.h
    src/applicationmodel.cpp
    src/docksettings.cpp
    src/iconthemeimageprovider.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/systemappmonitor.cpp
    src/systemappitem.cpp
    src/processprovider.cpp
    src/trashmanager.cpp
    src/utils.cpp
    src/windowinterface.cpp
    src/xwindowinterface.cpp
    src/waylandwindowinterface.cpp
    src/activity.cpp
    src/windowpreviewmodel.cpp
    src/windowthumbnail.cpp

    src/fakewindow.cpp
)

set(RESOURCES
    resources.qrc
)

qt_add_dbus_adaptor(DBUS_SOURCES
                     src/com.lingmo.Dock.xml
                     src/mainwindow.h MainWindow)
set_source_files_properties(${DBUS_SOURCES} PROPERTIES SKIP_AUTOGEN ON)

add_executable(${PROJECT_NAME} ${SRCS} ${DBUS_SOURCES} ${RESOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Concurrent
        Qt6::DBus
        KF6::WindowSystem
        XCB::XCB
        XCB::COMPOSITE
        XCB::RENDER
        XCB::SHAPE
        LayerShellQt::Interface
        Plasma::KWaylandClient
)

file(GLOB TS_FILES translations/*.ts)
foreach(filepath ${TS_FILES})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" filename ${filepath})
    list(APPEND ts_files_replaced ${filename})
endforeach()
qt_add_translation(QM_FILES ${ts_files_replaced})

add_custom_target(translations ALL DEPENDS ${QM_FILES} SOURCES ${TS_FILES})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${QM_FILES} DESTINATION /usr/share/${PROJECT_NAME}/translations)
install(FILES lingmo-dock-list.conf DESTINATION /etc)
# Not shown in menus: tells KWin (Wayland) which privileged interfaces the dock uses
install(FILES data/lingmo-dock.desktop DESTINATION share/applications)
