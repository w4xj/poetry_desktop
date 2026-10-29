cmake_minimum_required(VERSION 3.25)
project(poetry_desktop VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Fallbacks for known local Qt installs only. The list is machine-specific by design:
# pass -DCMAKE_PREFIX_PATH (or set the environment variable) to override it. See README.md.
if(NOT CMAKE_PREFIX_PATH AND WIN32)
    foreach(qt_root "S:/soft/QT/6.8.3/mingw_64" "E:/codeSoft/QT/6.8.3/mingw_64")
        if(EXISTS "${qt_root}/lib/cmake/Qt6/Qt6Config.cmake")
            set(CMAKE_PREFIX_PATH "${qt_root}")
            break()
        endif()
    endforeach()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)

set(POETRY_CORE_SOURCES
    src/core/models.cpp
    src/core/config_store.cpp
    src/core/libraries.cpp
    src/core/renderer.cpp
    src/core/wallpaper_service.cpp
    src/platform/windows_autostart.cpp
    src/platform/windows_wallpaper_setter.cpp
)

set(POETRY_HEADERS
    src/core/models.h
    src/core/config_store.h
    src/core/libraries.h
    src/core/renderer.h
    src/core/wallpaper_service.h
    src/platform/windows_autostart.h
    src/platform/windows_wallpaper_setter.h
)

qt_add_resources(POETRY_RESOURCES resources/resources.qrc)

set(POETRY_APP_SOURCES
    main.cpp
    src/ui/main_window.cpp
    ${POETRY_CORE_SOURCES}
    ${POETRY_HEADERS}
    ${POETRY_RESOURCES}
)
if(WIN32)
    # Gives the .exe itself an icon, which is what a taskbar pin shows.
    list(APPEND POETRY_APP_SOURCES resources/app.rc)
endif()

add_executable(poetry_desktop WIN32 ${POETRY_APP_SOURCES})

if(WIN32)
    target_include_directories(poetry_desktop PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()

target_include_directories(poetry_desktop PRIVATE src)
target_link_libraries(poetry_desktop PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
if(WIN32)
    target_compile_definitions(poetry_desktop PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(poetry_desktop PRIVATE advapi32)
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
        target_link_libraries(poetry_tests PRIVATE advapi32)
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
        target_link_libraries(poetry_ui_tests PRIVATE advapi32)
    endif()
    add_test(NAME poetry_ui_tests COMMAND poetry_ui_tests)
endif()





get_filename_component(POETRY_QT_INSTALL_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)

if(WIN32 AND BUILD_TESTING)
    set_tests_properties(poetry_core_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "PATH=${POETRY_QT_INSTALL_PREFIX}/bin;QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${POETRY_QT_INSTALL_PREFIX}/plugins")
    set_tests_properties(poetry_ui_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "PATH=${POETRY_QT_INSTALL_PREFIX}/bin;QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${POETRY_QT_INSTALL_PREFIX}/plugins")
endif()

# ---------------------------------------------------------------------------
# Packaging. Build a Release tree, then:
#   cpack                                  -> portable ZIP
#   cmake --build <build> --target installer -> per-user NSIS installer
# See README.md "打包发布".
# ---------------------------------------------------------------------------
install(TARGETS poetry_desktop RUNTIME DESTINATION ".")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/README.txt" DESTINATION ".")

if(WIN32)
    # windeployqt must run against the staged copy, so it belongs in the install
    # step: CPack installs into a scratch prefix, and the installer target does
    # the same through `cmake --install`.
    set(POETRY_WINDEPLOYQT "${POETRY_QT_INSTALL_PREFIX}/bin/windeployqt.exe")
    install(CODE "
        execute_process(
            COMMAND \"${POETRY_WINDEPLOYQT}\"
                --release --no-translations --compiler-runtime
                \"\${CMAKE_INSTALL_PREFIX}/poetry_desktop.exe\"
            RESULT_VARIABLE _poetry_deploy_result
            OUTPUT_QUIET ERROR_QUIET)
        if(NOT _poetry_deploy_result EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed with exit code \${_poetry_deploy_result}\")
        endif()
    ")
    set(CPACK_PACKAGE_FILE_NAME "poetry_desktop-${PROJECT_VERSION}-windows-x64")
endif()

set(CPACK_PACKAGE_NAME "poetry_desktop")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "PoetryDesktop")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "本地图片与诗词合成桌面壁纸")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/w4xj/poetry_desktop")
set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)

if(WIN32)
    # CPack's NSIS template redirects a non-admin install to Documents. This app
    # is deliberately per-user, so packaging/installer.nsi installs to
    # %LOCALAPPDATA%\Programs\PoetryDesktop instead and needs no elevation.
    find_program(POETRY_MAKENSIS_EXECUTABLE makensis
        HINTS "$ENV{ProgramFiles}/NSIS" "C:/Program Files (x86)/NSIS")
    if(POETRY_MAKENSIS_EXECUTABLE)
        set(POETRY_INSTALLER_STAGE "${CMAKE_CURRENT_BINARY_DIR}/installer-stage")
        set(POETRY_INSTALLER_FILE "${CMAKE_CURRENT_BINARY_DIR}/poetry_desktop-${PROJECT_VERSION}-windows-x64-setup.exe")
        file(TO_NATIVE_PATH "${POETRY_INSTALLER_STAGE}" POETRY_INSTALLER_STAGE_NATIVE)
        file(TO_NATIVE_PATH "${POETRY_INSTALLER_FILE}" POETRY_INSTALLER_FILE_NATIVE)
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.ico" POETRY_INSTALLER_ICON_NATIVE)
        add_custom_target(installer
            COMMAND "${CMAKE_COMMAND}" -E remove_directory "${POETRY_INSTALLER_STAGE}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${POETRY_INSTALLER_STAGE}"
            COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_CURRENT_BINARY_DIR}" --prefix "${POETRY_INSTALLER_STAGE}"
            COMMAND "${POETRY_MAKENSIS_EXECUTABLE}" -V2
                "-DSTAGE_DIR=${POETRY_INSTALLER_STAGE_NATIVE}"
                "-DOUT_FILE=${POETRY_INSTALLER_FILE_NATIVE}"
                "-DAPP_VERSION=${PROJECT_VERSION}"
                "-DAPP_VERSION_QUAD=${PROJECT_VERSION}.0"
                "-DICON_FILE=${POETRY_INSTALLER_ICON_NATIVE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installer.nsi"
            BYPRODUCTS "${POETRY_INSTALLER_FILE}"
            DEPENDS poetry_desktop
            COMMENT "Staging the deployment tree and building the NSIS installer"
            VERBATIM)
    else()
        message(STATUS "makensis not found; the 'installer' target is unavailable")
    endif()
endif()


