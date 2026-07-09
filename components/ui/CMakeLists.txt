# components/ui/CMakeLists.txt
idf_component_register(
    SRCS "ui.c" "screens.c" "images.c" "styles.c"
         "ui_font_my_character_14.c"
         "ui_font_my_character_16.c"
         "ui_font_my_character_18.c"
         "ui_font_my_character_24.c"
         "ui_font_my_character_32.c"   
         "vars.cpp" "eez-flow.cpp" "action.cpp"   # ... 等所有源文件
    INCLUDE_DIRS "."
    REQUIRES lvgl function ctl_mutex
)

set_source_files_properties(screens.c images.c styles.c PROPERTIES LANGUAGE CXX)

# 在 CMakeLists.txt 末尾添加
set_source_files_properties(images.c PROPERTIES COMPILE_FLAGS "-Wno-missing-field-initializers")