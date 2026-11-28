#include "ui/components/icon_draw.h"
#include "ui/theme.h"

static int32_t X(const IconCtx &c, int32_t u) { return c.x0 + u * c.s / 100; }
static int32_t Y(const IconCtx &c, int32_t u) { return c.y0 + u * c.s / 100; }

IconCtx icon_ctx(lv_event_t *e) {
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    IconCtx c;
    c.layer = lv_event_get_layer(e);
    c.x0 = a.x1;
    c.y0 = a.y1;
    c.s = LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a));
    c.bg = COL_BG;
    for (lv_obj_t *p = lv_obj_get_parent(obj); p; p = lv_obj_get_parent(p)) {
        if (lv_obj_get_style_bg_opa(p, LV_PART_MAIN) >= LV_OPA_50) {
            c.bg = lv_obj_get_style_bg_color(p, LV_PART_MAIN);
            break;
        }
    }
    return c;
}

void icon_circle(const IconCtx &c, int32_t cx, int32_t cy, int32_t r, lv_color_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col;
    d.bg_opa = LV_OPA_COVER;
    d.radius = LV_RADIUS_CIRCLE;
    int32_t rp = r * c.s / 100;
    lv_area_t a = {X(c, cx) - rp, Y(c, cy) - rp, X(c, cx) + rp, Y(c, cy) + rp};
    lv_draw_rect(c.layer, &d, &a);
}

void icon_rrect(const IconCtx &c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t radius,
                lv_color_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col;
    d.bg_opa = LV_OPA_COVER;
    d.radius = radius < 0 ? LV_RADIUS_CIRCLE : radius * c.s / 100;
    lv_area_t a = {X(c, x1), Y(c, y1), X(c, x2), Y(c, y2)};
    lv_draw_rect(c.layer, &d, &a);
}

void icon_triangle(const IconCtx &c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3,
                   lv_color_t col) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.bg_color = col;
    d.bg_opa = LV_OPA_COVER;
    d.p[0].x = X(c, x1);
    d.p[0].y = Y(c, y1);
    d.p[1].x = X(c, x2);
    d.p[1].y = Y(c, y2);
    d.p[2].x = X(c, x3);
    d.p[2].y = Y(c, y3);
    lv_draw_triangle(c.layer, &d);
}

void icon_rrect_outline(const IconCtx &c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t radius,
                        int32_t w, lv_color_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = col;
    d.border_opa = LV_OPA_COVER;
    d.border_width = LV_MAX(1, w * c.s / 100);
    d.radius = radius * c.s / 100;
    lv_area_t a = {X(c, x1), Y(c, y1), X(c, x2), Y(c, y2)};
    lv_draw_rect(c.layer, &d, &a);
}

void icon_line(const IconCtx &c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t w, lv_color_t col) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = col;
    d.width = LV_MAX(1, w * c.s / 100);
    d.round_start = 1;
    d.round_end = 1;
    d.p1.x = X(c, x1);
    d.p1.y = Y(c, y1);
    d.p2.x = X(c, x2);
    d.p2.y = Y(c, y2);
    lv_draw_line(c.layer, &d);
}

void icon_arc(const IconCtx &c, int32_t cx, int32_t cy, int32_t r, int32_t w, int32_t start, int32_t end,
              lv_color_t col) {
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.color = col;
    d.width = LV_MAX(1, w * c.s / 100);
    d.center.x = X(c, cx);
    d.center.y = Y(c, cy);
    d.radius = (uint16_t)(r * c.s / 100);
    d.start_angle = start;
    d.end_angle = end;
    d.rounded = 1;
    lv_draw_arc(c.layer, &d);
}
