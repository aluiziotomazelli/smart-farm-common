idf_component_register(
    SRCS 
        "main.cpp"
        "test_sun_schedule.cpp"
    INCLUDE_DIRS 
        "."
        "../../common"
    REQUIRES 
        gtest
        smart-farm-common
        idf_hals
        WHOLE_ARCHIVE
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()