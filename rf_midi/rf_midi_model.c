#include "rf_midi_model.h"

#include <limits.h>

RfMidiConfig rf_midi_config_default(void) {
    return (RfMidiConfig){.tracks = RF_MIDI_TRACKS_DEFAULT, .points = RF_MIDI_POINTS_DEFAULT};
}

bool rf_midi_config_adjust(RfMidiConfig* config, bool edit_points, int16_t delta) {
    const uint16_t old = edit_points ? config->points : config->tracks;
    const int32_t minimum = edit_points ? RF_MIDI_POINTS_MIN : RF_MIDI_TRACKS_MIN;
    const int32_t maximum = edit_points ? RF_MIDI_POINTS_MAX : RF_MIDI_TRACKS_MAX;
    int32_t value = (int32_t)old + delta;
    if(value < minimum) value = minimum;
    if(value > maximum) value = maximum;
    if(edit_points) {
        config->points = value;
    } else {
        config->tracks = value;
    }
    return old != value;
}

uint32_t rf_midi_point_frequency(RfMidiConfig config, uint8_t track, uint16_t point) {
    if(config.tracks < RF_MIDI_TRACKS_MIN || config.tracks > RF_MIDI_TRACKS_MAX ||
       config.points < RF_MIDI_POINTS_MIN || config.points > RF_MIDI_POINTS_MAX ||
       track >= config.tracks || point >= config.points) {
        return 0;
    }
    // 48 + 77 + 149 MHz of valid receive bandwidth, with the gaps removed.
    const uint32_t segments = config.points - 1U;
    const uint64_t position = (uint64_t)274000000U * (track * segments + point);
    const uint32_t offset = position / (config.tracks * segments);
    // A shared window boundary belongs to the lower window's inclusive endpoint.
    if(offset <= 48000000U) return 300000000U + offset;
    if(offset <= 125000000U) return 387000000U + offset - 48000000U;
    return 779000000U + offset - 125000000U;
}

uint8_t rf_midi_frequency_window(uint32_t frequency) {
    if(frequency >= 300000000U && frequency <= 348000000U) return 0;
    if(frequency >= 387000000U && frequency <= 464000000U) return 1;
    if(frequency >= 779000000U && frequency <= 928000000U) return 2;
    return UINT8_MAX;
}

uint8_t rf_midi_rssi_to_cc(float rssi) {
    if(!(rssi > -95.0f)) return 0; // Includes NaN.
    if(rssi >= -45.0f) return 127;
    return (uint8_t)(((rssi + 95.0f) * 127.0f / 50.0f) + 0.5f);
}

static uint32_t rf_midi_ticks_scale(uint32_t ticks, uint32_t frequency, uint32_t scale) {
    if(!frequency) return 0;
    const uint64_t result = ((uint64_t)ticks * scale) / frequency;
    return result > UINT32_MAX ? UINT32_MAX : result;
}

uint32_t rf_midi_ticks_to_us(uint32_t ticks, uint32_t tick_frequency) {
    return rf_midi_ticks_scale(ticks, tick_frequency, 1000000U);
}

uint32_t rf_midi_ticks_to_ms(uint32_t ticks, uint32_t tick_frequency) {
    return rf_midi_ticks_scale(ticks, tick_frequency, 1000U);
}

void rf_midi_timing_observe(
    RfMidiTiming* timing,
    uint32_t point_ticks,
    uint32_t track_ticks,
    uint16_t points,
    uint32_t tick_frequency) {
    if(!points || !tick_frequency || !point_ticks || track_ticks < point_ticks) return;
    const uint32_t point_us = rf_midi_ticks_to_us(point_ticks, tick_frequency) / points;
    const uint32_t overhead_us = rf_midi_ticks_to_us(track_ticks - point_ticks, tick_frequency);
    if(timing->calibrated) {
        timing->point_us = ((uint64_t)timing->point_us * 3U + point_us) / 4U;
        timing->track_overhead_us = ((uint64_t)timing->track_overhead_us * 3U + overhead_us) / 4U;
    } else {
        timing->point_us = point_us;
        timing->track_overhead_us = overhead_us;
        timing->calibrated = true;
    }
}

uint32_t rf_midi_estimate_ms(const RfMidiTiming* timing, RfMidiConfig config) {
    const uint32_t floor_ms = (uint32_t)config.tracks * config.points * RF_MIDI_SETTLE_MS;
    if(!timing->calibrated) return floor_ms;
    const uint64_t us =
        ((uint64_t)timing->point_us * config.points + timing->track_overhead_us) * config.tracks;
    const uint32_t measured_ms = (us + 999U) / 1000U;
    return measured_ms > floor_ms ? measured_ms : floor_ms;
}

uint16_t rf_midi_disabled_mask(uint8_t old_tracks, uint8_t new_tracks) {
    if(old_tracks > RF_MIDI_TRACKS_MAX || new_tracks >= old_tracks) return 0;
    return ((1UL << old_tracks) - 1U) & ~((1UL << new_tracks) - 1U);
}
