#include "rf_midi_view.h"

#include <stdio.h>

static void
    rf_midi_view_row(Canvas* canvas, uint8_t y, bool selected, const char* label, uint16_t value) {
    char text[12];
    if(selected) {
        canvas_draw_box(canvas, 0, y - 10U, 128, 13);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 3, y, label);
    snprintf(text, sizeof(text), "< %u >", value);
    canvas_draw_str_aligned(canvas, 124, y, AlignRight, AlignBottom, text);
    canvas_set_color(canvas, ColorBlack);
}

static void rf_midi_view_duration(char* text, size_t size, uint32_t milliseconds) {
    if(milliseconds < 1000U) {
        snprintf(text, size, "%lums", (unsigned long)milliseconds);
    } else if(milliseconds >= 1000000U) {
        snprintf(text, size, ">=1000s");
    } else {
        snprintf(
            text,
            size,
            "%lu.%02lus",
            (unsigned long)(milliseconds / 1000U),
            (unsigned long)(milliseconds % 1000U / 10U));
    }
}

void rf_midi_view_draw(Canvas* canvas, const RfMidiViewState* state) {
    char text[32];
    char duration[16];
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 9, "RF MIDI");
    canvas_set_font(canvas, FontSecondary);
    if(state->paused) {
        snprintf(text, sizeof(text), "PAUSED");
    } else {
        snprintf(text, sizeof(text), "RUN %u/%u", state->track + 1U, state->config.tracks);
    }
    canvas_draw_str_aligned(canvas, 128, 9, AlignRight, AlignBottom, text);

    rf_midi_view_row(canvas, 23, !state->edit_points, "Tracks", state->config.tracks);
    rf_midi_view_row(canvas, 36, state->edit_points, "Points/track", state->config.points);

    rf_midi_view_duration(duration, sizeof(duration), state->estimate_ms);
    snprintf(
        text,
        sizeof(text),
        "Est %s%s",
        state->estimate_ms >= 1000000U ? "" : (state->calibrated ? "~" : ">="),
        duration);
    canvas_draw_str(canvas, 0, 48, text);
    if(state->last_valid) {
        rf_midi_view_duration(duration, sizeof(duration), state->last_ms);
        snprintf(text, sizeof(text), "Last %s", duration);
    } else {
        snprintf(text, sizeof(text), "Last --");
    }
    canvas_draw_str_aligned(canvas, 128, 48, AlignRight, AlignBottom, text);

    const uint32_t total = (uint32_t)state->config.tracks * state->config.points;
    canvas_draw_line(canvas, 0, 52, 127, 52);
    if(total && !state->paused) {
        canvas_draw_box(canvas, 0, 51, 128U * state->completed_points / total, 3);
    }
    canvas_draw_str(canvas, 0, 61, state->paused ? "OK resume" : "OK pause");
    const char* usb = "USB off";
    if(state->usb_status == RfMidiUsbReady) usb = "USB ready";
    if(state->usb_status == RfMidiUsbSent) usb = "MIDI queued";
    if(state->usb_status == RfMidiUsbBusy) usb = "MIDI drop";
    canvas_draw_str_aligned(canvas, 128, 61, AlignRight, AlignBottom, usb);
}
