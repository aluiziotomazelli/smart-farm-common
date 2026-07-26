idf_component_register(
    SRCS
        "nvs_core.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        nvs_flash
        common
        idf_hals
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()