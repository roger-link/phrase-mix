# Phrase Mix

Phrase Mix is a native macOS instrument for discovering music by ear. Start a Main phrase, add related follower tracks, try patterns and sounds independently while the music runs, perform with the mix, and record the result. It is intended for open-ended exploration rather than timeline editing.

This is an evolving JUCE/C++ prototype, not a signed or notarized release.

## What it does

- Up to eight tracks with a Main phrase and related followers, each with its own pattern, sound, level, Hold, Mute, and Solo.
- Still, Drift, and Renew behavior; per-track pattern history and sound audition history.
- Built-in synthesis, mapped instruments, bundled samples, microphone captures, and takes turned into sounds.
- Per-track expression and shaping, shared space and mix controls, Pieces for editable saves, and Keeps for non-overwriting snapshots.
- Stereo take recording and trimmed, normalized final WAV export.
- An in-app Help window covering the creative approach and every control.

## Build on macOS

Install Xcode or its command line tools, CMake 3.22 or newer, and a JUCE source checkout. JUCE is an external dependency and is not included in this repository. The app has been built with JUCE 8.0.10.

```bash
cmake -S . -B build -DJUCE_PATH=/absolute/path/to/JUCE
cmake --build build --target PhraseMix -j 4
open "build/PhraseMix_artefacts/Phrase Mix.app"
```

If you use the Xcode generator, build with `--config Debug` and open the app in `build/PhraseMix_artefacts/Debug/` instead. Review JUCE's license before distributing a build.

## Local data

Phrase Mix keeps saved setups and user audio under `~/Library/Phrase Mix/`. This data is **not** part of the source repository. In the app, saved Pieces are editable setups; Keeps are separate snapshots with copied captured sounds; Takes and Finals are audio files belonging to a Piece. Removing a saved Piece setup does not remove its audio folder.

`Assets/Samples/` contains bundled CC0 samples and their source/license notices. Keep those notices with the assets when redistributing them.

## Engine checks

```bash
c++ -std=c++17 -O2 tests/engine_test.cpp -o /tmp/phrase-mix-engine-test
/tmp/phrase-mix-engine-test
```

These checks cover generation and audio-engine behavior. They do not replace listening tests or validation of audio devices and the macOS UI.
