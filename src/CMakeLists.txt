set(CMAKE_INCLUDE_CURRENT_DIR ON)
add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.cesar.kisland\")

plasma_add_applet(org.cesar.kisland
    CPP_SOURCES
        kislandapplet.cpp
    QML_SOURCES
        qml/main.qml
    RESOURCES
        res/main.xml
)

target_include_directories(org.cesar.kisland PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(org.cesar.kisland PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Quick
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
    Plasma::Plasma
    Plasma::PlasmaQuick
)
