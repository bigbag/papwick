cmake_minimum_required(VERSION 3.16)
project(papwick-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

get_filename_component(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/common
  ${PROJECT_ROOT}/lib/FitnessCore/src
  ${PROJECT_ROOT}/lib/Game2048Core/src
  ${PROJECT_ROOT}/lib/SystemCore/src
  ${PROJECT_ROOT}/lib/LightsOutCore/src
  ${PROJECT_ROOT}/lib/SokobanCore/src
  ${PROJECT_ROOT}/lib/PomodoroCore/src
  ${PROJECT_ROOT}/lib/Connect4Core/src
  ${PROJECT_ROOT}/lib/I18n/src
)

file(GLOB FITNESS_CORE_SRC ${PROJECT_ROOT}/lib/FitnessCore/src/*.cpp)
file(GLOB SYSTEM_CORE_SRC ${PROJECT_ROOT}/lib/SystemCore/src/*.cpp)
add_executable(FitnessCoreTest unit/FitnessCoreTest.cpp ${FITNESS_CORE_SRC} ${SYSTEM_CORE_SRC})
target_compile_options(FitnessCoreTest PRIVATE -Wall -Wextra -Werror)

file(GLOB GAME2048_CORE_SRC ${PROJECT_ROOT}/lib/Game2048Core/src/*.cpp)
add_executable(Game2048Test unit/Game2048Test.cpp ${GAME2048_CORE_SRC})
target_compile_options(Game2048Test PRIVATE -Wall -Wextra -Werror)

add_executable(SystemCoreTest unit/SystemCoreTest.cpp ${SYSTEM_CORE_SRC})
target_compile_options(SystemCoreTest PRIVATE -Wall -Wextra -Werror)

foreach(name LightsOut Sokoban Connect4)
  file(GLOB ${name}_SRC ${PROJECT_ROOT}/lib/${name}Core/src/*.cpp)
  add_executable(${name}Test unit/${name}Test.cpp ${${name}_SRC})
  target_compile_options(${name}Test PRIVATE -Wall -Wextra -Werror)
endforeach()

add_executable(I18nTest unit/I18nTest.cpp ${PROJECT_ROOT}/lib/I18n/src/I18n.cpp)
target_compile_options(I18nTest PRIVATE -Wall -Wextra -Werror)

add_executable(PomodoroTest unit/PomodoroTest.cpp)
target_compile_options(PomodoroTest PRIVATE -Wall -Wextra -Werror)
