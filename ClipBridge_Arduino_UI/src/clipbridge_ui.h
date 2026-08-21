#pragma once

#include <Arduino.h>
#include "lvgl/lvgl.h"

class ClipBridgeUI {
public:
    using ActionCallback = void (*)();
    using HistoryCallback = void (*)(uint8_t index);

    static constexpr uint8_t HISTORY_SLOTS = 4;

    enum class HistoryFilter : uint8_t {
        All = 0,
        Text,
        Link,
        File
    };

    void begin(
        ActionCallback home_callback,
        ActionCallback settings_callback,
        ActionCallback clear_all_callback,
        ActionCallback view_all_callback,
        ActionCallback browser_prev_callback,
        ActionCallback browser_next_callback,
        ActionCallback browser_open_callback,
        ActionCallback browser_send_callback
    );

    void setWifiCallbacks(
        ActionCallback scan_callback,
        ActionCallback connect_callback
    );

    void setHistoryCallbacks(
        HistoryCallback select_callback,
        HistoryCallback open_callback,
        HistoryCallback send_callback
    );

    void showWifiSetup();
    void showHistory();
    void showSelectedItem(
        const String &type,
        const String &title,
        const String &preview,
        const String &details,
        bool has_item
    );
    void showHistoryBrowser(
        const String &type,
        const String &title,
        const String &preview,
        const String &details,
        uint8_t position,
        uint8_t total,
        bool can_open,
        bool has_item
    );

    void setWifiStatus(const String &status);
    void setWifiScanResults(
        const String *ssids,
        const int32_t *rssi,
        size_t count
    );
    String selectedWifiSsid() const;
    String wifiPassword() const;
    void clearWifiPassword();

    void setBattery(float volts, uint8_t percent);
    void setHeaderSsid(const String &ssid);
    void setHistoryCount(uint8_t count);
    void setHistoryItem(
        uint8_t index,
        const String &type,
        const String &title,
        const String &meta,
        bool selected
    );
    void clearHistoryItem(uint8_t index);
    void setSelectedHistory(uint8_t index);
    void setFilter(HistoryFilter filter);
    void showTextPreview(const String &type_title, const String &text);
    void showToast(const String &message);

private:
    static void settingsEvent(lv_event_t *event);
    static void homeEvent(lv_event_t *event);
    static void historyNavEvent(lv_event_t *event);
    static void viewAllEvent(lv_event_t *event);
    static void filterEvent(lv_event_t *event);
    static void historyRowEvent(lv_event_t *event);
    static void historyOpenEvent(lv_event_t *event);
    static void historySendEvent(lv_event_t *event);
    static void previewCloseEvent(lv_event_t *event);
    static void toastDeleteTimer(lv_timer_t *timer);

    static void browserPrevEvent(lv_event_t *event);
    static void browserNextEvent(lv_event_t *event);
    static void browserOpenEvent(lv_event_t *event);
    static void browserSendEvent(lv_event_t *event);

    static void wifiBackEvent(lv_event_t *event);
    static void wifiScanEvent(lv_event_t *event);
    static void wifiConnectEvent(lv_event_t *event);
    static void wifiNetworkEvent(lv_event_t *event);
    static void clearAllRequestEvent(lv_event_t *event);
    static void clearAllCancelEvent(lv_event_t *event);
    static void clearAllConfirmEvent(lv_event_t *event);

    void buildHistoryScreen(lv_obj_t *screen);
    void buildSelectedScreen(lv_obj_t *screen);
    void buildBrowserScreen(lv_obj_t *screen);
    void buildWifiScreen(lv_obj_t *screen);
    void showClearAllDialog();
    void hideClearAllDialog();
    void applyFilterStyle();
    void applyHistoryVisibility();
    void applyHistorySelectionStyle();
    void applyWifiSelectionStyle();
    void setTypeVisual(lv_obj_t *icon, lv_obj_t *pill, lv_obj_t *pill_label, const String &type);

    static ClipBridgeUI *instance_;

