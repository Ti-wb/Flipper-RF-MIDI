#pragma once

#include "rf_midi_model.h"
#include <gui/canvas.h>

typedef enum {
    RfMidiUsbOffline,
    RfMidiUsbReady,
    RfMidiUsbSent,
    RfMidiUsbBusy,
} RfMidiUsbStatus;

typedef struct {
    RfMidiConfig config;
    uint32_t estimate_ms;
    uint32_t last_ms;
    uint16_t completed_points;
    uint8_t track;
    bool edit_points;
    bool paused;
    bool calibrated;
    bool last_valid;
    RfMidiUsbStatus usb_status;
} RfMidiViewState;

/** Render one immutable snapshot. All coordinates use the Flipper 128 x 64 canvas. */
void rf_midi_view_draw(Canvas* canvas, const RfMidiViewState* state);
