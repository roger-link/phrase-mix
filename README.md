# Phrase Mix

Phrase Mix is a native macOS instrument for discovering music by ear. Start a Main phrase, add related follower tracks, try patterns and sounds independently while the music runs, perform with the mix, and record the result. It is intended for open-ended exploration rather than timeline editing.

This is an evolving JUCE/C++ prototype, not a signed or notarized release.

## What it does

- Up to eight tracks with a Main phrase and related followers, each with its own pattern, sound, level, Hold, Mute, Solo, Focus, and timed level fades. Clone copies a selected track into an independent new row.
- Still, Drift, and Renew behavior; per-track pattern history and sound audition history.
- Optional on-device AI ideas for one track's phrase and expression, or for a whole arrangement with complementary tracks.
- Built-in synthesis, mapped instruments, bundled samples, microphone captures, and takes turned into sounds.
- Per-track expression and shaping, shared space and mix controls, Pieces for editable saves, and Keeps for non-overwriting snapshots.
- Stereo take recording and trimmed, normalized final WAV export.
- A hosted AU/VST3 instrument source per track, followed by two ordered audio effect slots; a whole-piece effect follows the mix. Plugin settings are saved in Pieces and Keeps.
- Opt-in stereo audio return from the selected two-channel input, monitored through Phrase Mix and included in takes.
- MIDI Learn for connected input controllers: CC faders/knobs for track levels and expression, and CC buttons or notes for track actions, pattern navigation, Keep, and transport. Mappings persist across Pieces.
- Opt-in MIDI note output to one shared virtual port, eight separate track ports, or another MIDI destination.
- An in-app Help window covering the creative approach and every control.

## Build on macOS

Install Xcode 26 or newer, CMake 3.22 or newer, and a JUCE source checkout. JUCE is an external dependency and is not included in this repository. The app has been built with JUCE 8.0.10. The AI helper needs the Foundation Models SDK to compile; using AI idea at runtime also requires macOS 26, a compatible Mac, and Apple Intelligence enabled.

```bash
cmake -S . -B build-local -G "Unix Makefiles" -DJUCE_PATH=/absolute/path/to/JUCE -DCMAKE_BUILD_TYPE=Debug
cmake --build build-local --target PhraseMix -j 4
open "build-local/PhraseMix_artefacts/Debug/Phrase Mix.app"
```

If you use the Xcode generator, build with `--config Debug` and open the app in that build folder's `PhraseMix_artefacts/Debug/` directory. Review JUCE's license before distributing a build.

## Local data

Phrase Mix keeps saved setups and user audio under `~/Library/Phrase Mix/`. This data is **not** part of the source repository. In the app, saved Pieces are editable setups; Keeps are separate snapshots with copied captured sounds; Takes and Finals are audio files belonging to a Piece. Removing a saved Piece setup does not remove its audio folder.

MIDI input selections, Learn assignments, and output routing are stored separately in `~/Library/Phrase Mix/MidiControls.json`. Audio input/output selection is stored in `~/Library/Phrase Mix/AudioDevice.xml`; scanned effect plugin listings are cached in `~/Library/Phrase Mix/Effects.xml`. Enable an input in the MIDI menu before learning a control.

`Assets/Samples/` contains bundled CC0 samples and their source/license notices. Keep those notices with the assets when redistributing them.

## Engine checks

```bash
c++ -std=c++17 -O2 tests/engine_test.cpp -o /tmp/phrase-mix-engine-test
/tmp/phrase-mix-engine-test
c++ -std=c++17 -O2 tests/midi_mapping_test.cpp -o /tmp/phrase-mix-midi-test
/tmp/phrase-mix-midi-test
c++ -std=c++17 -O2 tests/midi_output_test.cpp -o /tmp/phrase-mix-midi-output-test
/tmp/phrase-mix-midi-output-test
c++ -std=c++17 -O2 tests/audio_return_test.cpp -o /tmp/phrase-mix-audio-return-test
/tmp/phrase-mix-audio-return-test
c++ -std=c++17 -O2 tests/apple_idea_test.cpp -o /tmp/phrase-mix-apple-idea-test
/tmp/phrase-mix-apple-idea-test
```

These checks cover generation and audio-engine behavior. They do not replace listening tests or validation of audio devices and the macOS UI.

## Test track loops

Select a track and set `Loop steps` beside its pattern controls to any value from 1 to 64. For example, use 5 steps on one track and 64 on another: each track repeats independently. When any track is longer than 16 steps, `Page` shows and edits one 16-step section at a time across all tracks. Its default `Follow` setting changes pages with the selected track's playhead; choose a numbered page to hold the view for editing. A page change does not alter playback. Tiles beyond a track's loop are dimmed and retain their notes if you lengthen it again. New pages are empty until you add notes or generate a new pattern. `Note length` remains a separate control for how long each note rings. Change Every still counts 16-step cycles. Loop steps and all pages are saved with Pieces and Keeps and copied by Clone.

