target_compile_definitions(${PROJECT_NAME} PRIVATE
    # hardware specific
    USE_AUDIO_I2S=1
    SDCARD_USE_CD=false
    SDCARD_CD_GPIO=21 # not used...
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=4

    # weact
    SAMPLES_PER_BUFFER=256
    SDCARD_CMD_GPIO=11
    SDCARD_D0_GPIO=12 
    AUDIO_CLK_GPIO=16 
    AUDIO_DIN_GPIO=18
    I2C_SDA_PIN=20
    I2C_SCL_PIN=21
    LED_1_GPIO=9
    LED_2_GPIO=19
    LED_3_GPIO=29
    LED_4_GPIO=24
    LED_TOP_GPIO=25
    CLOCK_INPUT_GPIO=22
    BTN_ROW_START=0
    BTN_COL_START=5
    INCLUDE_BUTTONS=1
    INCLUDE_KNOBS=1
    INCLUDE_PCA9552=1
    INCLUDE_CLOCKINPUT=1
    INCLUDE_INPUTHANDLING=1
    INCLUDE_FILTER=1
    INCLUDE_ZEPTOCORE=1
    INCLUDE_MIDI=1
    INCLUDE_CUEDSOUNDS=1
    # INCLUDE_SSD1306=1

    # ARCADE DEFINITIONS
    MCP23017_ADDR1=0x20
    MCP23017_ADDR2=0x21
    ADS7830_ADDR=0x48

    # # black
    # SDCARD_CMD_GPIO=2
    # SDCARD_D0_GPIO=3
    # AUDIO_CLK_GPIO=16 # LCK=17
    # AUDIO_DIN_GPIO=18
    # I2C_SDA_PIN=20
    # I2C_SCL_PIN=21
    # LED_1_GPIO=0
    # LED_2_GPIO=19
    # LED_3_GPIO=19
    # LED_4_GPIO=19
    # CLOCK_INPUT_GPIO=22
    # BTN_ROW_START=7
    # BTN_COL_START=12
    # INCLUDE_BUTTONS=1
    # INCLUDE_KNOBS=1
    # INCLUDE_PCA9552=1
    # INCLUDE_CLOCKINPUT=1
    # INCLUDE_INPUTHANDLING=1
    # INCLUDE_FILTER=1
    # INCLUDE_ZEPTOCORE=1


    # utilize core1 for audio to avoid dropouts
    CORE1_PROCESS_I2S_CALLBACK=1 
    DO_OVERCLOCK=1

    # debug printing
    # PRINT_AUDIO_USAGE=1
    # PRINT_AUDIO_OVERLOADS=1
    # PRINT_AUDIO_CPU_USAGE=1
    # PRINT_MEMORY_USAGE=1
    # PRINT_SDCARD_TIMING=1
    # PRINT_AUDIOBLOCKDROPS=1

    # turn off gpio for leds
    LEDS_NO_GPIO=1

    # file variations
    FILE_VARIATIONS=2

    # basics 
    # INCLUDE_KEYBOARD=1
    # INCLUDE_RGBLED=1
    INCLUDE_SINEBASS=1

    USBD_PID=0x1836

    # DETROITUNDERGROUND=1
)

# uncomment these lines to include midi
target_link_libraries(${PROJECT_NAME} 
    tinyusb_device
    tinyusb_board
)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# # uncomment these lines to have normal USB
# pico_enable_stdio_usb(${PROJECT_NAME} 1)
# pico_enable_stdio_uart(${PROJECT_NAME} 1)

