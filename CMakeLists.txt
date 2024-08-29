cmake_minimum_required(VERSION 3.10.2)

if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
endif()
if(POLICY CMP0024)
    cmake_policy(SET CMP0024 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0024 NEW)
endif()

project(cobridge LANGUAGES CXX VERSION 0.0.1)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(AMENT_LINT_FILTERS "-build/include_order")

macro(enable_strict_compiler_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /WX /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wold-style-cast -Wfloat-equal -Wmost -Wunused-exception-parameter)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wold-style-cast -Wfloat-equal)
    endif()
endmacro()

find_package(nlohmann_json QUIET)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(websocketpp REQUIRED)
find_package(ZLIB REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(USE_ASIO_STANDALONE "Build with standalone ASIO" ON)
if(USE_ASIO_STANDALONE)
    message(STATUS "Using standalone ASIO")
    add_definitions(-DASIO_STANDALONE)
else()
    message(STATUS "Using Boost ASIO")
    find_package(Boost REQUIRED)
endif()

include(TestBigEndian)
test_big_endian(ENDIAN)
if(ENDIAN)
    add_compile_definitions(ARCH_IS_BIG_ENDIAN=1)
endif()

find_program(GIT_SCM git DOC "Git version control")
if(GIT_SCM)
    execute_process(
            COMMAND ${GIT_SCM} describe --always --dirty --exclude="*"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE COBRIDGE_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
set(COBRIDGE_VERSION "${CMAKE_PROJECT_VERSION}")
configure_file(cobridge_base/src/version.cpp.in
        cobridge_base/src/version.cpp @ONLY)

add_library(cobridge_base SHARED
        cobridge_base/src/base64.cpp
        cobridge_base/src/cobridge.cpp
        cobridge_base/src/parameter.cpp
        cobridge_base/src/serialization.cpp
        cobridge_base/src/server_factory.cpp
#        cobridge_base/src/test/test_client.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/cobridge_base/src/version.cpp
)
target_include_directories(cobridge_base
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cobridge_base/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(cobridge_base
        OpenSSL::Crypto
        OpenSSL::SSL
        ZLIB::ZLIB
        ${CMAKE_THREAD_LIBS_INIT}
)
if(nlohmann_json_FOUND)
    target_link_libraries(cobridge_base nlohmann_json::nlohmann_json)
else()
    message(STATUS "nlohmann_json not found, will search at compile time")
endif()
enable_strict_compiler_warnings(cobridge_base)

message(STATUS "ROS_VERSION: " $ENV{ROS_VERSION})
message(STATUS "ROS_DISTRO: " $ENV{ROS_DISTRO})
message(STATUS "ROS_ROOT: " $ENV{ROS_ROOT})
if("$ENV{ROS_VERSION}" STREQUAL "1")
    if(CATKIN_DEVEL_PREFIX OR catkin_FOUND OR CATKIN_BUILD_BINARY_PACKAGE)
        message(STATUS "Building with catkin")
        set(ROS_BUILD_TYPE "catkin")

        find_package(catkin REQUIRED COMPONENTS nodelet resource_retriever ros_babel_fish rosgraph_msgs roslib roscpp)
        find_package(Boost REQUIRED)

        catkin_package(
                INCLUDE_DIRS cobridge_base/include
                LIBRARIES cobridge_base cobridge_nodelet
                CATKIN_DEPENDS nodelet resource_retriever ros_babel_fish rosgraph_msgs roslib roscpp
                DEPENDS Boost
        )

        add_library(cobridge_nodelet
                ros1_bridge/src/ros1_bridge_nodelet.cpp
                ros1_bridge/src/param_utils.cpp
                ros1_bridge/src/service_utils.cpp
        )

        target_include_directories(cobridge_nodelet
                SYSTEM PRIVATE
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cobridge_base/include>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ros1_bridge/include>
                $<INSTALL_INTERFACE:include>
                ${catkin_INCLUDE_DIRS}
        )
        target_link_libraries(cobridge_nodelet cobridge_base ${catkin_LIBRARIES})
        enable_strict_compiler_warnings(cobridge_nodelet)

        add_executable(cobridge ros1_bridge/src/ros1_bridge_node.cpp)
        target_include_directories(cobridge SYSTEM PRIVATE ${catkin_INCLUDE_DIRS})
        target_link_libraries(cobridge ${catkin_LIBRARIES})
        enable_strict_compiler_warnings(cobridge)
    else()
        message(FATAL_ERROR "Could not find catkin")
    endif()
elseif("$ENV{ROS_VERSION}" STREQUAL "2")
    if("$ENV{ROS_DISTRO}" STREQUAL "foxy")
        add_compile_definitions(ROS2_VERSION_FOXY)
    else()
        add_compile_definitions(ROS2_VERSION_HUMBLE)
    endif()

    if(DEFINED ENV{AMENT_PREFIX_PATH})
        message(STATUS "Building with ament_cmake")
        set(ROS_BUILD_TYPE "ament_cmake")

        find_package(ament_cmake REQUIRED)
        find_package(rosgraph_msgs REQUIRED)
        find_package(rclcpp REQUIRED)
        find_package(rclcpp_components REQUIRED)
        find_package(resource_retriever REQUIRED)
        find_package(cv_bridge REQUIRED)
        find_package(OpenCV REQUIRED)

#        find_package(ament_lint_auto REQUIRED)
#        ament_lint_auto_find_test_dependencies()

        add_library(cobridge_component SHARED
                ros2_bridge/src/message_definition_cache.cpp
                ros2_bridge/src/param_utils.cpp
                ros2_bridge/src/ros2_bridge.cpp
                ros2_bridge/src/parameter_interface.cpp
                ros2_bridge/src/generic_client.cpp
                ros2_bridge/src/generic_subscription.cpp
                ros2_bridge/src/generic_publisher.cpp
                ros2_bridge/src/typesupport_helpers.cpp
        )
        target_include_directories(cobridge_component
                PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cobridge_base/include>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ros2_bridge/include>
                $<INSTALL_INTERFACE:include>
        )

        #ament_target_dependencies(cobridge_component rclcpp rclcpp_components resource_retriever rosgraph_msgs sensor_msgs cv_bridge)
        ament_target_dependencies(cobridge_component rclcpp rclcpp_components resource_retriever rosgraph_msgs)
        target_link_libraries(cobridge_component cobridge_base)
        rclcpp_components_register_nodes(cobridge_component "cobridge::CoBridge")
        enable_strict_compiler_warnings(cobridge_component)
        add_executable(cobridge
                ros2_bridge/src/ros2_bridge_node.cpp
        )
        target_include_directories(cobridge SYSTEM PRIVATE ${rclcpp_INCLUDE_DIRS})
        ament_target_dependencies(cobridge rclcpp rclcpp_components)
        enable_strict_compiler_warnings(cobridge)
    else()
        message(FATAL_ERROR "Could not find ament_cmake")
    endif()
else()
    message(FATAL_ERROR "ROS_VERSION environment variable must be 1 or 2")
endif()

if(ROS_BUILD_TYPE STREQUAL "catkin")
    if(CATKIN_ENABLE_TESTING)
        message(STATUS "Building tests with catkin")

        find_package(catkin REQUIRED COMPONENTS roscpp std_msgs std_srvs)
        if(NOT "$ENV{ROS_DISTRO}" STREQUAL "melodic")
            find_package(GTest REQUIRED)
        endif()
        find_package(rostest REQUIRED)
        find_package(Boost REQUIRED COMPONENTS system)

        catkin_add_gtest(version_test cobridge_base/tests/version_test.cpp)
        target_link_libraries(version_test cobridge_base ${Boost_LIBRARIES})
        enable_strict_compiler_warnings(version_test)
#
#        catkin_add_gtest(serialization_test foxglove_bridge_base/tests/serialization_test.cpp)
#        target_link_libraries(serialization_test foxglove_bridge_base ${Boost_LIBRARIES})
#        enable_strict_compiler_warnings(foxglove_bridge)
#
#        catkin_add_gtest(base64_test foxglove_bridge_base/tests/base64_test.cpp)
#        target_link_libraries(base64_test foxglove_bridge_base ${Boost_LIBRARIES})
#        enable_strict_compiler_warnings(foxglove_bridge)
#
#        add_rostest_gtest(smoke_test ros1_foxglove_bridge/tests/smoke.test ros1_foxglove_bridge/tests/smoke_test.cpp)
#        target_include_directories(smoke_test SYSTEM PRIVATE
#                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/foxglove_bridge_base/include>
#                ${catkin_INCLUDE_DIRS}
#                $<INSTALL_INTERFACE:include>
#        )
#        target_link_libraries(smoke_test foxglove_bridge_base ${catkin_LIBRARIES})
#        enable_strict_compiler_warnings(smoke_test)
    endif()
elseif(ROS_BUILD_TYPE STREQUAL "ament_cmake")
    if(BUILD_TESTING)
        message(STATUS "Building tests with ament_cmake")

        find_package(ament_cmake_gtest REQUIRED)
        find_package(ament_lint_auto REQUIRED)
        ament_lint_auto_find_test_dependencies()

        ament_add_gtest(version_test cobridge_base/tests/version_test.cpp)
        target_link_libraries(version_test cobridge_base)
        enable_strict_compiler_warnings(version_test)

    endif()
endif()


if(ROS_BUILD_TYPE STREQUAL "catkin")
    install(TARGETS cobridge
            RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
    )
    install(TARGETS cobridge_base cobridge_nodelet
            ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
            LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
            RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
    )
    install(FILES nodelets.xml
            DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
    )
    install(DIRECTORY ros1_bridge/launch/
            DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
    )
elseif(ROS_BUILD_TYPE STREQUAL "ament_cmake")
    install(FILES ros2_bridge/include/ros2_bridge.hpp
            DESTINATION include/${PROJECT_NAME}/
    )
    install(TARGETS cobridge
            DESTINATION lib/${PROJECT_NAME}
    )
    install(TARGETS cobridge_base cobridge_component
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
    )
    install(DIRECTORY ros2_bridge/launch
            DESTINATION share/${PROJECT_NAME}/
    )
    install(FILES ros2_bridge/include/utils.hpp
            DESTINATION include/${PROJECT_NAME}/
    )
    ament_export_libraries(cobridge_base cobridge_component)
    ament_package()
endif()
