# Flipper RF MIDI

**Turn radio activity into MIDI control.**

Flipper RF MIDI turns Sub-GHz signal strength into USB MIDI Control Change messages. Use nearby radio activity to move parameters in your DAW or other MIDI software. Each track supplies a control value you can map to a parameter.

## What it does

- **Native USB MIDI:** connects as **Flipper RF MIDI**, with up to 16 CC outputs.
- **Adjustable on device:** choose the number of tracks and sampling points without restarting.
- **Live feedback:** see scan progress, estimated duration, the last completed scan, and USB status.
- **Pause and resume:** stop sampling with one button, then start a fresh scan.

## Quick start

Download `rf_midi.fap` from the [latest release](https://github.com/Ti-wb/Flipper-RF-MIDI/releases/latest), then:

1. Copy it to `apps/Tools/` on your Flipper Zero’s microSD card.
2. Connect Flipper to your computer with a USB data cable.
3. Open **Apps → Tools → RF MIDI**.
4. Select **Flipper RF MIDI** as an input in your MIDI software, use **Channel 1**, and map a CC to a parameter.

For example, assign **CC20** to a synth’s filter cutoff using your host’s MIDI Learn or manual mapping. The first track’s radio activity will then control that parameter.

## Tune the scan

| Setting | Range | Default |
| --- | --- | --- |
| Tracks (independent CC controls) | 1–16 | 10 |
| Points per track | 2–200 | 50 |

More points give denser sampling and slower scans. Fewer points give faster, sparser scans. Changing the track count redistributes the full supported frequency range, so the same CC can represent a different RF range. Settings reset when you relaunch the app.

| Control | Action |
| --- | --- |
| Up / Down | Select Tracks or Points/track |
| Left / Right | Decrease or increase by 1 |
| Hold Left / Right | Repeat; point adjustments accelerate to steps of 5 |
| OK | Pause / resume |
| Back | Exit and restore the previous USB interface |

**Est** shows an estimated scan duration; `>=` means only the minimum wait time is known, while `~` marks an estimate based on measured timing. **Last** shows the most recent complete scan and clears when you change settings or pause.

## Fixed MIDI mapping

- **Channel:** 1
- **Tracks 1–16:** CC20–CC35; the default 10 tracks use CC20–CC29.
- **Value:** each track’s highest sampled RSSI, mapped from **−95…−45 dBm** to **0…127**, clamped at either end.
- **Receive windows:** **300–348, 387–464, and 779–928 MHz**.

Tracks update in sequence, with scan time determined by your settings. `MIDI queued` means the packet entered the USB transmit queue.

## Compatibility & development

The app temporarily replaces the USB interface used by qFlipper; press **Back** to restore it. **v0.8** builds against SDK API **88.2**. The current binary still needs hardware validation; see [Compatibility & testing](rf_midi/verification.md).

To build from source, place `rf_midi/` under `applications_user/` in an API-compatible [official firmware SDK](https://github.com/flipperdevices/flipperzero-firmware), then run `./fbt fap_rf_midi`.

## License

[MIT](LICENSE).
