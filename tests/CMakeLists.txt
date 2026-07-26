# Standalone test project - deliberately independent of the top-level plugin CMakeLists.txt so
# the VISCA/control-logic layer can be built and tested without libobs, Qt, or Xcode.app.
cmake_minimum_required(VERSION 3.20)
project(obs-ptz-gamepad-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/visca/ViscaCamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/visca/UdpViscaTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/control/PtzController.cpp
)

add_executable(test_visca_packets test_visca_packets.cpp ${CORE_SOURCES})
target_include_directories(test_visca_packets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_executable(test_ptz_controller test_ptz_controller.cpp ${CORE_SOURCES})
target_include_directories(test_ptz_controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

enable_testing()
add_test(NAME visca_packets COMMAND test_visca_packets)
add_test(NAME ptz_controller COMMAND test_ptz_controller)
