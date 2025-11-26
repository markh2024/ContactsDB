cmake_minimum_required(VERSION 3.15)
project(ContactsApp VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Find GTK4
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

# Find MariaDB Connector/C++
find_path(MARIADB_INCLUDE_DIR mariadb/conncpp.hpp
    PATHS
    /usr/include
    /usr/local/include
    /opt/homebrew/include
)

find_library(MARIADB_LIBRARY
    NAMES mariadbcpp mariadb++
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/homebrew/lib
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
)

if(NOT MARIADB_INCLUDE_DIR OR NOT MARIADB_LIBRARY)
    message(FATAL_ERROR "MariaDB Connector/C++ not found. Please install it first.")
endif()

message(STATUS "MariaDB include dir: ${MARIADB_INCLUDE_DIR}")
message(STATUS "MariaDB library: ${MARIADB_LIBRARY}")

# Source files
set(SOURCES
    src/main.cpp
    src/DB.cpp
    src/MainWindow.cpp
    src/ContactDialogs.cpp
    src/DBConnectionDialog.cpp
)

set(HEADERS
    include/DB.hpp
    include/MainWindow.hpp
    include/ContactDialogs.hpp
    include/DBConnectionDialog.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MARIADB_INCLUDE_DIR}
)

# Use pkg-config to get GTKmm include directories
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${GTKMM_INCLUDE_DIRS}
)

# Link directories
target_link_directories(${PROJECT_NAME} PRIVATE
    ${GTKMM_LIBRARY_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${GTKMM_LIBRARIES}
    ${MARIADB_LIBRARY}
)

# Install targets
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "GTKmm include dirs: ${GTKMM_INCLUDE_DIRS}")
message(STATUS "GTKmm libraries: ${GTKMM_LIBRARIES}")
