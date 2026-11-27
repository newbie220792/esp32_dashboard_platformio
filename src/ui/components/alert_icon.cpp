#include "ui/components/alert_icon.h"

#include <stdint.h>

#include "ui/components/icon_draw.h"
#include "ui/theme.h"

// Shapes follow the 24x24 artboard icons, scaled to 0..100 units.
static void draw_cb(lv_event_t *e) {
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    const AlertLevel lv = (AlertLevel)(uintptr_t)lv_obj_get_user_data(obj);
    IconCtx c = icon_ctx(e);
    const lv_color_t ink = COL_BG;
    switch (lv) {
    case AlertLevel::Critical:
        icon_circle(c, 50, 50, 46, COL_STOPPED);
        icon_line(c, 50, 27, 50, 56, 11, ink);
        icon_circle(c, 50, 72, 6, ink);
        break;
    case AlertLevel::Warning:
        icon_triangle(c, 50, 8, 96, 88, 4, 88, COL_TEMP);
        icon_line(c, 50, 40, 50, 60, 10, ink);
        icon_circle(c, 50, 74, 6, ink);
        break;
    case AlertLevel::Info:
        icon_rrect_outline(c, 12, 12, 88, 88, 17, 9, COL_RAM);
        icon_line(c, 50, 46, 50, 67, 10, COL_RAM);
        icon_circle(c, 50, 32, 6, COL_RAM);
        break;
    }
}

lv_obj_t *alert_icon_create(lv_obj_t *parent, int32_t size) {
    lv_obj_t *o = ui_box(parent);
    lv_obj_set_size(o, size, size);
    lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    alert_icon_set(o, AlertLevel::Info);
    return o;
}

void alert_icon_set(lv_obj_t *icon, AlertLevel level) {
    lv_obj_set_user_data(icon, (void *)(uintptr_t)level);
    lv_obj_invalidate(icon);
}
