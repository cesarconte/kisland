set(CMAKE_INCLUDE_CURRENT_DIR ON)
add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.cesar.kisland\")

# Reusable, unit-testable modules (song recognition)
add_library(kisland_modules STATIC
    songrecognizer.cpp
    songrecognizer.h
)
set_target_properties(kisland_modules PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(kisland_modules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kisland_modules PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    KF6::I18n
    KF6::Notifications
)

plasma_add_applet(org.cesar.kisland
    CPP_SOURCES
        kislandapplet.cpp
    QML_SOURCES
        qml/main.qml
        qml/config.qml
        qml/ConfigGeneral.qml
    RESOURCES
        res/main.xml
)

target_include_directories(org.cesar.kisland PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(org.cesar.kisland PRIVATE
    kisland_modules
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Network
    Qt6::Quick
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
    KF6::Notifications
    Plasma::Plasma
    Plasma::PlasmaQuick
)

install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/shazam_recognize.py DESTINATION ${KDE_INSTALL_DATADIR}/kisland)
