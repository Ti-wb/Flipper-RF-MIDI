#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RF_MIDI_TRACKS_MIN     1U
#define RF_MIDI_TRACKS_MAX     16U
#define RF_MIDI_TRACKS_DEFAULT 10U
#define RF_MIDI_POINTS_MIN     2U
#define RF_MIDI_POINTS_MAX     200U
#define RF_MIDI_POINTS_DEFAULT 50U
#define RF_MIDI_SETTLE_MS      2U
#define RF_MIDI_CC_FIRST       20U

typedef struct {
    uint8_t tracks;
    uint16_t points;
} RfMidiConfig;

typedef struct {
    uint32_t point_us;
    uint32_t track_overhead_us;
    bool calibrated;
} RfMidiTiming;

RfMidiConfig rf_midi_config_default(void);
bool rf_midi_config_adjust(RfMidiConfig* config, bool edit_points, int16_t delta);
/** Equal-width tracks over the concatenated receive windows; never returns a gap. */
uint32_t rf_midi_point_frequency(RfMidiConfig config, uint8_t track, uint16_t point);
uint8_t rf_midi_frequency_window(uint32_t frequency);
uint8_t rf_midi_rssi_to_cc(float rssi);
uint32_t rf_midi_ticks_to_us(uint32_t ticks, uint32_t tick_frequency);
uint32_t rf_midi_ticks_to_ms(uint32_t ticks, uint32_t tick_frequency);
/** Learn only complete, uninterrupted tracks. overhead includes MIDI and scheduling/UI. */
void rf_midi_timing_observe(
    RfMidiTiming* timing,
    uint32_t point_ticks,
    uint32_t track_ticks,
    uint16_t points,
    uint32_t tick_frequency);
/** Before calibration this is explicitly only the mandatory settle-time lower bound. */
uint32_t rf_midi_estimate_ms(const RfMidiTiming* timing, RfMidiConfig config);
uint16_t rf_midi_disabled_mask(uint8_t old_tracks, uint8_t new_tracks);