## Test AI idea

When the on-device model is available, select a track, press `AI idea` beside its pattern controls, enter a short musical direction, and press `Create idea`. When a valid suggestion arrives, Phrase Mix changes only that track's notes and expression controls. The suggested note count keeps busy model output from filling every step. Use `< Pattern` to return to the previous candidate.

For a broader change, make a Keep first, then press `AI piece` by the tempo controls. It also works from an empty piece. The proposal shows tempo, density, scale, its phrase or drone structure, a sound palette and variation, and any tracks it would add. A phrase request arranges unheld follower phrases around a main motif in Still mode. A drone request makes one to three continuous, connected layers in Drift; an upper layer moves to a nearby scale note every four cycles and glides there, while the low layer anchors it. Switch to Still to freeze the pitches. Extra existing unheld tracks are muted and held rather than removed. `Keep sounds` preserves timbres, while `Refresh built-ins` applies role-specific sounds and starting sound settings to unheld built-in tracks. Selected sample drones use Bed; hosted instruments depend on their own sustain. Plugin effects, previously held tracks, and existing row levels remain. Shared Space and Room may change. `Generate track`, Drift, and Renew remain local C++ generation and work without Apple Intelligence. If the model is unavailable, the AI buttons stay disabled and their tooltips explain why. Availability is checked again while the app remains open.

## Test MIDI output

Choose `MIDI > Send notes to > Phrase Mix virtual port` and `MIDI > Send from > Track 1`. In the receiving music app, select `Phrase Mix` as its MIDI input and load a software instrument. Start Phrase Mix playback. The virtual port carries notes only; the other app plays its own audio through the Mac output. `All tracks` sends tracks 1-8 on channels 1-8. Phrase Mix takes do not include the other app's audio.

To connect separate standalone synths, choose `MIDI > Send notes to > Separate track ports (1-8)`. Each synth selects one input, such as `Phrase Mix Track 1` or `Phrase Mix Track 2`. Each port sends on channel 1. The existing shared port remains available when a host can route channels itself.

## Test external audio return

Use a stereo loopback device such as [BlackHole 2ch](https://github.com/ExistentialAudio/BlackHole). Set the external synth's **audio output** to BlackHole, not Phrase Mix's output. In Phrase Mix, choose `Audio device`, select BlackHole as a two-channel input, and leave output on speakers or headphones. Enable `Return`, adjust its level, then record a take. The take contains Phrase Mix plus the audio arriving at its input. Phrase Mix remembers the selected audio devices, but Return is off on every launch and is not saved with Pieces or Keeps. Do not send Phrase Mix's own output to BlackHole, or it may feed back into itself.

## Test an effect plugin

Select a track and use its two `Track effects` rows to load Audio Unit or VST3 effects. Slot 1 processes audio before slot 2; either can be empty. The upper `Effect plugin` row hosts one effect on the whole mix. In each row, `Choose effect` opens the plugin list, `Scan / manage` finds installed plugins, `Edit` opens that plugin's controls, `Bypass ON` skips that slot, and `Remove` clears it. Track effects run before the shared Space and whole-piece effect; the whole-piece effect runs before monitoring and `Record take`. Save a Piece or Keep to preserve plugin identities and settings. Older saves with one track effect restore it to slot 1. Return remains a separate stereo input monitor for audio from outside Phrase Mix; it is not processed by these effect slots.

## Test a hosted instrument

Select a track, set Kind to `Plugin`, and press `Choose`. Use `Scan / manage` to discover installed AU or VST3 instruments, select one, and press `Load instrument`. Start playback. The track's pattern notes reach the instrument directly even when external MIDI output is Off; its audio passes through both track effects and is included in `Record take`. `Edit` opens the instrument controls. Save a Piece or Keep, then reload it to verify the instrument and its settings return. If the plugin is missing, the track shows `Not loaded` and stays silent until the plugin is available again.

## Test MIDI Learn without hardware

On macOS, compile and run the temporary virtual input in a terminal:

```bash
c++ -std=c++17 tests/midi_virtual_source.cpp -framework CoreMIDI -framework CoreFoundation -o /tmp/phrase-mix-midi-virtual-source
/tmp/phrase-mix-midi-virtual-source
```

In Phrase Mix, choose `MIDI > Input devices > Phrase Mix Test Input`, then `MIDI > Learn control > Track 1 > Level`. Enter `cc 1 7 20` in the terminal to learn CC 7 on channel 1. Send `cc 1 7 30` to see the level stay put, then `cc 1 7 80` and `cc 1 7 100` to see pickup and movement. For a button test, learn `Track 1 > Mute` with `note 1 60 10`, then send `off 1 60` and `note 1 60 5` to toggle it. Enter `quit` to remove the virtual input, and remove its assignments in the MIDI menu when done.
