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

## Max for Live (M4L) Integration 🎛️

Loop Bro includes a dedicated Max for Live Audio Effect device: **`LoopBroControl.amxd`**.

### Features
- **Dual Soundwave Visualization (Split Side-by-Side)**:
  - **Captured Riff Waveform**: Real-time waveform display of the recorded take loaded into `buffer~` / `waveform~` with a synchronized playhead cursor.
  - **Live Audio Oscilloscope**: Real-time scrolling waveform monitor (`live.scope~`) fed directly from incoming/outgoing guitar audio.
- **Bi-Directional Parameter Control & Startup Synchronization**:
  - Live dials and toggles for `Start Threshold`, `Stop Threshold`, `Pre-Roll`, `Stop Hold`, `Loop Level`, and `Live Thru`.
  - Transport controls: `ARM`, `PLAY LOOP`, and `CLEAR`.
  - **Dynamic ARM / STOP REC Button**: Transforms into `STOP REC` during active recording to allow manual early take completion with anti-click fades, and automatically disarms when recording completes.
  - Initialized parameter defaults to prevent zero-jump desynchronization when inserting the device into an active set.
  - Automatic two-way synchronization between Loop Bro GUI and M4L dials.
- **Ableton Info View Text Hints**:
  - Comprehensive native Ableton Info View annotations across all dials, toggles, buttons, waveform display, and live oscilloscope.
  - Instant guitar-centric calibration advice and workflow hints visible in Live's lower-left Info View panel on hover.
- **Zero-Configuration Local OSC**:
  - M4L &rarr; VST: UDP port `9001` (`/loopbro/arm`, `/loopbro/play`, `/loopbro/clear`, `/loopbro/param/*`, `/loopbro/query`)
  - VST &rarr; M4L: UDP port `9002` (`/loopbro/state`, `/loopbro/playhead`, `/loopbro/meter`, `/loopbro/waveform/file`, `/loopbro/param/*`)
  - Headless background audio take staging: WAV files are immediately staged and broadcast over OSC even with the VST plugin window closed.

### Setup in Ableton Live
1. Insert **Loop Bro (VST3 or AU)** on an audio track.
2. Insert **`LoopBroControl.amxd`** immediately after Loop Bro on the same track.
   - The device is pre-installed to: `~/Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/LoopBroControl.amxd`
   - Or drag `m4l/LoopBroControl.amxd` directly from the repository into your track.
3. Everything synchronizes automatically on insert via `/loopbro/query`!

### Building the M4L Device
To regenerate or rebuild the binary `.amxd` file from source:
```bash
python3 m4l/build_amxd.py
```
This builds `m4l/LoopBroControl.amxd` and automatically installs it to your Ableton User Library presets directory if present.

