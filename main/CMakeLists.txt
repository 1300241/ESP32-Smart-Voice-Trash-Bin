# main 组件 CMakeLists.txt
idf_component_register(
    SRCS
        "main.c"
        "drivers/pca9685.c"
        "drivers/pcf8574.c"
        "drivers/hc_sr04.c"
        "drivers/ssd1306.c"
        "drivers/button.c"
        "services/bin_manager.c"
        "services/event_bus.c"
        "comm/voice_uart.c"
        "comm/bluetooth_spp.c"
        "ui/display.c"
    INCLUDE_DIRS
        "."
        "drivers"
        "services"
        "comm"
        "ui"
    REQUIRES
        driver
        freertos
        esp_timer
        nvs_flash
        bt
        u8g2
)

# 让 menuconfig 菜单生效
if(CONFIG_SMART_TRASH_BIN_USE_KCONFIG)
    set(kconfig_input "${CMAKE_CURRENT_LIST_DIR}/Kconfig.projbuild")
endif()
