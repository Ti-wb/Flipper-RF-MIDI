#include "rf_midi_model.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    unsigned long samples = 0;
    for(uint8_t tracks = 1; tracks <= 16; tracks++) {
        for(uint16_t points = 2; points <= 200; points++) {
            RfMidiConfig config = {.tracks = tracks, .points = points};
            assert(rf_midi_point_frequency(config, 0, 0) == 300000000U);
            assert(rf_midi_point_frequency(config, tracks - 1, points - 1) == 928000000U);
            uint32_t previous = 0;
            for(uint8_t track = 0; track < tracks; track++) {
                for(uint16_t point = 0; point < points; point++) {
                    uint32_t frequency = rf_midi_point_frequency(config, track, point);
                    assert(rf_midi_frequency_window(frequency) < 3);
                    assert(frequency >= previous);
                    if(point > 0) assert(frequency > previous);
                    previous = frequency;
                    samples++;
                }
            }
        }
    }
    assert(rf_midi_point_frequency((RfMidiConfig){1, 1}, 0, 0) == 0);
    assert(rf_midi_point_frequency((RfMidiConfig){17, 200}, 0, 0) == 0);
    assert(rf_midi_point_frequency((RfMidiConfig){16, 200}, 16, 0) == 0);
    assert(rf_midi_point_frequency((RfMidiConfig){16, 200}, 0, 200) == 0);
    assert(rf_midi_frequency_window(500000000U) == UINT8_MAX);

    RfMidiConfig config = rf_midi_config_default();
    assert(config.tracks == 10 && config.points == 50);
    assert(rf_midi_config_adjust(&config, false, 100));
    assert(config.tracks == 16);
    assert(!rf_midi_config_adjust(&config, false, 1));
    assert(rf_midi_config_adjust(&config, true, -500));
    assert(config.points == 2);
    assert(!rf_midi_config_adjust(&config, true, -1));
    assert(rf_midi_config_adjust(&config, false, -500));
    assert(config.tracks == 1);
    assert(rf_midi_config_adjust(&config, true, 500));
    assert(config.points == 200);

    RfMidiTiming timing = {0};
    config = (RfMidiConfig){.tracks = 1, .points = 2};
    assert(rf_midi_estimate_ms(&timing, config) == 4);
    assert(rf_midi_ticks_to_ms(1024, 1024) == 1000);
    assert(rf_midi_ticks_to_us(128, 1024) == 125000);
    assert(rf_midi_ticks_to_us(UINT32_MAX, 1000) == UINT32_MAX);
    assert(rf_midi_ticks_to_ms(3, 0) == 0);
    rf_midi_timing_observe(&timing, 0, 10, 2, 1000);
    assert(!timing.calibrated);
    rf_midi_timing_observe(&timing, 8, 7, 2, 1000);
    assert(!timing.calibrated);
    rf_midi_timing_observe(&timing, 8, 10, 2, 1000);
    assert(timing.calibrated && timing.point_us == 4000 && timing.track_overhead_us == 2000);
    assert(rf_midi_estimate_ms(&timing, config) == 10);
    config = (RfMidiConfig){.tracks = 16, .points = 200};
    assert(rf_midi_estimate_ms(&timing, config) == 12832);
    // Unsigned elapsed subtraction remains valid across the scheduler tick wrap.
    uint32_t start = UINT32_MAX - 7U;
    uint32_t now = 2U;
    assert(rf_midi_ticks_to_ms(now - start, 1000) == 10);

    assert(rf_midi_disabled_mask(16, 1) == 0xFFFE);
    assert(rf_midi_disabled_mask(10, 8) == 0x0300);
    assert(rf_midi_disabled_mask(1, 16) == 0);
    assert(rf_midi_disabled_mask(16, 16) == 0);
    assert(rf_midi_rssi_to_cc(-100) == 0);
    assert(rf_midi_rssi_to_cc(-95) == 0);
    assert(rf_midi_rssi_to_cc(-70) == 64);
    assert(rf_midi_rssi_to_cc(-45) == 127);
    assert(rf_midi_rssi_to_cc(0) == 127);
    assert(rf_midi_rssi_to_cc(NAN) == 0);
    printf(
        "PASS model: 3184 configurations, %lu frequency samples, timing/limits/masks/RSSI\n",
        samples);
}
