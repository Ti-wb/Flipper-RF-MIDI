#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <furi_hal_usb.h>

extern FuriHalUsbInterface rf_midi_usb_interface;

/** Send one USB-MIDI 1.0 Control Change event.
 *
 * The call drops immediately while disconnected and waits no longer than
 * timeout_ms for a busy IN endpoint.
 */
bool usb_midi_tx_control_change(uint8_t controller, uint8_t value, uint32_t timeout_ms);

/** True once the host configured MIDI and while the USB bus is awake. */
bool usb_midi_tx_is_connected(void);
