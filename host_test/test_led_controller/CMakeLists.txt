cmake_minimum_required(VERSION 3.16)

# Components reside at the repo root and external
list(APPEND EXTRA_COMPONENT_DIRS
    "../.."
    "../gtest"
    "../../external/idf_hals"
    "$ENV{IDF_PATH}/tools/mocks/driver"
)

set(COMPONENTS 
    "main"
    "smart-farm-common"
    "idf_hals"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_led_controller)

# Set the patterns to extract from the binary
set(EXTRACT_PATTERNS_test_led_controller "*/src/led_controller.cpp")

# Set the patterns to remove from the binary
set(REMOVE_PATTERNS_test_led_controller "*/include/*" "*host_test/*" "*build/*")

# Include common coverage logic
include("../coverage_common.cmake")
setup_gtest_coverage(test_led_controller test_led_controller.elf)
