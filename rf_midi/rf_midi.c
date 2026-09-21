#include "rf_midi_model.h"
#include "rf_midi_view.h"
#include "usb_midi_tx.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <lib/subghz/devices/cc1101_configs.h>

#define RF_MIDI_TX_TIMEOUT_MS 2U
#define RF_MIDI_REFRESH_MS    100U
#define RF_MIDI_IDLE_POLL_MS  50U

typedef struct {
    FuriMessageQueue* input_queue;
    FuriMutex* ui_mutex;
    ViewPort* view_port;
    Gui* gui;
    RfMidiViewState snapshot; // Shared with GUI; always protected by ui_mutex.
    bool exit_requested; // Back has its own mailbox, so repeats cannot crowd it out.
    RfMidiViewState state; // Everything below is owned by the app thread.
    RfMidiTiming timing;
    uint32_t tick_frequency;
    uint32_t last_refresh;
    uint16_t pending_zero;
    uint8_t repeats;
    bool usb_connected;
} RfMidiApp;

static void rf_midi_draw(Canvas* canvas, void* context) {
    RfMidiApp* app = context;
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    const RfMidiViewState snapshot = app->snapshot;
    furi_mutex_release(app->ui_mutex);
    rf_midi_view_draw(canvas, &snapshot);
}

static void rf_midi_input(InputEvent* event, void* context) {
    RfMidiApp* app = context;
    if(event->key == InputKeyBack && event->type == InputTypePress) {
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        app->exit_requested = true;
        furi_mutex_release(app->ui_mutex);
    } else {
        // GUI callbacks never wait for the RF worker or for a full input queue.
        furi_message_queue_put(app->input_queue, event, 0);
    }
}

static bool rf_midi_should_exit(RfMidiApp* app) {
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    const bool exit = app->exit_requested;
    furi_mutex_release(app->ui_mutex);
    return exit;
}

static void rf_midi_publish(RfMidiApp* app, bool force) {
    const uint32_t now = furi_get_tick();
    if(!force && now - app->last_refresh < furi_ms_to_ticks(RF_MIDI_REFRESH_MS)) return;
    app->state.estimate_ms = rf_midi_estimate_ms(&app->timing, app->state.config);
    app->state.calibrated = app->timing.calibrated;
    if(!usb_midi_tx_is_connected()) app->state.usb_status = RfMidiUsbOffline;
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->snapshot = app->state;
    furi_mutex_release(app->ui_mutex);
    app->last_refresh = now;
    view_port_update(app->view_port);
}

static void rf_midi_invalidate_round(RfMidiApp* app) {
    app->state.last_valid = false;
    app->state.completed_points = 0;
    app->state.track = 0;
}

static bool rf_midi_apply_input(RfMidiApp* app, const InputEvent* event) {
    if(event->type == InputTypeRelease) {
        app->repeats = 0;
        return false;
    }
    const bool press = event->type == InputTypePress;
    const bool repeat = event->type == InputTypeRepeat;
    if(!press && !repeat) return false;
    if(press) app->repeats = 0;
    if(press && (event->key == InputKeyUp || event->key == InputKeyDown)) {
        app->state.edit_points = !app->state.edit_points;
        rf_midi_publish(app, true);
    } else if(press && event->key == InputKeyOk) {
        app->state.paused = !app->state.paused;
        rf_midi_invalidate_round(app);
        rf_midi_publish(app, true);
        return true;
    } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        if(repeat && app->repeats < UINT8_MAX) app->repeats++;
        // Fine taps; after five held repeats adjust points by five per repeat.
        int16_t delta = (app->state.edit_points && app->repeats >= 5U) ? 5 : 1;
        if(event->key == InputKeyLeft) delta = -delta;
        const uint8_t old_tracks = app->state.config.tracks;
        if(rf_midi_config_adjust(&app->state.config, app->state.edit_points, delta)) {
            app->pending_zero |= rf_midi_disabled_mask(old_tracks, app->state.config.tracks);
            rf_midi_invalidate_round(app);
            rf_midi_publish(app, true);
            return true;
        }
    }
    return false;
}

static bool rf_midi_process_input(RfMidiApp* app) {
    bool restart = false;
    InputEvent event;
    // Drain a bounded batch; held input cannot starve RF indefinitely.
    for(uint8_t i = 0; i < 16U; i++) {
        if(furi_message_queue_get(app->input_queue, &event, 0) != FuriStatusOk) break;
        if(rf_midi_apply_input(app, &event)) restart = true;
    }
    return restart;
}

static bool rf_midi_send_cc(RfMidiApp* app, uint8_t track, uint8_t value) {
    const bool sent =
        usb_midi_tx_control_change(RF_MIDI_CC_FIRST + track, value, RF_MIDI_TX_TIMEOUT_MS);
    app->state.usb_status = sent ? RfMidiUsbSent :
                                   (usb_midi_tx_is_connected() ? RfMidiUsbBusy : RfMidiUsbOffline);
    return sent;
}

static void rf_midi_clear_disabled(RfMidiApp* app) {
    const bool connected = usb_midi_tx_is_connected();
    if(connected && !app->usb_connected) {
        // A reconnect may have lost earlier zero packets; clear inactive controllers again.
        app->pending_zero |= rf_midi_disabled_mask(RF_MIDI_TRACKS_MAX, app->state.config.tracks);
        app->state.usb_status = RfMidiUsbReady;
    }
    app->usb_connected = connected;
    if(!connected) return;
    for(uint8_t track = 0; track < RF_MIDI_TRACKS_MAX; track++) {
        const uint16_t bit = 1U << track;
        if(!(app->pending_zero & bit)) continue;
        // Re-enabled tracks will be updated by the next full round.
        if(track < app->state.config.tracks || rf_midi_send_cc(app, track, 0)) {
            app->pending_zero &= ~bit;
        }
    }
}

