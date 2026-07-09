#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_dough_sub(lv_event_t * e);
extern void action_dough_add(lv_event_t * e);
extern void action_set_200g(lv_event_t * e);
extern void action_set_300g(lv_event_t * e);
extern void action_set_500g(lv_event_t * e);
extern void action_set_1000g(lv_event_t * e);
extern void action_star_mixer(lv_event_t * e);
extern void action_stop(lv_event_t * e);
extern void action_step_out(lv_event_t * e);
extern void action_step_back(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/