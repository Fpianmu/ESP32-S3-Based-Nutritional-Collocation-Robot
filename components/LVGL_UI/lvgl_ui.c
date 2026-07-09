#include "lvgl_ui.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "lcd.h"
#include "touch.h"

static const char *TAG = "LVGL_UI";

void lvgl_ui_init(void)
{
    // ---- 1. 初始化LVGL核心 ----
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));
    ESP_LOGI(TAG, "LVGL port 初始化完成");

    // ---- 2. 注册LCD显示 ----
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle   = bsp_lcd_get_io_handle(),
        .panel_handle = bsp_lcd_get_panel_handle(),
        .buffer_size = LCD_HOR_RES * 50,   // 240*50 = 12K pixels
        .double_buffer = true,
        .hres        = LCD_HOR_RES,
        .vres        = LCD_VER_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .swap_bytes  = 1,
            .buff_dma    = 1,
            .buff_spiram = 0,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "LVGL 显示注册失败");
        return;
    }

    lv_display_set_default(disp);
    ESP_LOGI(TAG, "LVGL 显示注册成功 (%dx%d)", LCD_HOR_RES, LCD_VER_RES);

    // ---- 3. 注册触摸输入 ----
    if (ft6336u_touch_handle) {
        lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = ft6336u_touch_handle,
            .scale = {
                .x = 1.0f,
                .y = 1.0f,
            },
        };
        lv_indev_t *indev = lvgl_port_add_touch(&touch_cfg);
        if (indev) {
            ESP_LOGI(TAG, "LVGL 触摸注册成功");
        } else {
            ESP_LOGW(TAG, "LVGL 触摸注册失败");
        }
    } else {
        ESP_LOGW(TAG, "触摸未初始化，跳过触摸注册");
    }
}

// ===================== 触摸测试界面 =====================

static lv_obj_t *coord_label;
static lv_obj_t *status_label;

static void test_screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) {
        lv_indev_t *indev = lv_event_get_indev(e);
        if (!indev) return;

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        if (code == LV_EVENT_PRESSED) {
            lv_label_set_text_fmt(status_label, "状态: 按下");
        } else if (code == LV_EVENT_PRESSING) {
            lv_label_set_text_fmt(status_label, "状态: 拖动");
        } else {
            lv_label_set_text_fmt(status_label, "状态: 释放");
        }

        lv_label_set_text_fmt(coord_label, "X: %4ld  Y: %4ld", point.x, point.y);
    }
}

void lvgl_ui_test(void)
{
    lvgl_port_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // ---- 标题 ----
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "FT6336U 触摸测试");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // ---- 坐标显示 ----
    coord_label = lv_label_create(scr);
    lv_label_set_text(coord_label, "X:    0  Y:    0");
    lv_obj_set_style_text_color(coord_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(coord_label, &lv_font_montserrat_14, 0);
    lv_obj_align(coord_label, LV_ALIGN_CENTER, 0, -20);

    // ---- 状态显示 ----
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "状态: 等待触摸");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 20);

    // ---- 圆点指示器 ----
    lv_obj_t *dot = lv_obj_create(scr);
    lv_obj_set_size(dot, 16, 16);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ---- 透明触摸层（覆盖整个屏幕）----
    lv_obj_t *touch_area = lv_obj_create(scr);
    lv_obj_set_size(touch_area, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(touch_area, LV_OPA_0, 0);
    lv_obj_set_style_border_width(touch_area, 0, 0);
    lv_obj_remove_flag(touch_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(touch_area, test_screen_event_cb,
                        LV_EVENT_PRESSED | LV_EVENT_PRESSING | LV_EVENT_RELEASED, NULL);

    ESP_LOGI(TAG, "触摸测试界面创建完成");
    lvgl_port_unlock();
}
