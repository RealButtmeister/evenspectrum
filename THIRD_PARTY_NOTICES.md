# Third-party notices

EvenSpectrum builds against the [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk), version 3.8.1 at commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`. The SDK is obtained separately and is not included in this repository. Its original MIT notice is preserved in [Licenses/Steinberg-VST3.txt](Licenses/Steinberg-VST3.txt). VST is a trademark of Steinberg Media Technologies GmbH.

The original project also included [Licenses/LLVM-MinGW.txt](Licenses/LLVM-MinGW.txt), which is retained here. LLVM-MinGW and its runtime components are not distributed by this source repository. Consult the toolchain's complete notices when distributing a compiled binary, particularly a build using static runtimes.

The plugin source uses the VST3 SDK directly; no JUCE source or binary is included. No license has been selected for the project's own source. These notices apply to the named third-party components and do not license the original plugin implementation.
