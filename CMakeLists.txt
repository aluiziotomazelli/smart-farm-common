idf_component_register(
    SRCS
        "src/button_ota_trigger.cpp"
        "src/espnow_ota_trigger.cpp"
        "src/nvs_core.cpp"
        "src/persistence_backend.cpp"
    INCLUDE_DIRS
        "include"
        "include/interfaces"
    REQUIRES
        driver
        esp_timer
        nvs_flash
        idf_hals
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()
