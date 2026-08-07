# CMake generated Testfile for 
# Source directory: E:/cProject/poetry_desktop
# Build directory: E:/cProject/poetry_desktop/cmake-build-release
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("poetry_core_tests" "E:/cProject/poetry_desktop/cmake-build-release/poetry_tests.exe")
set_tests_properties("poetry_core_tests" PROPERTIES  ENVIRONMENT "PATH=E:/codeSoft/QT/6.8.3/mingw_64/bin;QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=E:/codeSoft/QT/6.8.3/mingw_64/plugins" WORKING_DIRECTORY "E:/cProject/poetry_desktop/cmake-build-release" _BACKTRACE_TRIPLES "E:/cProject/poetry_desktop/CMakeLists.txt;63;add_test;E:/cProject/poetry_desktop/CMakeLists.txt;0;")
add_test("poetry_ui_tests" "E:/cProject/poetry_desktop/cmake-build-release/poetry_ui_tests.exe")
set_tests_properties("poetry_ui_tests" PROPERTIES  ENVIRONMENT "PATH=E:/codeSoft/QT/6.8.3/mingw_64/bin;QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=E:/codeSoft/QT/6.8.3/mingw_64/plugins" WORKING_DIRECTORY "E:/cProject/poetry_desktop/cmake-build-release" _BACKTRACE_TRIPLES "E:/cProject/poetry_desktop/CMakeLists.txt;77;add_test;E:/cProject/poetry_desktop/CMakeLists.txt;0;")
