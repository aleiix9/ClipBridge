#include "clipbridge_ui.h"
#include <cstdio>

ClipBridgeUI *ClipBridgeUI::instance_ = nullptr;

namespace {
constexpr uint32_t COLOR_BG        = 0xF5F8FC;
constexpr uint32_t COLOR_PANEL     = 0xFFFFFF;
constexpr uint32_t COLOR_BORDER    = 0xD8E2EF;
constexpr uint32_t COLOR_TEXT      = 0x111827;
constexpr uint32_t COLOR_MUTED     = 0x667085;
constexpr uint32_t COLOR_BLUE      = 0x1769EA;
constexpr uint32_t COLOR_BLUE_DARK = 0x0E48A8;
constexpr uint32_t COLOR_BLUE_SOFT = 0xEAF2FF;
constexpr uint32_t COLOR_GREEN     = 0x0F9D6E;
constexpr uint32_t COLOR_GREEN_SOFT= 0xE8F8F2;
constexpr uint32_t COLOR_PURPLE    = 0x6B4EFF;
constexpr uint32_t COLOR_PURPLE_SOFT=0xF0EDFF;
constexpr uint32_t COLOR_ORANGE    = 0xC98200;
constexpr uint32_t COLOR_ORANGE_SOFT=0xFFF5DD;
constexpr uint32_t COLOR_RED       = 0xD64545;
constexpr uint32_t COLOR_RED_SOFT  = 0xFFF0F0;
constexpr uint32_t COLOR_SELECTED  = 0xEEF5FF;

static intptr_t eventIndex(lv_event_t *event) {
    return reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
}

static void styleRoot(lv_obj_t *root) {
    lv_obj_set_size(root, 240, 320);
    lv_obj_set_style_bg_color(root, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(root, LV_ALIGN_TOP_LEFT, 0, 0);
}

static lv_obj_t *makeTopbar(
    lv_obj_t *root,
    lv_obj_t **ssid_label,
    lv_obj_t **battery_label
) {
    lv_obj_t *topbar = lv_obj_create(root);
    lv_obj_set_size(topbar, 240, 26);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(topbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(topbar, lv_color_hex(0xE7EDF5), 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_remove_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(topbar, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *wifi_icon = lv_label_create(topbar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_12, 0);
    lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 8, 0);

    *ssid_label = lv_label_create(topbar);
    lv_label_set_text(*ssid_label, "ClipBridge");
    lv_label_set_long_mode(*ssid_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(*ssid_label, 142, 18);
    lv_obj_set_style_text_color(*ssid_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(*ssid_label, &lv_font_montserrat_12, 0);
    lv_obj_align(*ssid_label, LV_ALIGN_LEFT_MID, 27, 0);

    lv_obj_t *battery_icon = lv_label_create(topbar);
    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(battery_icon, lv_color_hex(COLOR_GREEN), 0);
    lv_obj_set_style_text_font(battery_icon, &lv_font_montserrat_12, 0);
    lv_obj_align(battery_icon, LV_ALIGN_RIGHT_MID, -42, 0);

    *battery_label = lv_label_create(topbar);
    lv_label_set_text(*battery_label, "95%");
    lv_obj_set_style_text_color(*battery_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(*battery_label, &lv_font_montserrat_12, 0);
    lv_obj_align(*battery_label, LV_ALIGN_RIGHT_MID, -7, 0);
    return topbar;
}

static void makeBottomNav(
    lv_obj_t *root,
    uint8_t active_index,
    lv_event_cb_t home_cb,
    lv_event_cb_t history_cb,
    lv_event_cb_t settings_cb
) {
    lv_obj_t *bottom = lv_obj_create(root);
    lv_obj_set_size(bottom, 240, 44);
    lv_obj_set_style_bg_color(bottom, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bottom, 1, 0);
    lv_obj_set_style_border_side(bottom, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bottom, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(bottom, 0, 0);
    lv_obj_set_style_pad_all(bottom, 0, 0);
    lv_obj_remove_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    const char *symbols[3] = {LV_SYMBOL_HOME, LV_SYMBOL_LIST, LV_SYMBOL_SETTINGS};
    const char *texts[3] = {"Home", "History", "Settings"};
    lv_event_cb_t callbacks[3] = {home_cb, history_cb, settings_cb};

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *btn = lv_button_create(bottom);
        lv_obj_set_size(btn, 80, 43);
        lv_obj_set_pos(btn, i * 80, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, callbacks[i], LV_EVENT_CLICKED, nullptr);

        const bool active = static_cast<uint8_t>(i) == active_index;
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, symbols[i]);
        lv_obj_set_style_text_color(icon, active ? lv_color_hex(COLOR_BLUE) : lv_color_hex(COLOR_MUTED), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 4);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, texts[i]);
        lv_obj_set_style_text_color(label, active ? lv_color_hex(COLOR_BLUE) : lv_color_hex(COLOR_MUTED), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -4);

        if (active) {
            lv_obj_t *indicator = lv_obj_create(btn);
            lv_obj_set_size(indicator, 50, 3);
            lv_obj_set_style_bg_color(indicator, lv_color_hex(COLOR_BLUE), 0);
            lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(indicator, 0, 0);
            lv_obj_set_style_radius(indicator, 2, 0);
            lv_obj_set_style_pad_all(indicator, 0, 0);
            lv_obj_align(indicator, LV_ALIGN_TOP_MID, 0, 0);
        }
    }
}
}

void ClipBridgeUI::begin(
    ActionCallback home_callback,
    ActionCallback settings_callback,
    ActionCallback clear_all_callback,
    ActionCallback view_all_callback,
    ActionCallback browser_prev_callback,
    ActionCallback browser_next_callback,
    ActionCallback browser_open_callback,
    ActionCallback browser_send_callback
) {
    instance_ = this;
    home_callback_ = home_callback;
    settings_callback_ = settings_callback;
    clear_all_callback_ = clear_all_callback;
    view_all_callback_ = view_all_callback;
    browser_prev_callback_ = browser_prev_callback;
    browser_next_callback_ = browser_next_callback;
    browser_open_callback_ = browser_open_callback;
    browser_send_callback_ = browser_send_callback;

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    buildHistoryScreen(screen);
    buildSelectedScreen(screen);
    buildBrowserScreen(screen);
    buildWifiScreen(screen);

    setHeaderSsid("ClipBridge");
    setBattery(0.0f, 95);
    setFilter(HistoryFilter::All);
    showSelectedItem("NONE", "No item selected", "", "", false);
}

void ClipBridgeUI::buildSelectedScreen(lv_obj_t *screen) {
    selected_root_ = lv_obj_create(screen);
    styleRoot(selected_root_);
    lv_obj_add_flag(selected_root_, LV_OBJ_FLAG_HIDDEN);

    makeTopbar(selected_root_, &selected_ssid_label_, &selected_battery_label_);

    // Brand block. Kept intentionally compact so the whole 240x320 layout
    // remains fixed and nothing can overlap the NFC cards or bottom nav.
    lv_obj_t *title = lv_label_create(selected_root_);
    lv_label_set_text(title, "ClipBridge");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 31);

    lv_obj_t *author = lv_label_create(selected_root_);
    lv_label_set_text(author, "by Aleix Ferrer. No app needed.");
    lv_obj_set_style_text_color(author, lv_color_hex(COLOR_MUTED), 0);
    lv_label_set_long_mode(author, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(author, 220, 16);
    lv_obj_set_style_text_font(author, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(author, 10, 54);

    auto makeNfcCard = [&](int y, bool copy_to_phone) {
        const uint32_t accent = copy_to_phone ? COLOR_BLUE : COLOR_GREEN;
        const uint32_t soft = copy_to_phone ? COLOR_BLUE_SOFT : COLOR_GREEN_SOFT;

        lv_obj_t *card = lv_obj_create(selected_root_);
        lv_obj_set_size(card, 220, 61);
        lv_obj_set_pos(card, 10, y);
        lv_obj_set_style_bg_color(card, lv_color_hex(soft), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(accent), 0);
        lv_obj_set_style_radius(card, 15, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *icon_circle = lv_obj_create(card);
        lv_obj_set_size(icon_circle, 40, 40);
        lv_obj_set_style_bg_color(icon_circle, lv_color_hex(accent), 0);
        lv_obj_set_style_bg_opa(icon_circle, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(icon_circle, 0, 0);
        lv_obj_set_style_radius(icon_circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(icon_circle, 0, 0);
        lv_obj_remove_flag(icon_circle, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(icon_circle, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_t *icon = lv_label_create(icon_circle);
        lv_label_set_text(icon, copy_to_phone ? LV_SYMBOL_DOWNLOAD : LV_SYMBOL_UPLOAD);
        lv_obj_set_style_text_color(icon, lv_color_white(), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
        lv_obj_center(icon);

        lv_obj_t *main = lv_label_create(card);
        lv_label_set_text(main, copy_to_phone ? "COPY TO PHONE" : "COPY INSIDE");
        lv_label_set_long_mode(main, LV_LABEL_LONG_CLIP);
        lv_obj_set_size(main, 154, 20);
        lv_obj_set_style_text_color(main, lv_color_hex(accent), 0);
        lv_obj_set_style_text_font(main, &lv_font_montserrat_16, 0);
        lv_obj_set_pos(main, 60, 7);

        lv_obj_t *hint = lv_label_create(card);
        lv_label_set_text(hint, copy_to_phone ? "Get the selected item" : "Send text, image or file");
        lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
        lv_obj_set_size(hint, 154, 18);
        lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(hint, 60, 34);
    };

    // COPY and PASTE are named from the PHONE/user point of view:
    // COPY = receive/copy from ClipBridge into the phone.
    // PASTE = COPY INSIDE: send/copy from the phone into ClipBridge.
    makeNfcCard(76, true);
    makeNfcCard(143, false);

    lv_obj_t *current_label = lv_label_create(selected_root_);
    lv_label_set_text(current_label, "CURRENT ITEM");
    lv_obj_set_style_text_color(current_label, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(current_label, &lv_font_montserrat_12, 0);
    lv_obj_align(current_label, LV_ALIGN_TOP_LEFT, 10, 211);

    lv_obj_t *card = lv_obj_create(selected_root_);
    lv_obj_set_size(card, 220, 46);
    lv_obj_set_pos(card, 10, 228);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    selected_icon_circle_ = lv_obj_create(card);
    lv_obj_set_size(selected_icon_circle_, 30, 30);
    lv_obj_set_style_bg_color(selected_icon_circle_, lv_color_hex(COLOR_BLUE_SOFT), 0);
    lv_obj_set_style_bg_opa(selected_icon_circle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(selected_icon_circle_, 0, 0);
    lv_obj_set_style_radius(selected_icon_circle_, 9, 0);
    lv_obj_set_style_pad_all(selected_icon_circle_, 0, 0);
    lv_obj_remove_flag(selected_icon_circle_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(selected_icon_circle_, LV_ALIGN_LEFT_MID, 7, 0);

    selected_icon_label_ = lv_label_create(selected_icon_circle_);
    lv_label_set_text(selected_icon_label_, "-");
    lv_obj_set_style_text_color(selected_icon_label_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(selected_icon_label_, &lv_font_montserrat_14, 0);
    lv_obj_center(selected_icon_label_);

    // Explicit fixed widths: title/status can never run underneath the type pill.
    selected_title_label_ = lv_label_create(card);
    lv_label_set_text(selected_title_label_, "No item selected");
    lv_label_set_long_mode(selected_title_label_, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(selected_title_label_, 105, 16);
    lv_obj_set_style_text_color(selected_title_label_, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(selected_title_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(selected_title_label_, 45, 5);

    selected_type_pill_ = lv_obj_create(card);
    lv_obj_set_size(selected_type_pill_, 56, 20);
    lv_obj_set_pos(selected_type_pill_, 157, 4);
    lv_obj_set_style_bg_color(selected_type_pill_, lv_color_hex(0xEEF1F5), 0);
    lv_obj_set_style_bg_opa(selected_type_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(selected_type_pill_, 1, 0);
    lv_obj_set_style_border_color(selected_type_pill_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_radius(selected_type_pill_, 9, 0);
    lv_obj_set_style_pad_all(selected_type_pill_, 0, 0);
    lv_obj_remove_flag(selected_type_pill_, LV_OBJ_FLAG_SCROLLABLE);

    selected_type_label_ = lv_label_create(selected_type_pill_);
    lv_label_set_text(selected_type_label_, "NONE");
    lv_obj_set_style_text_color(selected_type_label_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(selected_type_label_, &lv_font_montserrat_12, 0);
    lv_obj_center(selected_type_label_);

    selected_status_label_ = lv_label_create(card);
    lv_label_set_text(selected_status_label_, "Choose in History");
    lv_label_set_long_mode(selected_status_label_, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(selected_status_label_, 168, 15);
    lv_obj_set_style_text_color(selected_status_label_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(selected_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(selected_status_label_, 45, 26);

    // Retained for the existing data model; intentionally hidden on Home.
    selected_preview_label_ = lv_label_create(card);
    lv_obj_add_flag(selected_preview_label_, LV_OBJ_FLAG_HIDDEN);
    selected_details_label_ = lv_label_create(card);
    lv_obj_add_flag(selected_details_label_, LV_OBJ_FLAG_HIDDEN);

    makeBottomNav(selected_root_, 0, homeEvent, historyNavEvent, settingsEvent);
}

void ClipBridgeUI::buildHistoryScreen(lv_obj_t *screen) {
    history_root_ = lv_obj_create(screen);
    styleRoot(history_root_);

    makeTopbar(history_root_, &ssid_label_, &battery_label_);

    lv_obj_t *title = lv_label_create(history_root_);
    lv_label_set_text(title, "History");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 32);

    lv_obj_t *hint = lv_label_create(history_root_);
    lv_label_set_text(hint, "Tap item to select for COPY");
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 10, 58);

    lv_obj_t *tabs = lv_obj_create(history_root_);
    lv_obj_set_size(tabs, 220, 28);
    lv_obj_set_style_bg_color(tabs, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(tabs, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tabs, 1, 0);
    lv_obj_set_style_border_color(tabs, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(tabs, 11, 0);
    lv_obj_set_style_pad_all(tabs, 2, 0);
    lv_obj_set_style_pad_column(tabs, 2, 0);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(tabs, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(tabs, LV_ALIGN_TOP_MID, 0, 77);

    auto makeTab = [&](lv_obj_t **target, const char *text, HistoryFilter filter) {
        lv_obj_t *btn = lv_button_create(tabs);
        *target = btn;
        lv_obj_set_height(btn, 22);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(
            btn,
            filterEvent,
            LV_EVENT_CLICKED,
            reinterpret_cast<void *>(static_cast<intptr_t>(filter))
        );

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_center(label);
    };

    makeTab(&filter_all_btn_, "All", HistoryFilter::All);
    makeTab(&filter_text_btn_, "Text", HistoryFilter::Text);
    makeTab(&filter_link_btn_, "Links", HistoryFilter::Link);
    makeTab(&filter_file_btn_, "Files", HistoryFilter::File);

    lv_obj_t *list = lv_obj_create(history_root_);
    lv_obj_set_size(list, 220, 148);
    lv_obj_set_style_bg_color(list, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_border_color(list, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(list, 12, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 109);

    for (uint8_t i = 0; i < HISTORY_SLOTS; ++i) {
        lv_obj_t *row = lv_obj_create(list);
        history_row_[i] = row;
        lv_obj_set_size(row, 218, 37);
        lv_obj_set_pos(row, 0, i * 37);
        lv_obj_set_style_bg_color(row, lv_color_hex(COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(COLOR_BORDER), 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(
            row,
            historyRowEvent,
            LV_EVENT_CLICKED,
            reinterpret_cast<void *>(static_cast<intptr_t>(i))
        );

        lv_obj_t *icon_box = lv_obj_create(row);
        lv_obj_set_size(icon_box, 27, 27);
        lv_obj_set_style_bg_color(icon_box, lv_color_hex(COLOR_BLUE_SOFT), 0);
        lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(icon_box, 0, 0);
        lv_obj_set_style_radius(icon_box, 8, 0);
        lv_obj_set_style_pad_all(icon_box, 0, 0);
        lv_obj_remove_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(icon_box, LV_ALIGN_LEFT_MID, 6, 0);

        history_icon_[i] = lv_label_create(icon_box);
        lv_label_set_text(history_icon_[i], "T");
        lv_obj_set_style_text_color(history_icon_[i], lv_color_hex(COLOR_BLUE), 0);
        lv_obj_set_style_text_font(history_icon_[i], &lv_font_montserrat_14, 0);
        lv_obj_center(history_icon_[i]);

        history_title_[i] = lv_label_create(row);
        lv_label_set_text(history_title_[i], "Empty");
        lv_label_set_long_mode(history_title_[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_size(history_title_[i], 145, 17);
        lv_obj_set_style_text_color(history_title_[i], lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_text_font(history_title_[i], &lv_font_montserrat_12, 0);
        lv_obj_align(history_title_[i], LV_ALIGN_TOP_LEFT, 40, 3);

        history_meta_[i] = lv_label_create(row);
        lv_label_set_text(history_meta_[i], "");
        lv_label_set_long_mode(history_meta_[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_size(history_meta_[i], 145, 14);
        lv_obj_set_style_text_color(history_meta_[i], lv_color_hex(COLOR_MUTED), 0);
        lv_obj_set_style_text_font(history_meta_[i], &lv_font_montserrat_12, 0);
        lv_obj_align(history_meta_[i], LV_ALIGN_BOTTOM_LEFT, 40, -2);

        lv_obj_t *arrow = lv_label_create(row);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, lv_color_hex(COLOR_BLUE), 0);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -8, 0);

        history_open_btn_[i] = nullptr;
        history_send_btn_[i] = nullptr;
        clearHistoryItem(i);
    }

    lv_obj_t *view_all = lv_button_create(history_root_);
    lv_obj_set_size(view_all, 220, 17);
    lv_obj_set_style_bg_opa(view_all, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_all, 0, 0);
    lv_obj_set_style_shadow_width(view_all, 0, 0);
    lv_obj_set_style_pad_all(view_all, 0, 0);
    lv_obj_align(view_all, LV_ALIGN_TOP_MID, 0, 258);
    lv_obj_add_event_cb(view_all, viewAllEvent, LV_EVENT_CLICKED, nullptr);

    view_all_label_ = lv_label_create(view_all);
    lv_label_set_text(view_all_label_, "Browse full history  >");
    lv_obj_set_style_text_color(view_all_label_, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(view_all_label_, &lv_font_montserrat_12, 0);
    lv_obj_center(view_all_label_);

    makeBottomNav(history_root_, 1, homeEvent, historyNavEvent, settingsEvent);
}

void ClipBridgeUI::buildBrowserScreen(lv_obj_t *screen) {
    browser_root_ = lv_obj_create(screen);
    styleRoot(browser_root_);
    lv_obj_add_flag(browser_root_, LV_OBJ_FLAG_HIDDEN);

    makeTopbar(browser_root_, &browser_ssid_label_, &browser_battery_label_);

    lv_obj_t *back = lv_button_create(browser_root_);
    lv_obj_set_size(back, 30, 28);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 4, 27);
    lv_obj_add_event_cb(back, historyNavEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(browser_root_);
    lv_label_set_text(title, "All History");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 39, 31);

    browser_position_label_ = lv_label_create(browser_root_);
    lv_label_set_text(browser_position_label_, "0 / 0");
    lv_obj_set_style_text_color(browser_position_label_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(browser_position_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(browser_position_label_, LV_ALIGN_TOP_RIGHT, -10, 36);

    lv_obj_t *card = lv_obj_create(browser_root_);
    lv_obj_set_size(card, 220, 172);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(card, 17, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 61);

    browser_icon_circle_ = lv_obj_create(card);
    lv_obj_set_size(browser_icon_circle_, 40, 40);
    lv_obj_set_style_bg_color(browser_icon_circle_, lv_color_hex(COLOR_BLUE_SOFT), 0);
    lv_obj_set_style_bg_opa(browser_icon_circle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(browser_icon_circle_, 0, 0);
    lv_obj_set_style_radius(browser_icon_circle_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(browser_icon_circle_, 0, 0);
    lv_obj_remove_flag(browser_icon_circle_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(browser_icon_circle_, LV_ALIGN_TOP_LEFT, 12, 11);

    browser_icon_label_ = lv_label_create(browser_icon_circle_);
    lv_label_set_text(browser_icon_label_, LV_SYMBOL_FILE);
    lv_obj_set_style_text_color(browser_icon_label_, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(browser_icon_label_, &lv_font_montserrat_20, 0);
    lv_obj_center(browser_icon_label_);

    browser_type_pill_ = lv_obj_create(card);
    lv_obj_set_size(browser_type_pill_, 68, 24);
    lv_obj_set_style_bg_color(browser_type_pill_, lv_color_hex(COLOR_BLUE_SOFT), 0);
    lv_obj_set_style_bg_opa(browser_type_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(browser_type_pill_, 1, 0);
    lv_obj_set_style_border_color(browser_type_pill_, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_radius(browser_type_pill_, 10, 0);
    lv_obj_set_style_pad_all(browser_type_pill_, 0, 0);
    lv_obj_remove_flag(browser_type_pill_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(browser_type_pill_, LV_ALIGN_TOP_LEFT, 62, 11);

    browser_type_label_ = lv_label_create(browser_type_pill_);
    lv_label_set_text(browser_type_label_, "NONE");
    lv_obj_set_style_text_color(browser_type_label_, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(browser_type_label_, &lv_font_montserrat_12, 0);
    lv_obj_center(browser_type_label_);

    browser_title_label_ = lv_label_create(card);
    lv_label_set_text(browser_title_label_, "No history item");
    lv_label_set_long_mode(browser_title_label_, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(browser_title_label_, 196, 22);
    lv_obj_set_style_text_color(browser_title_label_, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(browser_title_label_, &lv_font_montserrat_16, 0);
    lv_obj_align(browser_title_label_, LV_ALIGN_TOP_LEFT, 12, 58);

    lv_obj_t *preview_box = lv_obj_create(card);
    lv_obj_set_size(preview_box, 196, 58);
    lv_obj_set_style_bg_color(preview_box, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(preview_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(preview_box, 1, 0);
    lv_obj_set_style_border_color(preview_box, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(preview_box, 11, 0);
    lv_obj_set_style_pad_all(preview_box, 7, 0);
    lv_obj_remove_flag(preview_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(preview_box, LV_ALIGN_TOP_MID, 0, 84);

    browser_preview_label_ = lv_label_create(preview_box);
    lv_label_set_text(browser_preview_label_, "No item available.");
    lv_label_set_long_mode(browser_preview_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(browser_preview_label_, 180, 44);
    lv_obj_set_style_text_color(browser_preview_label_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(browser_preview_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(browser_preview_label_, LV_ALIGN_TOP_LEFT, 0, 0);

    browser_details_label_ = lv_label_create(card);
    lv_label_set_text(browser_details_label_, "");
    lv_label_set_long_mode(browser_details_label_, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(browser_details_label_, 196, 16);
    lv_obj_set_style_text_color(browser_details_label_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(browser_details_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(browser_details_label_, LV_ALIGN_BOTTOM_LEFT, 12, -5);

    browser_open_btn_ = lv_button_create(browser_root_);
    lv_obj_set_size(browser_open_btn_, 65, 32);
    lv_obj_set_style_radius(browser_open_btn_, 10, 0);
    lv_obj_set_style_bg_color(browser_open_btn_, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(browser_open_btn_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(browser_open_btn_, 1, 0);
    lv_obj_set_style_border_color(browser_open_btn_, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_shadow_width(browser_open_btn_, 0, 0);
    lv_obj_set_style_pad_all(browser_open_btn_, 0, 0);
    lv_obj_align(browser_open_btn_, LV_ALIGN_TOP_LEFT, 10, 238);
    lv_obj_add_event_cb(browser_open_btn_, browserOpenEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *open_label = lv_label_create(browser_open_btn_);
    lv_label_set_text(open_label, "Open");
    lv_obj_set_style_text_color(open_label, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(open_label, &lv_font_montserrat_12, 0);
    lv_obj_center(open_label);

    lv_obj_t *prev_btn = lv_button_create(browser_root_);
    lv_obj_set_size(prev_btn, 43, 32);
    lv_obj_set_style_radius(prev_btn, 10, 0);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(prev_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(prev_btn, 1, 0);
    lv_obj_set_style_border_color(prev_btn, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_shadow_width(prev_btn, 0, 0);
    lv_obj_set_style_pad_all(prev_btn, 0, 0);
    lv_obj_align(prev_btn, LV_ALIGN_TOP_LEFT, 79, 238);
    lv_obj_add_event_cb(prev_btn, browserPrevEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(prev_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_16, 0);
    lv_obj_center(prev_label);

    lv_obj_t *next_btn = lv_button_create(browser_root_);
    lv_obj_set_size(next_btn, 43, 32);
    lv_obj_set_style_radius(next_btn, 10, 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(next_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(next_btn, 1, 0);
    lv_obj_set_style_border_color(next_btn, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_shadow_width(next_btn, 0, 0);
    lv_obj_set_style_pad_all(next_btn, 0, 0);
    lv_obj_align(next_btn, LV_ALIGN_TOP_LEFT, 126, 238);
    lv_obj_add_event_cb(next_btn, browserNextEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(next_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_16, 0);
    lv_obj_center(next_label);

    lv_obj_t *send_btn = lv_button_create(browser_root_);
    lv_obj_set_size(send_btn, 61, 32);
    lv_obj_set_style_radius(send_btn, 10, 0);
    lv_obj_set_style_bg_color(send_btn, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_bg_opa(send_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(send_btn, 0, 0);
    lv_obj_set_style_shadow_width(send_btn, 0, 0);
    lv_obj_set_style_pad_all(send_btn, 0, 0);
    lv_obj_align(send_btn, LV_ALIGN_TOP_RIGHT, -10, 238);
    lv_obj_add_event_cb(send_btn, browserSendEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "Send");
    lv_obj_set_style_text_color(send_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_12, 0);
    lv_obj_center(send_label);

    makeBottomNav(browser_root_, 1, homeEvent, historyNavEvent, settingsEvent);
}

void ClipBridgeUI::buildWifiScreen(lv_obj_t *screen) {
    wifi_root_ = lv_obj_create(screen);
    styleRoot(wifi_root_);
    lv_obj_add_flag(wifi_root_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *header = lv_obj_create(wifi_root_);
    lv_obj_set_size(header, 240, 28);
    lv_obj_set_style_bg_color(header, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *back = lv_button_create(header);
    lv_obj_set_size(back, 38, 24);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_add_event_cb(back, wifiBackEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *clear_btn = lv_button_create(header);
    lv_obj_set_size(clear_btn, 52, 24);
    lv_obj_set_style_bg_opa(clear_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clear_btn, 0, 0);
    lv_obj_set_style_shadow_width(clear_btn, 0, 0);
    lv_obj_set_style_pad_all(clear_btn, 0, 0);
    lv_obj_align(clear_btn, LV_ALIGN_RIGHT_MID, -1, 0);
    lv_obj_add_event_cb(clear_btn, clearAllRequestEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_set_style_text_color(clear_label, lv_color_hex(0xFFD9D9), 0);
    lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_12, 0);
    lv_obj_center(clear_label);

    wifi_status_label_ = lv_label_create(wifi_root_);
    lv_label_set_text(wifi_status_label_, "Choose a Wi-Fi network");
    lv_label_set_long_mode(wifi_status_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(wifi_status_label_, 220, 24);
    lv_obj_set_style_text_color(wifi_status_label_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(wifi_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(wifi_status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_status_label_, LV_ALIGN_TOP_MID, 0, 31);

    for (int i = 0; i < 3; ++i) {
        wifi_network_btn_[i] = lv_button_create(wifi_root_);
        lv_obj_set_size(wifi_network_btn_[i], 220, 24);
        lv_obj_set_style_radius(wifi_network_btn_[i], 9, 0);
        lv_obj_set_style_bg_color(wifi_network_btn_[i], lv_color_hex(COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(wifi_network_btn_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(wifi_network_btn_[i], 1, 0);
        lv_obj_set_style_border_color(wifi_network_btn_[i], lv_color_hex(COLOR_BORDER), 0);
        lv_obj_set_style_shadow_width(wifi_network_btn_[i], 0, 0);
        lv_obj_set_style_pad_hor(wifi_network_btn_[i], 8, 0);
        lv_obj_set_style_pad_ver(wifi_network_btn_[i], 0, 0);
        lv_obj_align(wifi_network_btn_[i], LV_ALIGN_TOP_MID, 0, 56 + i * 27);
        lv_obj_add_event_cb(
            wifi_network_btn_[i],
            wifiNetworkEvent,
            LV_EVENT_CLICKED,
            reinterpret_cast<void *>(static_cast<intptr_t>(i))
        );

        wifi_network_label_[i] = lv_label_create(wifi_network_btn_[i]);
        lv_label_set_text(wifi_network_label_[i], i == 0 ? "Press Scan" : "--");
        lv_label_set_long_mode(wifi_network_label_[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_width(wifi_network_label_[i], 198);
        lv_obj_set_style_text_color(wifi_network_label_[i], lv_color_hex(COLOR_TEXT), 0);
        lv_obj_set_style_text_font(wifi_network_label_[i], &lv_font_montserrat_12, 0);
        lv_obj_align(wifi_network_label_[i], LV_ALIGN_LEFT_MID, 0, 0);
    }

    wifi_selected_label_ = lv_label_create(wifi_root_);
    lv_label_set_text(wifi_selected_label_, "SSID: none");
    lv_label_set_long_mode(wifi_selected_label_, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(wifi_selected_label_, 220, 18);
    lv_obj_set_style_text_color(wifi_selected_label_, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(wifi_selected_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(wifi_selected_label_, LV_ALIGN_TOP_MID, 0, 139);

    wifi_password_area_ = lv_textarea_create(wifi_root_);
    lv_obj_set_size(wifi_password_area_, 220, 32);
    lv_obj_align(wifi_password_area_, LV_ALIGN_TOP_MID, 0, 158);
    lv_textarea_set_one_line(wifi_password_area_, true);
    lv_textarea_set_password_mode(wifi_password_area_, true);
    lv_textarea_set_max_length(wifi_password_area_, 63);
    lv_textarea_set_placeholder_text(wifi_password_area_, "Password");
    lv_textarea_set_password_show_time(wifi_password_area_, 1400);
    lv_obj_set_style_text_font(wifi_password_area_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(wifi_password_area_, 9, 0);
    lv_obj_set_style_border_color(wifi_password_area_, lv_color_hex(COLOR_BORDER), 0);

    lv_obj_t *scan_btn = lv_button_create(wifi_root_);
    lv_obj_set_size(scan_btn, 82, 28);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_LEFT, 10, 194);
    lv_obj_set_style_radius(scan_btn, 9, 0);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(scan_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scan_btn, 1, 0);
    lv_obj_set_style_border_color(scan_btn, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_shadow_width(scan_btn, 0, 0);
    lv_obj_add_event_cb(scan_btn, wifiScanEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_set_style_text_color(scan_label, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_12, 0);
    lv_obj_center(scan_label);

    lv_obj_t *connect_btn = lv_button_create(wifi_root_);
    lv_obj_set_size(connect_btn, 132, 28);
    lv_obj_align(connect_btn, LV_ALIGN_TOP_RIGHT, -10, 194);
    lv_obj_set_style_radius(connect_btn, 9, 0);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_bg_opa(connect_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(connect_btn, 0, 0);
    lv_obj_set_style_shadow_width(connect_btn, 0, 0);
    lv_obj_add_event_cb(connect_btn, wifiConnectEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Save & connect");
    lv_obj_set_style_text_color(connect_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_12, 0);
    lv_obj_center(connect_label);

    wifi_keyboard_ = lv_keyboard_create(wifi_root_);
    lv_obj_set_size(wifi_keyboard_, 240, 98);
    lv_obj_align(wifi_keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(wifi_keyboard_, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(wifi_keyboard_, wifi_password_area_);
}

void ClipBridgeUI::setWifiCallbacks(ActionCallback scan_callback, ActionCallback connect_callback) {
    wifi_scan_callback_ = scan_callback;
    wifi_connect_callback_ = connect_callback;
}

void ClipBridgeUI::setHistoryCallbacks(
    HistoryCallback select_callback,
    HistoryCallback open_callback,
    HistoryCallback send_callback
) {
    history_select_callback_ = select_callback;
    history_open_callback_ = open_callback;
    history_send_callback_ = send_callback;
}

void ClipBridgeUI::showWifiSetup() {
    if (history_root_ != nullptr) lv_obj_add_flag(history_root_, LV_OBJ_FLAG_HIDDEN);
    if (selected_root_ != nullptr) lv_obj_add_flag(selected_root_, LV_OBJ_FLAG_HIDDEN);
    if (browser_root_ != nullptr) lv_obj_add_flag(browser_root_, LV_OBJ_FLAG_HIDDEN);
    if (wifi_root_ != nullptr) {
        lv_obj_remove_flag(wifi_root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(wifi_root_, -1);
    }
}

void ClipBridgeUI::showHistory() {
    if (wifi_root_ != nullptr) lv_obj_add_flag(wifi_root_, LV_OBJ_FLAG_HIDDEN);
    if (selected_root_ != nullptr) lv_obj_add_flag(selected_root_, LV_OBJ_FLAG_HIDDEN);
    if (browser_root_ != nullptr) lv_obj_add_flag(browser_root_, LV_OBJ_FLAG_HIDDEN);
    if (history_root_ != nullptr) {
        lv_obj_remove_flag(history_root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(history_root_, -1);
    }
}

void ClipBridgeUI::showSelectedItem(
    const String &type,
    const String &title,
    const String &preview,
    const String &details,
    bool has_item
) {
    setTypeVisual(selected_icon_label_, selected_type_pill_, selected_type_label_, has_item ? type : "NONE");

    if (selected_title_label_ != nullptr) {
        lv_label_set_text(selected_title_label_, has_item ? title.c_str() : "No item selected");
    }
    if (selected_preview_label_ != nullptr) {
        lv_label_set_text(
            selected_preview_label_,
            has_item ? preview.c_str() : "Open History and select what you want to copy to the phone."
        );
    }
    if (selected_details_label_ != nullptr) {
        lv_label_set_text(selected_details_label_, has_item ? details.c_str() : "Nothing is ready to copy to the phone");
    }
    if (selected_status_label_ != nullptr) {
        lv_label_set_text(
            selected_status_label_,
            has_item ? "Ready - tap COPY zone" : "Choose in History"
        );
    }

    if (history_root_ != nullptr) lv_obj_add_flag(history_root_, LV_OBJ_FLAG_HIDDEN);
    if (wifi_root_ != nullptr) lv_obj_add_flag(wifi_root_, LV_OBJ_FLAG_HIDDEN);
    if (browser_root_ != nullptr) lv_obj_add_flag(browser_root_, LV_OBJ_FLAG_HIDDEN);
    if (selected_root_ != nullptr) {
        lv_obj_remove_flag(selected_root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(selected_root_, -1);
    }
}

void ClipBridgeUI::showHistoryBrowser(
    const String &type,
    const String &title,
    const String &preview,
    const String &details,
    uint8_t position,
    uint8_t total,
    bool can_open,
    bool has_item
) {
    setTypeVisual(browser_icon_label_, browser_type_pill_, browser_type_label_, has_item ? type : "NONE");

    if (browser_title_label_ != nullptr) {
        lv_label_set_text(browser_title_label_, has_item ? title.c_str() : "No history item");
    }
    if (browser_preview_label_ != nullptr) {
        lv_label_set_text(browser_preview_label_, has_item ? preview.c_str() : "History is empty.");
    }
    if (browser_details_label_ != nullptr) {
        lv_label_set_text(browser_details_label_, has_item ? details.c_str() : "");
    }
    if (browser_position_label_ != nullptr) {
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%u / %u", position, total);
        lv_label_set_text(browser_position_label_, buffer);
    }
    if (browser_open_btn_ != nullptr) {
        if (has_item && can_open) lv_obj_remove_flag(browser_open_btn_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(browser_open_btn_, LV_OBJ_FLAG_HIDDEN);
    }

    if (history_root_ != nullptr) lv_obj_add_flag(history_root_, LV_OBJ_FLAG_HIDDEN);
    if (wifi_root_ != nullptr) lv_obj_add_flag(wifi_root_, LV_OBJ_FLAG_HIDDEN);
    if (selected_root_ != nullptr) lv_obj_add_flag(selected_root_, LV_OBJ_FLAG_HIDDEN);
    if (browser_root_ != nullptr) {
        lv_obj_remove_flag(browser_root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(browser_root_, -1);
    }
}

void ClipBridgeUI::setWifiStatus(const String &status) {
    if (wifi_status_label_ != nullptr) {
        lv_label_set_text(wifi_status_label_, status.c_str());
    }
}

void ClipBridgeUI::setWifiScanResults(
    const String *ssids,
    const int32_t *rssi,
    size_t count
) {
    wifi_selected_index_ = -1;
    if (wifi_selected_label_ != nullptr) {
        lv_label_set_text(wifi_selected_label_, "SSID: none");
    }

    for (size_t i = 0; i < 3; ++i) {
        wifi_ssids_[i] = "";
        String shown = "--";
        if (ssids != nullptr && i < count && ssids[i].length() > 0) {
            wifi_ssids_[i] = ssids[i];
            shown = ssids[i];
            if (shown.length() > 20) shown = shown.substring(0, 20);
            if (rssi != nullptr) {
                shown += "  ";
                shown += String(rssi[i]);
                shown += "dBm";
            }
        } else if (i == 0 && count == 0) {
            shown = "No networks found";
        }

        if (wifi_network_label_[i] != nullptr) {
            lv_label_set_text(wifi_network_label_[i], shown.c_str());
        }
    }
    applyWifiSelectionStyle();
}

String ClipBridgeUI::selectedWifiSsid() const {
    if (wifi_selected_index_ < 0 || wifi_selected_index_ >= 3) return String();
    return wifi_ssids_[wifi_selected_index_];
}

String ClipBridgeUI::wifiPassword() const {
    if (wifi_password_area_ == nullptr) return String();
    return String(lv_textarea_get_text(wifi_password_area_));
}

void ClipBridgeUI::clearWifiPassword() {
    if (wifi_password_area_ != nullptr) lv_textarea_set_text(wifi_password_area_, "");
}

void ClipBridgeUI::applyWifiSelectionStyle() {
    for (int i = 0; i < 3; ++i) {
        if (wifi_network_btn_[i] == nullptr) continue;
        const bool selected = i == wifi_selected_index_;
        lv_obj_set_style_bg_color(
            wifi_network_btn_[i],
            selected ? lv_color_hex(COLOR_BLUE) : lv_color_hex(COLOR_PANEL),
            0
        );
        lv_obj_set_style_border_color(
            wifi_network_btn_[i],
            selected ? lv_color_hex(COLOR_BLUE) : lv_color_hex(COLOR_BORDER),
            0
        );
        if (wifi_network_label_[i] != nullptr) {
            lv_obj_set_style_text_color(
                wifi_network_label_[i],
                selected ? lv_color_white() : lv_color_hex(COLOR_TEXT),
                0
            );
        }
    }
}

void ClipBridgeUI::setBattery(float volts, uint8_t percent) {
    (void)volts;
    char buffer[16];
    if (percent > 100) percent = 100;
    snprintf(buffer, sizeof(buffer), "%u%%", percent);

    if (battery_label_ != nullptr) lv_label_set_text(battery_label_, buffer);
    if (selected_battery_label_ != nullptr) lv_label_set_text(selected_battery_label_, buffer);
    if (browser_battery_label_ != nullptr) lv_label_set_text(browser_battery_label_, buffer);
}

void ClipBridgeUI::setHeaderSsid(const String &ssid) {
    String shown = ssid;
    if (shown.length() > 22) shown = shown.substring(0, 22);

    if (ssid_label_ != nullptr) lv_label_set_text(ssid_label_, shown.c_str());
    if (selected_ssid_label_ != nullptr) lv_label_set_text(selected_ssid_label_, shown.c_str());
    if (browser_ssid_label_ != nullptr) lv_label_set_text(browser_ssid_label_, shown.c_str());
}

void ClipBridgeUI::setHistoryCount(uint8_t count) {
    if (view_all_label_ == nullptr) return;
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "View all history (%u)  >", count);
    lv_label_set_text(view_all_label_, buffer);
}

void ClipBridgeUI::setTypeVisual(
    lv_obj_t *icon,
    lv_obj_t *pill,
    lv_obj_t *pill_label,
    const String &type
) {
    uint32_t accent = COLOR_BLUE;
    uint32_t soft = COLOR_BLUE_SOFT;
    const char *symbol = LV_SYMBOL_FILE;

    if (type == "TEXT") {
        accent = COLOR_BLUE;
        soft = COLOR_BLUE_SOFT;
        symbol = "T";
    } else if (type == "LINK" || type == "URL") {
        accent = COLOR_PURPLE;
        soft = COLOR_PURPLE_SOFT;
        symbol = "L";
    } else if (type == "IMAGE") {
        accent = COLOR_GREEN;
        soft = COLOR_GREEN_SOFT;
        symbol = LV_SYMBOL_IMAGE;
    } else if (type == "PDF") {
        accent = COLOR_ORANGE;
        soft = COLOR_ORANGE_SOFT;
        symbol = "PDF";
    } else if (type == "FILE") {
        accent = COLOR_ORANGE;
        soft = COLOR_ORANGE_SOFT;
        symbol = LV_SYMBOL_FILE;
    } else {
        accent = COLOR_MUTED;
        soft = 0xEEF1F5;
        symbol = "-";
    }

    if (icon != nullptr) {
        lv_label_set_text(icon, symbol);
        lv_obj_set_style_text_color(icon, lv_color_hex(accent), 0);
    }
    if (pill != nullptr) {
        lv_obj_set_style_bg_color(pill, lv_color_hex(soft), 0);
        lv_obj_set_style_border_color(pill, lv_color_hex(accent), 0);
    }
    if (pill_label != nullptr) {
        lv_label_set_text(pill_label, type.c_str());
        lv_obj_set_style_text_color(pill_label, lv_color_hex(accent), 0);
    }
}

void ClipBridgeUI::setHistoryItem(
    uint8_t index,
    const String &type,
    const String &title,
    const String &meta,
    bool selected
) {
    if (index >= HISTORY_SLOTS) return;

    history_valid_[index] = true;
    history_type_[index] = type;
    if (selected) selected_history_index_ = static_cast<int8_t>(index);

    String shown_title = title;
    shown_title.replace("\r", " ");
    shown_title.replace("\n", " ");
    if (shown_title.length() > 25) {
        shown_title = shown_title.substring(0, 22);
        shown_title += "...";
    }

    String shown_meta = meta;
    if (shown_meta.length() > 20) shown_meta = shown_meta.substring(0, 20);

    lv_label_set_text(history_title_[index], shown_title.c_str());
    lv_label_set_text(history_meta_[index], shown_meta.c_str());

    if (type == "TEXT") {
        lv_label_set_text(history_icon_[index], "T");
        lv_obj_set_style_text_color(history_icon_[index], lv_color_hex(COLOR_BLUE), 0);
    } else if (type == "LINK" || type == "URL") {
        lv_label_set_text(history_icon_[index], "L");
        lv_obj_set_style_text_color(history_icon_[index], lv_color_hex(COLOR_PURPLE), 0);
    } else if (type == "IMAGE") {
        lv_label_set_text(history_icon_[index], LV_SYMBOL_IMAGE);
        lv_obj_set_style_text_color(history_icon_[index], lv_color_hex(COLOR_GREEN), 0);
    } else {
        lv_label_set_text(history_icon_[index], type == "PDF" ? "P" : "F");
        lv_obj_set_style_text_color(history_icon_[index], lv_color_hex(COLOR_ORANGE), 0);
    }

    applyHistoryVisibility();
    applyHistorySelectionStyle();
}

void ClipBridgeUI::clearHistoryItem(uint8_t index) {
    if (index >= HISTORY_SLOTS) return;
    history_valid_[index] = false;
    history_type_[index] = "";
    lv_label_set_text(history_title_[index], "No item");
    lv_label_set_text(history_meta_[index], "Waiting for content");
    lv_obj_add_flag(history_row_[index], LV_OBJ_FLAG_HIDDEN);
}

void ClipBridgeUI::setSelectedHistory(uint8_t index) {
    if (index >= HISTORY_SLOTS || !history_valid_[index]) return;
    selected_history_index_ = static_cast<int8_t>(index);
    applyHistorySelectionStyle();
}

void ClipBridgeUI::applyHistoryVisibility() {
    uint8_t visible_index = 0;
    for (uint8_t i = 0; i < HISTORY_SLOTS; ++i) {
        if (history_row_[i] == nullptr) continue;

        bool visible = history_valid_[i];
        if (visible) {
            switch (filter_) {
                case HistoryFilter::Text:
                    visible = history_type_[i] == "TEXT";
                    break;
                case HistoryFilter::Link:
                    visible = history_type_[i] == "LINK" || history_type_[i] == "URL";
                    break;
                case HistoryFilter::File:
                    visible = history_type_[i] == "FILE" || history_type_[i] == "PDF" || history_type_[i] == "IMAGE";
                    break;
                case HistoryFilter::All:
                default:
                    break;
            }
        }

        if (visible) {
            lv_obj_set_y(history_row_[i], visible_index * 37);
            lv_obj_remove_flag(history_row_[i], LV_OBJ_FLAG_HIDDEN);
            ++visible_index;
        } else {
            lv_obj_add_flag(history_row_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ClipBridgeUI::applyHistorySelectionStyle() {
    for (uint8_t i = 0; i < HISTORY_SLOTS; ++i) {
        if (history_row_[i] == nullptr) continue;
        const bool selected = static_cast<int8_t>(i) == selected_history_index_;
        lv_obj_set_style_bg_color(
            history_row_[i],
            selected ? lv_color_hex(COLOR_SELECTED) : lv_color_hex(COLOR_PANEL),
            0
        );
        lv_obj_set_style_border_color(
            history_row_[i],
            selected ? lv_color_hex(COLOR_BLUE) : lv_color_hex(COLOR_BORDER),
            0
        );
    }
}

void ClipBridgeUI::applyFilterStyle() {
    auto styleTab = [&](lv_obj_t *button, bool active) {
        if (button == nullptr) return;
        lv_obj_set_style_bg_color(
            button,
            active ? lv_color_hex(COLOR_BLUE) : lv_color_hex(COLOR_PANEL),
            0
        );
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(button, active ? 0 : 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(COLOR_BORDER), 0);
        lv_obj_t *label = lv_obj_get_child(button, 0);
        if (label != nullptr) {
            lv_obj_set_style_text_color(label, active ? lv_color_white() : lv_color_hex(COLOR_TEXT), 0);
        }
    };

    styleTab(filter_all_btn_, filter_ == HistoryFilter::All);
    styleTab(filter_text_btn_, filter_ == HistoryFilter::Text);
    styleTab(filter_link_btn_, filter_ == HistoryFilter::Link);
    styleTab(filter_file_btn_, filter_ == HistoryFilter::File);
}

void ClipBridgeUI::setFilter(HistoryFilter filter) {
    filter_ = filter;
    applyFilterStyle();
    applyHistoryVisibility();
}

void ClipBridgeUI::showTextPreview(const String &title, const String &text) {
    if (preview_overlay_ != nullptr) {
        lv_obj_delete(preview_overlay_);
        preview_overlay_ = nullptr;
    }

    preview_overlay_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(preview_overlay_, 240, 320);
    lv_obj_set_style_bg_color(preview_overlay_, lv_color_hex(0x101828), 0);
    lv_obj_set_style_bg_opa(preview_overlay_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(preview_overlay_, 0, 0);
    lv_obj_set_style_pad_all(preview_overlay_, 0, 0);
    lv_obj_remove_flag(preview_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(preview_overlay_);
    lv_obj_set_size(card, 218, 276);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(card);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title.c_str());
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(title_label, 170, 24);
    lv_obj_set_style_text_color(title_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *close = lv_button_create(card);
    lv_obj_set_size(close, 30, 26);
    lv_obj_set_style_radius(close, 8, 0);
    lv_obj_set_style_bg_color(close, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_shadow_width(close, 0, 0);
    lv_obj_set_style_pad_all(close, 0, 0);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, -2);
    lv_obj_add_event_cb(close, previewCloseEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *close_label = lv_label_create(close);
    lv_label_set_text(close_label, "X");
    lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
    lv_obj_center(close_label);

    lv_obj_t *body = lv_obj_create(card);
    lv_obj_set_size(body, 196, 222);
    lv_obj_set_style_bg_color(body, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(body, 1, 0);
    lv_obj_set_style_border_color(body, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(body, 10, 0);
    lv_obj_set_style_pad_all(body, 8, 0);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *text_label = lv_label_create(body);
    lv_label_set_text(text_label, text.c_str());
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(text_label, 176);
    lv_obj_set_style_text_color(text_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_12, 0);
    lv_obj_align(text_label, LV_ALIGN_TOP_LEFT, 0, 0);
}

void ClipBridgeUI::showToast(const String &message) {
    if (toast_ != nullptr) {
        lv_obj_delete(toast_);
        toast_ = nullptr;
    }

    toast_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(toast_, 214, 48);
    lv_obj_set_style_radius(toast_, 13, 0);
    lv_obj_set_style_bg_color(toast_, lv_color_hex(0x102347), 0);
    lv_obj_set_style_bg_opa(toast_, LV_OPA_90, 0);
    lv_obj_set_style_border_width(toast_, 0, 0);
    lv_obj_set_style_pad_all(toast_, 5, 0);
    lv_obj_align(toast_, LV_ALIGN_BOTTOM_MID, 0, -48);

    lv_obj_t *label = lv_label_create(toast_);
    lv_label_set_text(label, message.c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(label, 198, 38);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);

    lv_timer_t *timer = lv_timer_create(toastDeleteTimer, 2300, toast_);
    lv_timer_set_repeat_count(timer, 1);
}

void ClipBridgeUI::toastDeleteTimer(lv_timer_t *timer) {
    lv_obj_t *toast = static_cast<lv_obj_t *>(lv_timer_get_user_data(timer));
    if (toast != nullptr && lv_obj_is_valid(toast)) lv_obj_delete(toast);
    if (instance_ != nullptr && instance_->toast_ == toast) instance_->toast_ = nullptr;
}

void ClipBridgeUI::showClearAllDialog() {
    if (clear_all_overlay_ != nullptr) return;

    clear_all_overlay_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(clear_all_overlay_, 240, 320);
    lv_obj_set_style_bg_color(clear_all_overlay_, lv_color_hex(0x101828), 0);
    lv_obj_set_style_bg_opa(clear_all_overlay_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(clear_all_overlay_, 0, 0);
    lv_obj_set_style_pad_all(clear_all_overlay_, 0, 0);
    lv_obj_remove_flag(clear_all_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(clear_all_overlay_);
    lv_obj_set_size(card, 214, 154);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(card);

    lv_obj_t *icon = lv_label_create(card);
    lv_label_set_text(icon, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_color(icon, lv_color_hex(COLOR_RED), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "Clear all ClipBridge data?");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 30, 1);

    lv_obj_t *body = lv_label_create(card);
    lv_label_set_text(body, "Deletes history and uploaded files.\nSaved Wi-Fi is kept.");
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(body, 188, 42);
    lv_obj_set_style_text_color(body, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_12, 0);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 39);

    lv_obj_t *cancel = lv_button_create(card);
    lv_obj_set_size(cancel, 88, 34);
    lv_obj_set_style_radius(cancel, 10, 0);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cancel, 1, 0);
    lv_obj_set_style_border_color(cancel, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_shadow_width(cancel, 0, 0);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(cancel, clearAllCancelEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *cancel_label = lv_label_create(cancel);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_color(cancel_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_12, 0);
    lv_obj_center(cancel_label);

    lv_obj_t *confirm = lv_button_create(card);
    lv_obj_set_size(confirm, 96, 34);
    lv_obj_set_style_radius(confirm, 10, 0);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(COLOR_RED), 0);
    lv_obj_set_style_bg_opa(confirm, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(confirm, 0, 0);
    lv_obj_set_style_shadow_width(confirm, 0, 0);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(confirm, clearAllConfirmEvent, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *confirm_label = lv_label_create(confirm);
    lv_label_set_text(confirm_label, "Delete all");
    lv_obj_set_style_text_color(confirm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(confirm_label, &lv_font_montserrat_12, 0);
    lv_obj_center(confirm_label);
}

void ClipBridgeUI::hideClearAllDialog() {
    if (clear_all_overlay_ != nullptr) {
        lv_obj_delete(clear_all_overlay_);
        clear_all_overlay_ = nullptr;
    }
}

void ClipBridgeUI::homeEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->home_callback_ != nullptr) instance_->home_callback_();
}

void ClipBridgeUI::historyNavEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr) instance_->showHistory();
}

void ClipBridgeUI::settingsEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->settings_callback_ != nullptr) instance_->settings_callback_();
}

void ClipBridgeUI::viewAllEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->view_all_callback_ != nullptr) instance_->view_all_callback_();
}

void ClipBridgeUI::filterEvent(lv_event_t *event) {
    if (instance_ == nullptr) return;
    const intptr_t raw = eventIndex(event);
    if (raw < 0 || raw > static_cast<intptr_t>(HistoryFilter::File)) return;
    instance_->setFilter(static_cast<HistoryFilter>(raw));
}

void ClipBridgeUI::historyRowEvent(lv_event_t *event) {
    if (instance_ == nullptr) return;
    const intptr_t raw = eventIndex(event);
    if (raw < 0 || raw >= HISTORY_SLOTS || !instance_->history_valid_[raw]) return;
    instance_->setSelectedHistory(static_cast<uint8_t>(raw));
    if (instance_->history_select_callback_ != nullptr) {
        instance_->history_select_callback_(static_cast<uint8_t>(raw));
    }
}

void ClipBridgeUI::historyOpenEvent(lv_event_t *event) {
    if (instance_ == nullptr) return;
    const intptr_t raw = eventIndex(event);
    if (raw < 0 || raw >= HISTORY_SLOTS || !instance_->history_valid_[raw]) return;
    if (instance_->history_open_callback_ != nullptr) {
        instance_->history_open_callback_(static_cast<uint8_t>(raw));
    }
}

void ClipBridgeUI::historySendEvent(lv_event_t *event) {
    if (instance_ == nullptr) return;
    const intptr_t raw = eventIndex(event);
    if (raw < 0 || raw >= HISTORY_SLOTS || !instance_->history_valid_[raw]) return;
    instance_->setSelectedHistory(static_cast<uint8_t>(raw));
    if (instance_->history_send_callback_ != nullptr) {
        instance_->history_send_callback_(static_cast<uint8_t>(raw));
    }
}

void ClipBridgeUI::browserPrevEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->browser_prev_callback_ != nullptr) instance_->browser_prev_callback_();
}

void ClipBridgeUI::browserNextEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->browser_next_callback_ != nullptr) instance_->browser_next_callback_();
}

void ClipBridgeUI::browserOpenEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->browser_open_callback_ != nullptr) instance_->browser_open_callback_();
}

void ClipBridgeUI::browserSendEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->browser_send_callback_ != nullptr) instance_->browser_send_callback_();
}

void ClipBridgeUI::previewCloseEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->preview_overlay_ != nullptr) {
        lv_obj_delete(instance_->preview_overlay_);
        instance_->preview_overlay_ = nullptr;
    }
}

void ClipBridgeUI::wifiBackEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr) instance_->showHistory();
}

void ClipBridgeUI::wifiScanEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr && instance_->wifi_scan_callback_ != nullptr) instance_->wifi_scan_callback_();
}

void ClipBridgeUI::wifiConnectEvent(lv_event_t *event) {
    (void)event;
    if (instance_ == nullptr) return;
    if (instance_->selectedWifiSsid().length() == 0) {
        instance_->showToast("Select a Wi-Fi network first.");
        return;
    }
    if (instance_->wifi_connect_callback_ != nullptr) instance_->wifi_connect_callback_();
}

void ClipBridgeUI::wifiNetworkEvent(lv_event_t *event) {
    if (instance_ == nullptr) return;
    const intptr_t raw = eventIndex(event);
    if (raw < 0 || raw >= 3 || instance_->wifi_ssids_[raw].length() == 0) return;

    instance_->wifi_selected_index_ = static_cast<int8_t>(raw);
    instance_->applyWifiSelectionStyle();

    if (instance_->wifi_selected_label_ != nullptr) {
        String text = "SSID: ";
        text += instance_->wifi_ssids_[raw];
        if (text.length() > 31) text = text.substring(0, 31);
        lv_label_set_text(instance_->wifi_selected_label_, text.c_str());
    }
}

void ClipBridgeUI::clearAllRequestEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr) instance_->showClearAllDialog();
}

void ClipBridgeUI::clearAllCancelEvent(lv_event_t *event) {
    (void)event;
    if (instance_ != nullptr) instance_->hideClearAllDialog();
}

void ClipBridgeUI::clearAllConfirmEvent(lv_event_t *event) {
    (void)event;
    if (instance_ == nullptr) return;
    instance_->hideClearAllDialog();
    if (instance_->clear_all_callback_ != nullptr) instance_->clear_all_callback_();
}
