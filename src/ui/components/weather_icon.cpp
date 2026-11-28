#include "ui/components/weather_icon.h"
#include "ui/components/icon_draw.h"
#include "ui/theme.h"

enum IconKind : uint8_t { IC_SUN, IC_MOON, IC_PARTLY_DAY, IC_PARTLY_NIGHT, IC_CLOUD, IC_FOG, IC_RAIN, IC_SNOW, IC_STORM };

#define C_SUN   lv_color_hex(0xF5A524)
#define C_MOON  lv_color_hex(0xDCE3EA)
#define C_CLOUD lv_color_hex(0xC9D3DE)
#define C_RAIN  lv_color_hex(0x6CB6FF)

static IconKind kind_for(int code, bool day) {
    switch (code) {
    case 0:  return day ? IC_SUN : IC_MOON;
    case 1:
    case 2:  return day ? IC_PARTLY_DAY : IC_PARTLY_NIGHT;
    case 3:  return IC_CLOUD;
    case 45:
    case 48: return IC_FOG;
    case 71: case 73: case 75: case 77: case 85: case 86: return IC_SNOW;
    case 95: case 96: case 99: return IC_STORM;
    default:
        if (code >= 51 && code <= 82) return IC_RAIN;
        return IC_CLOUD;
    }
}

const char *weather_code_text(int code) {
    switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mostly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: case 73: case 75: case 77: return "Snow";
    case 80: return "Showers";
    case 81: return "Rain showers";
    case 82: return "Heavy showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Storm, hail";
    default: return "--";
    }
}

static void sun(const IconCtx &c, int32_t cx, int32_t cy, int32_t r) {
    // 8 rays then disc
    static const int8_t dir[8][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}, {7, -7}, {7, 7}, {-7, 7}, {-7, -7}};
    for (auto &d : dir) {
        int32_t dx = d[0], dy = d[1];
        int32_t len = (dx && dy) ? 10 : 1;   // diagonals use 7/10 ~ 1/sqrt2
        int32_t r1 = r + r / 3, r2 = r + r * 2 / 3;
        icon_line(c, cx + dx * r1 / len, cy + dy * r1 / len, cx + dx * r2 / len, cy + dy * r2 / len,
             LV_MAX(4, r / 4), C_SUN);
    }
    icon_circle(c, cx, cy, r, C_SUN);
}

static void moon(const IconCtx &c, int32_t cx, int32_t cy, int32_t r) {
    icon_circle(c, cx, cy, r, C_MOON);
    icon_circle(c, cx + r * 5 / 10, cy - r * 4 / 10, r * 8 / 10, c.bg);
}

// Cloud centred around (cx, cy), roughly 70 x 40 units at scale 100.
static void cloud(const IconCtx &c, int32_t cx, int32_t cy, int32_t scale, lv_color_t col) {
    auto S = [&](int32_t v) { return v * scale / 100; };
    icon_circle(c, cx - S(14), cy + S(2), S(14), col);
    icon_circle(c, cx + S(6), cy - S(6), S(19), col);
    icon_circle(c, cx + S(22), cy + S(4), S(12), col);
    icon_rrect(c, cx - S(28), cy + S(2), cx + S(34), cy + S(16), -1, col);
}

static void draw_cb(lv_event_t *e) {
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    IconCtx c = icon_ctx(e);

    switch ((IconKind)(uintptr_t)lv_obj_get_user_data(obj)) {
    case IC_SUN:          sun(c, 50, 50, 22); break;
    case IC_MOON:         moon(c, 50, 50, 30); break;
    case IC_PARTLY_DAY:   sun(c, 36, 36, 16); cloud(c, 52, 58, 95, C_CLOUD); break;
    case IC_PARTLY_NIGHT: moon(c, 38, 36, 20); cloud(c, 52, 58, 95, C_CLOUD); break;
    case IC_CLOUD:        cloud(c, 48, 48, 115, C_CLOUD); break;
    case IC_FOG:
        cloud(c, 48, 36, 95, C_CLOUD);
        icon_line(c, 18, 70, 82, 70, 7, COL_MUTED);
        icon_line(c, 26, 84, 74, 84, 7, COL_MUTED);
        break;
    case IC_RAIN:
        cloud(c, 48, 36, 100, C_CLOUD);
        for (int i = 0; i < 3; i++) icon_line(c, 34 + i * 16, 66, 28 + i * 16, 86, 6, C_RAIN);
        break;
    case IC_SNOW:
        cloud(c, 48, 36, 100, C_CLOUD);
        for (int i = 0; i < 3; i++) icon_circle(c, 32 + i * 18, 76 + (i % 2) * 8, 5, COL_TEXT);
        break;
    case IC_STORM:
        cloud(c, 48, 34, 100, C_CLOUD);
        icon_line(c, 54, 56, 42, 74, 8, C_SUN);
        icon_line(c, 42, 74, 56, 74, 8, C_SUN);
        icon_line(c, 56, 74, 44, 92, 8, C_SUN);
        break;
    }
}

lv_obj_t *weather_icon_create(lv_obj_t *parent, int32_t size) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(o, size, size);
    lv_obj_set_user_data(o, (void *)(uintptr_t)IC_CLOUD);
    lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

void weather_icon_set(lv_obj_t *icon, int code, bool is_day) {
    lv_obj_set_user_data(icon, (void *)(uintptr_t)kind_for(code, is_day));
    lv_obj_invalidate(icon);
}
