# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

add_custom_target(web_ui_build
    COMMAND npm install && npm run build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web-front"
    COMMENT "Hardcore build: Installing dependencies and refreshing Vite dist..."
    VERBATIM
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(imu_demo VERSION 1.0.0)
