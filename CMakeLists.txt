cmake_minimum_required(VERSION 3.27)
project(Sunell)

set(CMAKE_CXX_STANDARD 20)

set(BOOST_ROOT "C:/Users/irahm/Documents/boost_1_83_0")

FIND_PACKAGE(Boost)
IF (Boost_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
    ADD_DEFINITIONS("-DHAS_BOOST")
ENDIF ()

add_executable(Sunell SSParser.h main.cpp)
