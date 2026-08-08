cmake_minimum_required(VERSION 3.16)

# Components reside at the repo root (nvs_core/, common/).
list(APPEND EXTRA_COMPONENT_DIRS
    "../.."
    # "../common"
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

project(test_nvs_core)

# Set the patterns to extract from the binary
set(EXTRACT_PATTERNS_test_nvs_core "*/src/nvs_core.cpp" "*/src/persistence_backend.cpp")

# Set the patterns to remove from the binary
set(REMOVE_PATTERNS_test_nvs_core "*/include/*" "*host_test/*" "*build/*")

# Include common coverage logic
include("../coverage_common.cmake")
setup_gtest_coverage(test_nvs_core test_nvs_core.elf)