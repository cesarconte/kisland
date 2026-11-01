find_package(Qt6Test ${QT_MIN_VERSION} REQUIRED)

add_executable(test_kisland test_kisland.cpp)
target_compile_definitions(test_kisland PRIVATE KISLAND_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(test_kisland PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Test
)

add_test(NAME test_kisland COMMAND test_kisland)
