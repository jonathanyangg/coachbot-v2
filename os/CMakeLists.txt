cmake_minimum_required(VERSION 3.16)
project(cocos2d CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ON refuses setVel: the Feather UART path is not implemented.
# Default OFF. Motors use the Pi GPIO path (libgpiod, or in-memory when the chip is absent).
option(COACHBOT_MOTORS_ON_ESP32
  "Refuse motor commands (ESP32 UART is an unconfirmed stub)" OFF)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(GPIOD REQUIRED libgpiod)
# msgpack-cxx 6 is header-only (/usr/include/msgpack.hpp). No pkg-config file.
# libgpiod 2.x (Debian 13 / Raspbian 13) replaced the 1.x line functions.
if(GPIOD_VERSION VERSION_GREATER_EQUAL "2.0")
  set(COCOS_GPIOD_V2 1)
  message(STATUS "libgpiod ${GPIOD_VERSION}: using API v2")
else()
  set(COCOS_GPIOD_V2 0)
  message(STATUS "libgpiod ${GPIOD_VERSION}: using API v1")
endif()

add_library(cocos2d_core STATIC
  config.cpp
  services/motion.cpp
  services/led.cpp
  drivers/pi_gpio/pi_gpio.cpp
  drivers/pi_gpio/pi_gpio_motor.cpp
  drivers/esp32/esp32_driver.cpp
  drivers/vive/vive_driver.cpp
  drivers/camera/camera_driver.cpp
  drivers/charger/charger_driver.cpp
  api/dispatcher.cpp
  api/api_server.cpp)
target_include_directories(cocos2d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${ZMQ_INCLUDE_DIRS}
                           ${GPIOD_INCLUDE_DIRS})
target_compile_options(cocos2d_core PRIVATE -Wall -Wextra)
if(COACHBOT_MOTORS_ON_ESP32)
  target_compile_definitions(cocos2d_core PUBLIC MOTORS_ON_ESP32=1)
endif()
if(COCOS_GPIOD_V2)
  target_compile_definitions(cocos2d_core PRIVATE COCOS_GPIOD_V2=1)
endif()
target_link_libraries(cocos2d_core PUBLIC ${ZMQ_LIBRARIES} ${GPIOD_LIBRARIES} Threads::Threads)

add_executable(cocos2d main.cpp)
target_link_libraries(cocos2d PRIVATE cocos2d_core)
target_compile_options(cocos2d PRIVATE -Wall -Wextra)

add_executable(cocos2d_logic_test tests/logic_test.cpp)
target_compile_definitions(cocos2d_logic_test PRIVATE
  EXAMPLE_CONF="${CMAKE_CURRENT_SOURCE_DIR}/coachbot.example.conf")
target_link_libraries(cocos2d_logic_test PRIVATE cocos2d_core)
target_compile_options(cocos2d_logic_test PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME logic COMMAND cocos2d_logic_test)
add_test(NAME api COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_api.py)
set_tests_properties(api PROPERTIES
  ENVIRONMENT "COCOS2D=$<TARGET_FILE:cocos2d>;PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../python")
