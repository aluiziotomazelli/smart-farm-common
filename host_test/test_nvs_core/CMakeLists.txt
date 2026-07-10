cmake_minimum_required(VERSION 3.16)

# Components reside at the repo root (nvs_core/, common/).
list(APPEND EXTRA_COMPONENT_DIRS
    "../../nvs_core"
    "../../common"
    "../common"
    "../gtest"
    "$ENV{IDF_PATH}/tools/mocks/driver"
)

set(COMPONENTS 
    "main"
    "nvs_core"
)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_nvs_core)

# Set the patterns to extract from the binary
set(EXTRACT_PATTERNS_test_nvs_core "*/components/nvs_core/*")

# Set the patterns to remove from the binary
set(REMOVE_PATTERNS_test_nvs_core "*/components/nvs_core/include/*")

# Include common coverage logic
include("../coverage_common.cmake")
setup_gtest_coverage(test_nvs_core test_nvs_core.elf)