static void rf_midi_rf_start(void) {
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
}

static void rf_midi_rf_stop(void) {
    furi_hal_subghz_idle();
    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    furi_hal_subghz_sleep();
}

/** Returns only whole rounds; settings and pause are applied at the next safe point boundary. */
static void rf_midi_scan_round(RfMidiApp* app) {
    const RfMidiConfig config = app->state.config;
    const uint32_t round_start = furi_get_tick();
    uint8_t window = UINT8_MAX;
    app->state.completed_points = 0;

    for(uint8_t track = 0; track < config.tracks; track++) {
        const uint32_t track_start = furi_get_tick();
        uint32_t point_ticks = 0;
        float max_rssi = -127.0f;
        app->state.track = track;
        for(uint16_t point = 0; point < config.points; point++) {
            const uint32_t point_start = furi_get_tick();
            if(rf_midi_process_input(app) || rf_midi_should_exit(app)) {
                furi_hal_subghz_idle();
                return;
            }
            const uint32_t frequency = rf_midi_point_frequency(config, track, point);
            const uint8_t next_window = rf_midi_frequency_window(frequency);
            furi_hal_subghz_idle();
            if(next_window != window) {
                furi_hal_subghz_set_frequency_and_path(frequency);
                window = next_window;
            } else {
                furi_hal_subghz_set_frequency(frequency);
            }
            furi_hal_subghz_rx();
            furi_delay_ms(RF_MIDI_SETTLE_MS);
            const float rssi = furi_hal_subghz_get_rssi();
            if(rssi > max_rssi) max_rssi = rssi;
            app->state.completed_points++;
            rf_midi_publish(app, false);
            point_ticks += furi_get_tick() - point_start;
        }
        // Consume changes that arrived during the final sample before sending/completing it.
        if(rf_midi_process_input(app) || rf_midi_should_exit(app)) {
            furi_hal_subghz_idle();
            return;
        }
        rf_midi_send_cc(app, track, rf_midi_rssi_to_cc(max_rssi));
        rf_midi_timing_observe(
            &app->timing,
            point_ticks,
            furi_get_tick() - track_start,
            config.points,
            app->tick_frequency);
    }
    furi_hal_subghz_idle();
    app->state.last_ms = rf_midi_ticks_to_ms(furi_get_tick() - round_start, app->tick_frequency);
    app->state.last_valid = true;
    rf_midi_publish(app, false);
}

static RfMidiApp* rf_midi_app_alloc(void) {
    RfMidiApp* app = malloc(sizeof(RfMidiApp));
    *app = (RfMidiApp){0};
    app->input_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app->ui_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    app->state.config = rf_midi_config_default();
    app->tick_frequency = furi_kernel_get_tick_frequency();
    app->state.estimate_ms = rf_midi_estimate_ms(&app->timing, app->state.config);
    app->snapshot = app->state;
    app->pending_zero = rf_midi_disabled_mask(RF_MIDI_TRACKS_MAX, app->state.config.tracks);
    view_port_draw_callback_set(app->view_port, rf_midi_draw, app);
    view_port_input_callback_set(app->view_port, rf_midi_input, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    return app;
}

static void rf_midi_app_free(RfMidiApp* app) {
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->ui_mutex);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t rf_midi_app(void* context) {
    UNUSED(context);
    RfMidiApp* app = rf_midi_app_alloc();
    FuriHalUsbInterface* previous_usb = furi_hal_usb_get_config();
    const bool usb_was_locked = furi_hal_usb_is_locked();
    if(usb_was_locked) furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&rf_midi_usb_interface, NULL)) {
        if(usb_was_locked) furi_hal_usb_lock();
        rf_midi_app_free(app);
        return -1;
    }
    // Keep this FAP's USB callbacks alive until it explicitly restores the old interface.
    furi_hal_usb_lock();
    rf_midi_rf_start();
    while(!rf_midi_should_exit(app)) {
        rf_midi_process_input(app);
        if(rf_midi_should_exit(app)) break;
        if(app->state.paused) furi_hal_subghz_idle();
        rf_midi_clear_disabled(app);
        if(app->state.paused) {
            furi_hal_subghz_idle();
            rf_midi_publish(app, false);
            InputEvent event;
            if(furi_message_queue_get(
                   app->input_queue, &event, furi_ms_to_ticks(RF_MIDI_IDLE_POLL_MS)) ==
               FuriStatusOk) {
                rf_midi_apply_input(app, &event);
            }
        } else {
            rf_midi_scan_round(app);
        }
    }
    rf_midi_rf_stop();
    // Best effort, bounded by 16 x TX timeout; disconnected USB returns immediately.
    for(uint8_t track = 0; track < RF_MIDI_TRACKS_MAX; track++) {
        rf_midi_send_cc(app, track, 0);
    }
    furi_hal_usb_unlock();
    // set_config is synchronous: no callbacks into this FAP may survive its return.
    furi_check(furi_hal_usb_set_config(previous_usb, NULL));
    if(usb_was_locked) furi_hal_usb_lock();
    rf_midi_app_free(app);
    return 0;
}
