# xiaozhi 语音控制组件 — 基于 xiaozhi-esp32 音频管线
set(AUDIO_SRCS
    "audio/audio_codec.cc"
    "audio/audio_service.cc"
    "audio/codecs/no_audio_codec.cc"
    "audio/codecs/dummy_audio_codec.cc"
    "audio/demuxer/ogg_demuxer.cc"
    "audio/wake_words/afe_wake_word.cc"
    "audio/processors/afe_audio_processor.cc"
    "audio/processors/no_audio_processor.cc"
    "audio/processors/audio_debugger.cc"
)

idf_component_register(
    SRCS
        "xiaozhi.cc"
        "settings.cc"
        "system_info.cc"
        "wss_client.c"
        ${AUDIO_SRCS}
    INCLUDE_DIRS
        "."
        "include"
        "audio"
        "audio/codecs"
        "audio/wake_words"
        "audio/processors"
        "audio/demuxer"
    REQUIRES
        freertos
        nvs_flash
        esp_http_client
        esp_netif
        esp_wifi
        esp_timer
        esp_driver_i2s
        esp-tls
        lwip
        json
        esp-sr
        esp_audio_codec
        esp_audio_effects
        esp_codec_dev
        esp-dsp
        mbedtls
        ctl_mutex
        function
)

# 兼容 ESP-IDF v6.0 的编译选项
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-macro-redefined
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BOARD_TYPE=\"custom-duplex\"
    BOARD_NAME=\"custom-duplex\"
)
