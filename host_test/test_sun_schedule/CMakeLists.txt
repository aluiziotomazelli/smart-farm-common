cmake_minimum_required(VERSION 3.16)

# Components reside at the repo root (nvs_core/, common/).
list(APPEND EXTRA_COMPONENT_DIRS
    "../.."
    "../gtest"
    "../../../idf_hals"
    "$ENV{IDF_PATH}/tools/mocks/driver"
)

set(COMPONENTS 
    "main"
    "smart-farm-common"
    "idf_hals"
)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_sun_schedule)

# Set the patterns to extract from the binary
set(EXTRACT_PATTERNS_test_sun_schedule "*/src/sun_schedule.cpp")

# Set the patterns to remove from the binary
set(REMOVE_PATTERNS_test_sun_schedule "*/include/*" "*host_test/*" "*build/*")

# Include common coverage logic
include("../coverage_common.cmake")
setup_gtest_coverage(test_sun_schedule test_sun_schedule.elf)