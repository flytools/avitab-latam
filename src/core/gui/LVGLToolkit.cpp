/*
 *   AviTab - Aviator's Virtual Tablet
 *   Copyright (C) 2018-2026 Folke Will and Avitab Contributors
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <chrono>
#include <lvgl.h>
#include "LVGLToolkit.h"
#include "widgets/Keyboard.h"
#include "platform/Platform.h"
#include "Logger.h"

namespace avitab {

namespace {

// The look of AviTab: the LatamAcars palette (dark surfaces, blue accent), flat and rounded widgets.
const uint32_t BG = 0x0F1115, BAR = 0x0B0D11, SURFACE = 0x171A21, FIELD = 0x11141A, RAISED = 0x202533, BORDER = 0x2A303C;
const uint32_t ACCENT = 0x4C7DFF, PRIMARY = 0x22098C, TEXT = 0xEDEFF3, MUTED = 0x8A93A3;

// The theme reuses LVGL's own global styles for some widgets (lv_style_transp...): changing them would repaint every
// transparent container, so they are left alone.
bool isShared(const lv_style_t *s) {
    return s == &lv_style_scr || s == &lv_style_transp || s == &lv_style_transp_fit || s == &lv_style_transp_tight ||
           s == &lv_style_plain || s == &lv_style_plain_color || s == &lv_style_pretty || s == &lv_style_pretty_color ||
           s == &lv_style_btn_rel || s == &lv_style_btn_pr || s == &lv_style_btn_tgl_rel || s == &lv_style_btn_tgl_pr || s == &lv_style_btn_ina;
}

void flat(lv_style_t *s, uint32_t color, lv_coord_t radius) {
    if (!s || isShared(s)) return;
    s->body.main_color = lv_color_hex(color);
    s->body.grad_color = lv_color_hex(color);
    s->body.radius = radius;
    s->body.opa = LV_OPA_COVER;
    s->body.shadow.width = 0;
    s->body.border.width = 0;
}

void outline(lv_style_t *s, uint32_t color, lv_coord_t width) {
    if (!s || isShared(s)) return;
    s->body.border.color = lv_color_hex(color);
    s->body.border.width = width;
    s->body.border.part = LV_BORDER_FULL;
    s->body.border.opa = LV_OPA_COVER;
}

void ink(lv_style_t *s, uint32_t color) {
    if (s && !isShared(s)) s->text.color = lv_color_hex(color);
}

void applyModernTheme(lv_theme_t *th) {
    // backgrounds and containers
    flat(th->style.scr, BG, 0);
    flat(th->style.bg, BG, 0);
    ink(th->style.bg, TEXT);
    flat(th->style.panel, SURFACE, 12);
    outline(th->style.panel, BORDER, 1);
    flat(th->style.cont, BG, 0);

    // text
    ink(th->style.label.prim, TEXT);
    ink(th->style.label.sec, MUTED);
    ink(th->style.label.hint, MUTED);

    // buttons
    flat(th->style.btn.rel, RAISED, 10);        ink(th->style.btn.rel, TEXT);
    flat(th->style.btn.pr, ACCENT, 10);         ink(th->style.btn.pr, 0xFFFFFF);
    flat(th->style.btn.tgl_rel, PRIMARY, 10);   ink(th->style.btn.tgl_rel, 0xFFFFFF);
    flat(th->style.btn.tgl_pr, ACCENT, 10);     ink(th->style.btn.tgl_pr, 0xFFFFFF);
    flat(th->style.btn.ina, SURFACE, 10);       ink(th->style.btn.ina, MUTED);

    // windows (every app is one)
    flat(th->style.win.bg, BG, 0);
    flat(th->style.win.header, BAR, 0);
    if (!isShared(th->style.win.header)) {
        th->style.win.header->body.border.color = lv_color_hex(BORDER);
        th->style.win.header->body.border.width = 1;
        th->style.win.header->body.border.part = LV_BORDER_BOTTOM;
    }
    ink(th->style.win.header, TEXT);
    flat(th->style.win.btn.pr, ACCENT, 8);

    // sliders, bars, switches, check boxes
    flat(th->style.slider.bg, BORDER, LV_RADIUS_CIRCLE);
    flat(th->style.slider.indic, ACCENT, LV_RADIUS_CIRCLE);
    flat(th->style.slider.knob, 0xFFFFFF, LV_RADIUS_CIRCLE);
    flat(th->style.bar.bg, BORDER, LV_RADIUS_CIRCLE);
    flat(th->style.bar.indic, ACCENT, LV_RADIUS_CIRCLE);
    flat(th->style.sw.bg, BORDER, LV_RADIUS_CIRCLE);
    flat(th->style.sw.indic, ACCENT, LV_RADIUS_CIRCLE);
    flat(th->style.sw.knob_off, 0xFFFFFF, LV_RADIUS_CIRCLE);
    flat(th->style.sw.knob_on, 0xFFFFFF, LV_RADIUS_CIRCLE);
    ink(th->style.cb.bg, TEXT);
    flat(th->style.cb.box.rel, RAISED, 6);      outline(th->style.cb.box.rel, BORDER, 1);
    flat(th->style.cb.box.pr, ACCENT, 6);
    flat(th->style.cb.box.tgl_rel, ACCENT, 6);
    flat(th->style.cb.box.tgl_pr, ACCENT, 6);

    // text fields and the on-screen keyboard
    flat(th->style.ta.area, FIELD, 10);         outline(th->style.ta.area, BORDER, 1);        ink(th->style.ta.area, TEXT);
    flat(th->style.ta.oneline, FIELD, 10);      outline(th->style.ta.oneline, BORDER, 1);     ink(th->style.ta.oneline, TEXT);
    if (th->style.ta.cursor) {
        th->style.ta.cursor->body.main_color = lv_color_hex(ACCENT);
        th->style.ta.cursor->body.grad_color = lv_color_hex(ACCENT);
    }
    flat(th->style.kb.btn.rel, RAISED, 8);      ink(th->style.kb.btn.rel, TEXT);
    flat(th->style.kb.btn.pr, ACCENT, 8);       ink(th->style.kb.btn.pr, 0xFFFFFF);
    flat(th->style.kb.btn.tgl_rel, PRIMARY, 8); ink(th->style.kb.btn.tgl_rel, 0xFFFFFF);
    flat(th->style.kb.btn.tgl_pr, ACCENT, 8);   ink(th->style.kb.btn.tgl_pr, 0xFFFFFF);
    flat(th->style.kb.btn.ina, SURFACE, 8);     ink(th->style.kb.btn.ina, MUTED);

    // lists and drop-downs
    flat(th->style.list.bg, SURFACE, 12);
    flat(th->style.list.btn.rel, SURFACE, 0);   ink(th->style.list.btn.rel, TEXT);
    if (!isShared(th->style.list.btn.rel)) {
        th->style.list.btn.rel->body.border.color = lv_color_hex(BORDER);
        th->style.list.btn.rel->body.border.width = 1;
        th->style.list.btn.rel->body.border.part = LV_BORDER_BOTTOM;
    }
    flat(th->style.list.btn.pr, RAISED, 0);     ink(th->style.list.btn.pr, TEXT);
    flat(th->style.list.btn.tgl_rel, PRIMARY, 0);
    flat(th->style.list.btn.tgl_pr, ACCENT, 0);
    flat(th->style.ddlist.bg, RAISED, 10);      ink(th->style.ddlist.bg, TEXT);
    flat(th->style.ddlist.sel, ACCENT, 6);
}

lv_disp_buf_t lvDispBuf;
lv_disp_drv_t lvDriver;
lv_indev_drv_t inputDriver;
std::vector<uint32_t> tmpBuffer;
}

LVGLToolkit::LVGLToolkit(std::shared_ptr<UiDriverBase> drv):
    driver(drv)
{
    static bool lvglIsInitialized = false;
    if (!lvglIsInitialized) {
        // LVGL does not support de-initialization so we can only do this once
        lv_log_register_print_cb([] (lv_log_level_t level, const char *file, uint32_t line, const char *msg) {
            switch (level) {
                case LV_LOG_LEVEL_WARN:
                    logger::warn("GUI: %s:%d %s", file, line, msg);
                    break;
                case LV_LOG_LEVEL_ERROR:
                    logger::error("GUI: %s:%d %s", file, line, msg);
                    break;
                default:
                    logger::verbose("GUI: %s:%d %s", file, line, msg);
                    break;
            }
        });

        lv_init();

        lv_theme_t *theme = lv_theme_night_init(210, LV_FONT_DEFAULT);
        lv_theme_set_current(theme);
        applyModernTheme(theme);
        lvglIsInitialized = true;
    }

    initDisplayDriver();
    initInputDriver();

    mainScreen = std::make_shared<Screen>();
}

LVGLToolkit::~LVGLToolkit() {
    logger::verbose("~LVGLToolkit");
    driver->setWantKeyInput(false);
    mainScreen.reset();
    inputDriver.user_data = nullptr;
    lvDriver.user_data = nullptr;
}

void LVGLToolkit::doLVGLtasks() {
    lv_task_handler();
}

void LVGLToolkit::doLVGLtick(int t) {
    lv_tick_inc(std::max(t, 1));
}

void LVGLToolkit::initDisplayDriver() {
    static_assert(sizeof(lv_color_t) == sizeof(uint32_t), "Invalid lvgl color type");
    bool isUpdate = (lvDriver.buffer != nullptr);

    tmpBuffer.resize(LV_HOR_RES_MAX * LV_VER_RES_MAX);
    lv_disp_buf_init(&lvDispBuf, tmpBuffer.data(), nullptr, tmpBuffer.size());

    lv_disp_drv_init(&lvDriver);

    lvDriver.user_data = this;
    lvDriver.hor_res = UiDriverBase::INITIAL_WIDTH;
    lvDriver.ver_res = UiDriverBase::INITIAL_HEIGHT;
    lvDriver.buffer = &lvDispBuf;

    lvDriver.flush_cb = [] (lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *data) {
        LVGLToolkit *us = (LVGLToolkit *) drv->user_data;
        if (!us) {
            return;
        }

        int x1 = area->x1;
        int y1 = area->y1;
        int x2 = area->x2;
        int y2 = area->y2;

        us->driver->blit(x1, y1, x2, y2, reinterpret_cast<const uint32_t *>(data));
        lv_disp_flush_ready(drv);
        ++us->frameCounter;
    };

    if (isUpdate) {
        lv_disp_drv_update(lv_disp_get_default(), &lvDriver);
    } else {
        lv_disp_drv_register(&lvDriver);
    }
}

void LVGLToolkit::initInputDriver() {
    bool isUpdate = (inputDriver.read_cb != nullptr);

    if (!isUpdate) {
        lv_indev_drv_init(&inputDriver);
    }

    inputDriver.type = LV_INDEV_TYPE_POINTER;
    inputDriver.user_data = this;
    inputDriver.read_cb = [] (lv_indev_drv_t  *drv, lv_indev_data_t *data) -> bool {
        LVGLToolkit *us = (LVGLToolkit *) drv->user_data;
        if (!us) {
            return false;
        }

        int x, y;
        bool pressed;
        us->driver->readPointerState(x, y, pressed);
        data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        data->point.x = x;
        data->point.y = y;
        return false;
    };

    if (isUpdate) {
        lv_indev_drv_update(lv_indev_get_next(nullptr), &inputDriver);
    } else {
        lv_indev_drv_register(&inputDriver);
    }
}

void LVGLToolkit::resizeScreen(int w, int h) {
    lvDriver.hor_res = std::min(w, LV_HOR_RES_MAX);
    lvDriver.ver_res = std::min(h, LV_VER_RES_MAX);
    lv_disp_drv_update(lv_disp_get_default(), &lvDriver);
}

std::shared_ptr<Screen> &LVGLToolkit::screen() {
    return mainScreen;
}

void LVGLToolkit::handleKeyboard() {
    // check if the current widget hierarchy includes an active keyboard
    auto keyboard = searchActiveKeyboard(lv_scr_act());
    driver->setWantKeyInput(keyboard != nullptr);

    // then process keys
    uint32_t c = 0;
    while ((c = driver->popKeyPress()) != 0) {
        if (keyboard) {
            auto ta = lv_kb_get_ta(keyboard);
            auto keyb = (Keyboard *) lv_obj_get_user_data(keyboard);
            if (!ta) {
                continue;
            }

            if (c == '\b') {
                lv_ta_del_char(ta);
            } else if (c == '\n') {
                if (keyb && keyb->hasOkAction()) {
                    lv_obj_get_event_cb(keyboard)(keyboard, LV_EVENT_APPLY);
                } else {
                    lv_ta_add_char(ta, '\n');
                }
            } else {
                lv_ta_add_char(ta, lv_txt_encoded_conv_wc(c));
            }
        }
    }
}

lv_obj_t *LVGLToolkit::searchActiveKeyboard(lv_obj_t* obj) {
    if (!obj || lv_obj_get_hidden(obj)) {
        return nullptr;
    }

    lv_obj_t *screen = lv_scr_act();
    if (screen) {
        if (!lv_area_is_on(&screen->coords, &obj->coords)) {
            return nullptr;
        }
    }

    lv_obj_t *curChild = nullptr;
    while ((curChild = lv_obj_get_child(obj, curChild)) != nullptr) {
        lv_obj_t *keyb = searchActiveKeyboard(curChild);
        if (keyb) {
            return keyb;
        }

        lv_obj_type_t type{};
        lv_obj_get_type(curChild, &type);
        if (strcmp(type.type[0], "lv_kb") == 0 && !lv_obj_get_hidden(curChild)) {      // a hidden keyboard does not take the keys
            if (lv_area_is_in(&curChild->coords, &screen->coords)) {
                return curChild;
            }
        }
    }
    return nullptr;
}

} /* namespace avitab */
