# Sample2MIDI 🎵

**A professional VST3 plugin that converts audio samples into clean, usable MIDI.**

Built with JUCE (C++) | By Feddy Beatz

---

## What It Does

Sample2MIDI analyzes any audio file and automatically extracts the musical notes, converting them into MIDI data that you can export or drag directly into your DAW's piano roll. No cloud. No subscription. Fully offline.

---

## Features

- **Drag & Drop Audio Import** — Drag WAV, MP3, FLAC, or OGG files directly onto the plugin
- **Waveform Display** — Visual waveform with zoom in/out and playback cursor
- **YIN Pitch Detection** — Accurate monophonic pitch detection with note smoothing
- **MIDI Generation** — Produces clean notes with start time, end time, velocity, and note number
- **Scale Lock** — Snap detected notes to a chosen musical scale (C Major, C Minor, A Minor, etc.)
- **Quantize Control** — Slider from 0% (raw timing) to 100% (perfectly grid-aligned)
- **Pitch Bend Detection** — Toggle to capture pitch slides as MIDI pitch bend data
- **Note Range Filter** — Filter by Full Range, Bass Range, Lead Range, or Piano Range
- **Chord Detection Mode** — Group simultaneous notes into chords or keep them separate
- **Audio Playback** — Play/Stop controls with playback cursor inside the plugin
- **MIDI Export** — Export .mid file button + drag MIDI directly into FL Studio piano roll

---

## Project Structure

```
Sample2MIDI/
├── Source/
│   ├── PluginProcessor.cpp/.h     ← Core audio engine + MIDI output
│   ├── PluginEditor.cpp/.h        ← Main UI layout and controls
│   ├── PitchDetector.cpp/.h       ← YIN pitch detection algorithm
│   ├── MidiBuilder.cpp/.h         ← Note assembly, quantize, MIDI file export
│   ├── WaveformDisplay.cpp/.h     ← Waveform rendering component
│   ├── AudioFileLoader.cpp/.h     ← File drag/drop and browser dialog
│   └── ScaleQuantizer.cpp/.h      ← Scale snap and quantize logic
├── JUCE/                          ← JUCE framework (submodule)
└── CMakeLists.txt                 ← Build configuration
```

---

## Requirements

- **OS:** Windows 10 or 11 (64-bit)
- **DAW:** FL Studio (VST3 compatible DAWs also supported)
- **Format:** VST3 only
- **Build Tools:** CMake 3.22+, GCC or MSVC, JUCE (latest stable)

---

## Building From Source

### On Linux / WSL (Ubuntu)

**1. Install dependencies:**
```bash
sudo apt install -y cmake build-essential ninja-build pkg-config \
  libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libfreetype6-dev libasound2-dev libwebkit2gtk-4.0-dev libgtk-3-dev
```

**2. Clone the repo:**
```bash
git clone https://github.com/yourname/Sample2MIDI.git
cd Sample2MIDI
```

**3. Configure:**
```bash
cmake -B Build -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"
```

**4. Build:**
```bash
cmake --build Build
```

The compiled VST3 will be located at:
```
Build/Sample2MIDI_artefacts/Release/VST3/Sample2MIDI.vst3
```

### On Windows (Visual Studio 2022)

```bash
cmake -B Build -G "Visual Studio 17 2022" -A x64
```
Then open `Build/Sample2MIDI.sln` and press **Ctrl + Shift + B**.

---

## Installation

Copy the compiled `Sample2MIDI.vst3` file to:

```
C:\Program Files\Common Files\VST3\
```

Then in FL Studio:
```
Options → Manage Plugins → Start Scan
```

The plugin will appear in your effects list.

---

## How to Use

1. Open Sample2MIDI on a mixer insert in FL Studio
2. Drag an audio file onto the plugin window (or click the file browser button)
3. The waveform will display — press **Play** to preview the audio
4. Choose your **Scale**, **Quantize** level, and **Note Range**
5. Click **Convert / Detect** to analyze the audio
6. Review the detected MIDI notes
7. Either:
   - Click **Export MIDI** to save a `.mid` file
   - Drag the MIDI output directly into the FL Studio piano roll

---

## Pitch Detection

Sample2MIDI uses the **YIN algorithm** for fundamental frequency estimation.

Key settings that affect output quality:

| Setting | Effect |
|---|---|
| YIN Threshold (0.15 default) | Lower = stricter detection, fewer false notes |
| Minimum Note Length | Filters out noise and very short artifacts |
| Scale Lock | Snaps borderline notes to the nearest in-key note |
| Quantize | Corrects timing drift towards the beat grid |

---

## Supported Audio Formats

| Format | Supported |
|---|---|
| WAV | ✅ |
| MP3 | ✅ |
| FLAC | ✅ |
| OGG | ✅ |

---

## Known Limitations

- Currently optimized for **monophonic** audio (single melody lines, bass lines, leads)
- Chord detection mode is experimental on polyphonic audio
- Pitch bend detection works best on sustained notes with smooth pitch movement
- Very percussive audio (drums, transients) will produce minimal useful MIDI

---

## Roadmap

- [ ] Polyphonic pitch detection (FFT-based)
- [ ] VST3 parameter automation support
- [ ] macOS build support
- [ ] Custom scale editor
- [ ] MIDI velocity curve control
- [ ] Stem separation pre-processing

---

## License

© 2024 Feddy Beatz. All rights reserved.

---

## Contact

Built by **Feddy Beatz** — Music producer, beatmaker, and audio software developer.

> *"Turn any sound into playable MIDI."*
