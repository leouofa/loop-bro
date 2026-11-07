# Loop Bro 🎸

An intelligent, auto-triggered guitar audio looper and phrase capture plugin (**VST3**, **AU**, and **macOS Standalone app**) built with **C++20** and **JUCE 8**.

Loop Bro is designed for guitarists who want hands-free riff capture: plug your guitar into your audio interface, arm the plugin, and just play. Loop Bro continuously buffers incoming audio, catches the pick attack transient, prepends a configurable pre-roll window, and automatically finishes recording when your strum amplitude dies down.

---

## Features

- **Hands-Free Auto-Capture**: Armed state continuously monitors input signal using a fast-attack envelope follower. The moment amplitude crosses `Start Threshold`, recording triggers immediately.
- **Configurable Pre-Roll Ring Buffer (`0–2000 ms`)**: Never misses the pick attack transient or slide-in. Pre-roll audio is prepended with a smooth 3 ms raised-cosine fade-in to eliminate starting clicks.
- **Automatic Tail Trimming with Anti-Click Fade**: When amplitude stays below `Stop Threshold` for `Stop Hold ms`, Loop Bro stops recording, automatically trims off the trailing silence, and applies a 15 ms cosine fade-out over natural decay.
- **Early Stop Capture**: Disarming while recording automatically finalizes and trims the take cleanly with anti-click crossfades, giving you immediate manual control when you don't want to wait for ring-out.
- **Clean Re-Arm Protection**: Dynamic reset of envelope followers, pre-roll buffers, and cached levels when arming prevents lingering decay tails from falsely triggering new captures.
- **Single-Slot Disarm Workflow**: Automatically disarms upon take completion into a single clip slot so you can loop, audition, or export without accidental re-triggers.
- **Free-Length Looper**: Audition and loop your riff at full fidelity independent of DAW tempo or transport.
- **Drag-to-DAW & 24-bit WAV Export**: Drag the captured riff directly from the waveform display onto an Ableton, Logic, Reaper, Studio One, or Bitwig track, or click **Save WAV...** to export anywhere.
- **Live Input Thru**: Jam along over your captured loop with the live guitar pass-through and a dedicated **Loop Level** knob.
- **Visual Threshold Metering**: Real-time dB level meter with color-coded marker lines for Start Threshold (Amber) and Stop Threshold (Green) makes calibration effortless.

---

## Controls

| Parameter | Range | Default | Description |
|---|---|---|---|
| **Start Threshold** | `-80.0 dB` to `0.0 dB` | `-30.0 dB` | Amplitude required to trigger recording on the first strum. |
| **Stop Threshold** | `-80.0 dB` to `0.0 dB` | `-48.0 dB` | Amplitude level below which the signal must drop to initiate stopping. |
| **Pre-Roll** | `0 ms` to `2000 ms` | `50 ms` | Duration of audio prior to the trigger to prepend to the clip. |
| **Stop Hold** | `50 ms` to `10000 ms` | `500 ms` | How long amplitude must remain below Stop Threshold before finishing recording. |
| **Loop Level** | `-60.0 dB` to `+6.0 dB` | `0.0 dB` | Playback gain for the looped audio take. |
| **Live Thru** | `On / Off` | `Off` | Monitors live guitar input through the plugin output. |

---

## Installation & Build Instructions

### Prerequisites
- macOS 12+ (Apple Silicon or Intel)
- CMake 3.22+ (`brew install cmake`)
- Xcode or Apple Command Line Tools (`xcode-select --install`)
- Python 3.8+ (for building the Max for Live device)

### Building from Source
```bash
# Clone the repository
git clone https://github.com/your-username/loop-bro.git
cd loop-bro

# Configure CMake (fetches JUCE 8 automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all plugin formats (AU, VST3, Standalone, Unit Tests)
cmake --build build --config Release -j8

# Run automated tests
ctest --test-dir build --output-on-failure --verbose
```

