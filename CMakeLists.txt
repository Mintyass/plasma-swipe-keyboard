cmake_minimum_required(VERSION 3.21)
project(plasma-swipe-keyboard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Qml Quick VirtualKeyboard)
qt_standard_project_setup(REQUIRES 6.10)

qt_add_qml_module(plasma_swipe_keyboard
    URI Plasma.SwipeKeyboard
    VERSION 1.0
    PLUGIN_TARGET plasma_swipe_keyboard
    NO_PLUGIN_OPTIONAL
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Plasma/SwipeKeyboard"
    SOURCES
        src/swipeinputmethod.h
        src/swipeinputmethod.cpp
        src/wordmatcher.h
        src/wordmatcher.cpp
)

target_include_directories(plasma_swipe_keyboard PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")


target_link_libraries(plasma_swipe_keyboard PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::VirtualKeyboard
)

qt_add_resources(plasma_swipe_keyboard "swipe_dict"
    PREFIX "/swipe"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/data"
    FILES
        data/frequency_dictionary_en_82_765.txt
)

# Standalone matcher tests — no GUI / no VKB.
qt_add_executable(test_matcher
    test/test_matcher.cpp
    src/wordmatcher.cpp
    src/wordmatcher.h
)
target_include_directories(test_matcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(test_matcher PRIVATE Qt6::Core Qt6::Gui)
qt_add_resources(test_matcher "swipe_dict_test"
    PREFIX "/swipe"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/data"
    FILES
        data/frequency_dictionary_en_82_765.txt
)
