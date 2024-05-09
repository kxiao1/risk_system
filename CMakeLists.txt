cmake_minimum_required(VERSION 3.0.0)
project(risk_system VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

add_executable(risk_system test_risk_system.cpp risk_system_structs.h risk_system.h)

set(COMPILE_OPTIONS "-Wall;-std=c++20")
set(DEBUG_OPTIONS "${COMPILE_OPTIONS};-O0;-DDEBUG")
set(RELEASE_OPTIONS "${COMPILE_OPTIONS};-O3;-fsanitize=address")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:${DEBUG_OPTIONS}>")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Release>:${RELEASE_OPTIONS}>")

# target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address)
target_link_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Release>:-fsanitize=address>")

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
