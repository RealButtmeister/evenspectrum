# EvenSpectrum 0.1 — adaptive spectral EQ

A Windows 64-bit VST3 prototype that continuously reduces stronger frequency regions and raises quieter regions toward a flat spectrum, with gentle low- and high-frequency rolloffs.

## Install and open

1. Build the plugin from this source using the instructions below. This repository does not include a compiled plugin or installer.
2. Copy the entire built `EvenSpectrum.vst3` folder into `%CommonProgramFiles%\VST3`. Windows may request administrator permission.
3. Rescan plugins in your DAW and insert **EvenSpectrum** as an audio effect.
4. Open the DAW's generic plugin/parameter controls. This version has no custom editor window.

Keep the `.vst3` folder intact, including its `Contents` subfolder. The binary is Windows x64; it is not a Mac plugin.

## Controls

| Control | Default | What it does |
|---|---:|---|
| Flatten | 100% | Strength of adaptive EQ. At 0%, rolloffs still apply. |
| Response | 250 ms | How quickly analysis and EQ adapt. Higher values give steadier changes. |
| Max boost | 12 dB | Maximum lift of quiet frequency regions. |
| Max cut | 18 dB | Maximum reduction of strong frequency regions. |
| Low rolloff | 35 Hz | Low-end corner, approximately 12 dB/octave below it. |
| High rolloff | 16,000 Hz | High-end corner, approximately 12 dB/octave above it. |
| Activity floor | −75 dB | Below this input RMS level, adaptive correction relaxes toward zero. Rolloffs remain active. |
| Output | −6 dB | Final output gain; reduce it if your DAW's output meter clips. |
| Bypass | Off | Delayed original signal for comparison; processing latency is preserved. |

For a gentler result, start around 50% Flatten and 500 ms Response. Boosting quiet regions also raises noise in those regions. There is no output limiter or automatic loudness matching.

## What “flat” means here

The target is equal average spectral power per Hz in the working range, measured across 40 logarithmically spaced analysis regions. It is not equal energy per octave, a loudness target, or a flattened waveform. Music will not necessarily sound subjectively neutral with a mathematically flat spectrum.

The target level follows the input. Stronger regions are cut and quieter ones boosted, subject to the limits, response smoothing, and suppression of boosts in nearly empty regions. This is an approximate, bounded adaptive EQ: it cannot make absent frequencies appear or independently separate two sounds occupying the same frequency region. It does not apply broadband compression to equalize loud and quiet moments over time.

Low/high corners are excluded from adaptive correction outside the selected range, and the rolloff curves shape those ends downward. Stereo channels use the same EQ curve to retain their relative balance.

## Technical details and limitations

- Mono or stereo; 32-bit floating-point processing inside a 64-bit Windows plugin.
- 2048-sample transform, 512-sample hop, square-root Hann weighted overlap-add.
- Reported latency: 2048 samples, approximately 42.7 ms at 48 kHz. Intended for mixing rather than low-latency live monitoring.
- Bass resolution is limited by the transform size (about 23.4 Hz per bin at 48 kHz).
- Host automation takes the last value in each block. Adaptive EQ is smoothed; automation is not sample accurate.
- Parameter values are saved/restored with the DAW session. The learned spectrum restarts when processing is activated.
- No custom graphics, sidechain, presets browser, or 64-bit sample processing in this prototype.

## Verification

The included development logs record **47 Steinberg VST3 validator checks passed, 0 failed**. The audio-engine tests cover reconstruction and latency at 44.1, 48, and 96 kHz, silent input, stereo linking, delayed bypass, finite output, reduction of a prominent tone, quieter-band lift, boost/cut limits, low/high rolloffs, and midrange preservation.

See `Verification/` for those historical logs, with local build paths removed. Run the tests again after building on your machine. The plugin has not been auditioned on your recordings or tested in your particular DAW.

## Build from source

Use Windows x64, Git, CMake 3.25+, a C++17 compiler, and the Steinberg VST3 SDK. The tested SDK commit is `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` (3.8.1). The following commands use Visual Studio 2022 with the Desktop development with C++ workload installed.

```powershell
git clone https://github.com/steinbergmedia/vst3sdk.git external/vst3sdk
git -C external/vst3sdk checkout 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
git -C external/vst3sdk submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVST3_SDK_ROOT="$PWD/external/vst3sdk"
cmake --build build --config Release --target EvenSpectrum engine_tests --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

The historical development build used LLVM-MinGW 20260826, CMake 4.4.3, and Ninja 1.13.2. CMake includes accommodations for that compiler. To use that toolchain, put LLVM-MinGW and Ninja on PATH and replace the configure command with:

```powershell
cmake -S . -B build-mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DVST3_SDK_ROOT="$PWD/external/vst3sdk"
cmake --build build-mingw --target EvenSpectrum engine_tests --parallel 2
ctest --test-dir build-mingw --output-on-failure
```

The complete plugin bundle is under the chosen build folder's `VST3/Release/EvenSpectrum.vst3`. The build does not install it automatically. Start with an empty build folder when switching compilers.

This project uses Steinberg's SDK directly; JUCE is not part of this plugin. The SDK and build tools are obtained separately, not vendored here. Original third-party notices are preserved in `Licenses/`; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

No license has been selected for this project's own source. Third-party licenses apply only to their respective components; making the source public does not add a separate reuse license.
