# Amorphetude

- Overdrive
- Echo
- Autowah
- BitCrushing
- Compressor

## Build

```bash
cmake -D CMAKE_BUILD_TYPE:STRING=Debug -D JUCE_ROOT_DIR=<path-to-JUCE> -B <path-to-build> -G "Unix Makefiles"
cmake --build <path-to-build> --config Debug --target <target> -j <jobs>
```