    ActionCallback home_callback_ = nullptr;
    ActionCallback settings_callback_ = nullptr;
    ActionCallback clear_all_callback_ = nullptr;
    ActionCallback view_all_callback_ = nullptr;
    ActionCallback browser_prev_callback_ = nullptr;
    ActionCallback browser_next_callback_ = nullptr;
    ActionCallback browser_open_callback_ = nullptr;
    ActionCallback browser_send_callback_ = nullptr;
    ActionCallback wifi_scan_callback_ = nullptr;
    ActionCallback wifi_connect_callback_ = nullptr;
    HistoryCallback history_select_callback_ = nullptr;
    HistoryCallback history_open_callback_ = nullptr;
    HistoryCallback history_send_callback_ = nullptr;

    String wifi_ssids_[3];
    int8_t wifi_selected_index_ = -1;

    lv_obj_t *history_root_ = nullptr;
    lv_obj_t *selected_root_ = nullptr;
    lv_obj_t *browser_root_ = nullptr;
    lv_obj_t *wifi_root_ = nullptr;

    lv_obj_t *ssid_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t *selected_ssid_label_ = nullptr;
    lv_obj_t *selected_battery_label_ = nullptr;
    lv_obj_t *browser_ssid_label_ = nullptr;
    lv_obj_t *browser_battery_label_ = nullptr;

    lv_obj_t *selected_icon_circle_ = nullptr;
    lv_obj_t *selected_icon_label_ = nullptr;
    lv_obj_t *selected_type_pill_ = nullptr;
    lv_obj_t *selected_type_label_ = nullptr;
    lv_obj_t *selected_title_label_ = nullptr;
    lv_obj_t *selected_preview_label_ = nullptr;
    lv_obj_t *selected_details_label_ = nullptr;
    lv_obj_t *selected_status_label_ = nullptr;

    lv_obj_t *browser_icon_circle_ = nullptr;
    lv_obj_t *browser_icon_label_ = nullptr;
    lv_obj_t *browser_type_pill_ = nullptr;
    lv_obj_t *browser_type_label_ = nullptr;
    lv_obj_t *browser_title_label_ = nullptr;
    lv_obj_t *browser_preview_label_ = nullptr;
    lv_obj_t *browser_details_label_ = nullptr;
    lv_obj_t *browser_position_label_ = nullptr;
    lv_obj_t *browser_open_btn_ = nullptr;

    lv_obj_t *toast_ = nullptr;
    lv_obj_t *preview_overlay_ = nullptr;
    lv_obj_t *clear_all_overlay_ = nullptr;

    lv_obj_t *history_row_[HISTORY_SLOTS] = {nullptr};
    lv_obj_t *history_icon_[HISTORY_SLOTS] = {nullptr};
    lv_obj_t *history_title_[HISTORY_SLOTS] = {nullptr};
    lv_obj_t *history_meta_[HISTORY_SLOTS] = {nullptr};
    lv_obj_t *history_open_btn_[HISTORY_SLOTS] = {nullptr};
    lv_obj_t *history_send_btn_[HISTORY_SLOTS] = {nullptr};
    String history_type_[HISTORY_SLOTS];
    bool history_valid_[HISTORY_SLOTS] = {false};
    int8_t selected_history_index_ = -1;
    lv_obj_t *view_all_label_ = nullptr;

    lv_obj_t *filter_all_btn_ = nullptr;
    lv_obj_t *filter_text_btn_ = nullptr;
    lv_obj_t *filter_link_btn_ = nullptr;
    lv_obj_t *filter_file_btn_ = nullptr;

    lv_obj_t *wifi_status_label_ = nullptr;
    lv_obj_t *wifi_network_btn_[3] = {nullptr, nullptr, nullptr};
    lv_obj_t *wifi_network_label_[3] = {nullptr, nullptr, nullptr};
    lv_obj_t *wifi_selected_label_ = nullptr;
    lv_obj_t *wifi_password_area_ = nullptr;
    lv_obj_t *wifi_keyboard_ = nullptr;

    HistoryFilter filter_ = HistoryFilter::All;
};
