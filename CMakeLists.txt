cmake_minimum_required(VERSION 3.15)
project (kpcli CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(kpcli kpcli.cpp)

add_custom_command(TARGET kpcli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/input.txt
        $<TARGET_FILE_DIR:kpcli>/input.txt
)