### Installed Locations
After building, the plugin bundles are automatically copied to:
- **VST3**: `~/Library/Audio/Plug-Ins/VST3/Loop Bro.vst3`
- **Audio Unit (AU)**: `~/Library/Audio/Plug-Ins/Components/Loop Bro.component`
- **Standalone App**: `build/LoopBro_artefacts/Release/Standalone/Loop Bro.app`

### Validating Audio Unit
```bash
auval -v aufx LpBr LpBr
```

---

## How to Use

1. **Insert Loop Bro** on your guitar track (or launch the Standalone app).
2. Look at the **dB meter** on the left while playing a strum to adjust your **Start Threshold** (amber line) and **Stop Threshold** (green line).
3. Click **ARM**. Loop Bro enters `ARMED — LISTENING`.
4. Play your riff! The plugin detects the attack, switches to `RECORDING`, and shows the elapsed time.
5. Stop playing. Once your guitar decays below the stop threshold for your configured **Stop Hold** duration, the plugin stops and displays the trimmed waveform.
6. Click **PLAY LOOP** to hear your riff looping seamlessly while jamming over it.
7. Click and drag the **"DRAG TO DAW"** badge or waveform onto any DAW track to drop the 24-bit WAV clip directly into your project!

---

## Ableton Live & Push 3 Integration 🎛️

Loop Bro includes deep, native integration for **Ableton Live 11/12** and the **Ableton Push 3 controller** (in both Controller and Standalone modes) via its companion Max for Live audio device: **`LoopBroControl.amxd`**.

```
┌────────────────────────────────────────────────────────────────────────┐
│ Ableton Audio Track                                                    │
│                                                                        │
│   Guitar In ──► [ Loop Bro (VST3/AU) ] ──► [ LoopBroControl.amxd ] ──► │
│                         ▲                          ▲                   │
│                         │   UDP 9001 (Control)     │                   │
│                         └──────────────────────────┤                   │
│                             UDP 9002 (Wave/State)  │                   │
│                                                    ▼                   │
│                                           [ Ableton Push 3 ]           │
└────────────────────────────────────────────────────────────────────────┘
```

---

### Ableton Push 3 Controller Support

`LoopBroControl.amxd` includes custom parameter bank definitions (`live.banks`) tailor-made for Push 3's 8 rotary encoders and high-resolution screen. Parameters feature contextual hardware names and clear state labels (no generic `val1`/`val2` or truncated identifiers).

#### Bank 1: "Main" (Core Performance & Detection)
Bank 1 maps all 8 Push rotary encoders to the primary recording, playback, and DSP detection parameters:

| Push Encoder | Parameter Name | Hardware Short Name | Value Range / States | Description |
|:---:|:---|:---|:---|:---|
| **1** | **`Arm/Disarm`** | `Arm` | `Disarmed` / `Armed` | Arms the engine for auto-strum recording; turns to `STOP REC` during capture. |
| **2** | **`Play/Stop Loop`** | `Loop` | `Stopped` / `Playing` | Starts or stops playback of the captured guitar riff. |
| **3** | **`Clear Loop`** | `Clear` | `Inactive` / `Clear` | Discards current riff buffer and resets engine to Idle. |
| **4** | **`Start Threshold`** | `Start Thr` | `-80.0 dB` to `0.0 dB` | Input level that triggers automatic recording on first pick attack. |
| **5** | **`Stop Threshold`** | `Stop Thr` | `-80.0 dB` to `0.0 dB` | Level below which signal must drop to initiate end of phrase. |
| **6** | **`Pre-Roll Time`** | `Pre-Roll` | `0 ms` to `2000 ms` | Audio prepended from the ring buffer to preserve the first transient. |
| **7** | **`Stop Hold Time`** | `Stop Hold` | `50 ms` to `10000 ms` | Duration amplitude must stay below Stop Threshold before finishing. |
| **8** | **`Loop Level`** | `Loop Lvl` | `-60.0 dB` to `+6.0 dB` | Volume of the looping take relative to dry guitar. |

