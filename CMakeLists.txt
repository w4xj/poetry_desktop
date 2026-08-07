cmake_minimum_required(VERSION 3.25)
project(poetry_desktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_PREFIX_PATH AND WIN32 AND EXISTS "E:/codeSoft/QT/6.8.3/mingw_64")
    set(CMAKE_PREFIX_PATH "E:/codeSoft/QT/6.8.3/mingw_64")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)

set(POETRY_CORE_SOURCES
    src/core/models.cpp
    src/core/config_store.cpp
    src/core/libraries.cpp
    src/core/renderer.cpp
    src/core/wallpaper_service.cpp
    src/platform/windows_wallpaper_setter.cpp
)

set(POETRY_HEADERS
    src/core/models.h
    src/core/config_store.h
    src/core/libraries.h
    src/core/renderer.h
    src/core/wallpaper_service.h
    src/platform/windows_wallpaper_setter.h
)

qt_add_resources(POETRY_RESOURCES resources/resources.qrc)
add_executable(poetry_desktop WIN32
    main.cpp
    src/ui/main_window.cpp
    ${POETRY_CORE_SOURCES}
    ${POETRY_HEADERS}
    ${POETRY_RESOURCES}
)

target_include_directories(poetry_desktop PRIVATE src)
target_link_libraries(poetry_desktop PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
if(WIN32)
    target_compile_definitions(poetry_desktop PRIVATE UNICODE _UNICODE NOMINMAX)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(poetry_tests
        tests/test_core.cpp
        ${POETRY_CORE_SOURCES}
        ${POETRY_HEADERS}
        ${POETRY_RESOURCES}
    )
    target_include_directories(poetry_tests PRIVATE src)
    target_link_libraries(poetry_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    if(WIN32)
        target_compile_definitions(poetry_tests PRIVATE UNICODE _UNICODE NOMINMAX)
    endif()
    add_test(NAME poetry_core_tests COMMAND poetry_tests)

    add_executable(poetry_ui_tests
        tests/test_ui.cpp
        src/ui/main_window.cpp
        ${POETRY_CORE_SOURCES}
        ${POETRY_HEADERS}
        ${POETRY_RESOURCES}
    )
    target_include_directories(poetry_ui_tests PRIVATE src)
    target_link_libraries(poetry_ui_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Test)
    if(WIN32)
        target_compile_definitions(poetry_ui_tests PRIVATE UNICODE _UNICODE NOMINMAX)
    endif()
    add_test(NAME poetry_ui_tests COMMAND poetry_ui_tests)
endif()





if(WIN32 AND BUILD_TESTING)
    get_filename_component(POETRY_QT_INSTALL_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
    set_tests_properties(poetry_core_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "PATH=${POETRY_QT_INSTALL_PREFIX}/bin;QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${POETRY_QT_INSTALL_PREFIX}/plugins")
    set_tests_properties(poetry_ui_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "PATH=${POETRY_QT_INSTALL_PREFIX}/bin;QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${POETRY_QT_INSTALL_PREFIX}/plugins")
endif()