#### Bank 2: "Monitoring" (Signal Routing)
Access Bank 2 on Push 3 by pressing the upper display bank button above the device:

| Push Encoder | Parameter Name | Hardware Short Name | Value Range / States | Description |
|:---:|:---|:---|:---|:---|
| **1** | **`Live Through`** | `Live Thru` | `Off` / `On` | Toggles dry guitar pass-through monitoring inside Loop Bro. |
| **2–8** | *Unused* | `-` | — | Reserved for future routing expansion. |

---

### Live Device GUI: Dual Waveform Display

In Live's device chain view, `LoopBroControl` presents an elegant dual-visualizer interface:

1. **Captured Riff Waveform**:
   - Real-time display of the recorded guitar take loaded dynamically into a Max `buffer~` / `waveform~`.
   - Features a high-contrast yellow playhead cursor showing the synchronized loop playback position and trimmed take boundaries.
2. **Live Audio Oscilloscope**:
   - Real-time scrolling waveform monitor (`live.scope~`) tapping directly into incoming and outgoing guitar audio signals via `plugin~` and `plugout~`.
   - Allows instant visual calibration of pickup output levels against threshold settings.
3. **Dynamic State Badge & Transport**:
   - Large colored status badge displaying current engine mode: `IDLE` (amber), `ARMED` (amber), `RECORDING` (red), `CAPTUR` (green), or `PLAY` (green).
   - One-click `ARM`, `PLAY LOOP`, `CLEAR`, and `SYNC` buttons with auto-disarm safety.

---

### Zero-Configuration OSC Bridge

The VST3/AU plugin and M4L device communicate over bidirectional local UDP network sockets:
- **Port 9001 (M4L &rarr; Plugin)**: Transmits transport commands (`/loopbro/arm`, `/loopbro/play`, `/loopbro/clear`), parameter dial tweaks (`/loopbro/param/*`), and full-state resync queries (`/loopbro/query`).
- **Port 9002 (Plugin &rarr; M4L)**: Streams engine states (`/loopbro/state`), playhead positions (`/loopbro/playhead`), dB peak metering (`/loopbro/meter`), and temporary 24-bit take paths (`/loopbro/waveform/file`).
- **Headless Background Staging**: Takes are exported and transferred to M4L automatically in the background, even when the VST3/AU editor window is closed.
- **Instant SYNC**: Click the `SYNC` button in the M4L device (or send `/loopbro/query`) at any time to immediately reconcile all parameters and states.

---

### Setup in Ableton Live

1. **Insert Loop Bro**:
   - Drag **Loop Bro** (VST3 or AU) onto an audio track with your guitar input.
2. **Insert LoopBroControl**:
   - Drag **`LoopBroControl.amxd`** immediately after Loop Bro in the same track's device chain.
   - The device is automatically available in Live's browser at:
     ```
     User Library > Presets > Audio Effects > Max Audio Effect > LoopBroControl.amxd
     ```
3. **Push 3 Operation**:
   - Select the track on Push 3.
   - Bank 1 ("Main") is active by default. Use encoders 1–3 to Arm, Play, and Clear, and encoders 4–8 to dial in threshold and timing values.
   - Press the bank button above the display to switch to Bank 2 ("Monitoring") and toggle `Live Thru`.
4. **Ableton Info View**:
   - Hover your mouse over any control or scope in the M4L device to view detailed guitar setup advice and threshold calibration tips directly in Live's lower-left Info View panel.

---

### Building & Deploying the M4L Device

To regenerate the `.amxd` binary file from the Python specification script:

```bash
python3 m4l/build_amxd.py
```

This compiles `m4l/LoopBroControl.amxd` and automatically installs it to your local Ableton User Library presets directory (`~/Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/LoopBroControl.amxd`).

