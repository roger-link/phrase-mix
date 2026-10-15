# Discover 1.0 audit

This is a working audit of the current Phrase Mix implementation against the
Elsewhen brief. The name is still provisional. The goal is to improve the
instrument by listening to focused changes, not to implement the whole brief
at once.

## What exists

- Main is a 16-step phrase. Each active step selects a note from the chosen
  key and scale, weighted by melodic shape, its previous note, and phrase
  position. It does not yet have an explicit motif or harmonic progression.
- Followers draw on Main's active notes or nearest note. Role and Relationship
  affect density, rhythmic alignment, phrase shift, pitch intervals, and
  register. Close and Distant are already more than a note-copy probability.
- The four roles are Shadow, Contrast, Sparse, and Free. They are optional per
  follower; Main has no role selector.
- A selected track can regenerate independently. Its sound and other tracks'
  phrases remain unchanged. Each track has Back/Forward history, bounded to
  24 note arrays.
- Still repeats. Drift changes the phrase every four cycles by default.
  Renew regenerates every four cycles. Hold protects a track from automatic
  changes and the all-track Generate command, but the selected-track Generate
  command can deliberately replace a held track.
- Sound selection is separate from pattern generation. Dynamics changes
  per-note intensity without changing pitch or rhythm. Articulation scales
  note duration; each track currently has one voice, so new notes retrigger
  that track's previous note.
- The selected track can browse Previous/Next or Random within its sound
  family while playback continues. Tried-sound Back/Forward history is
  independent of pattern history, bounded to 24 choices per track, and saved
  with the piece. A short per-track fade surrounds the switch. Captured sample
  selections are matched by name when the library is reloaded.
- Save Piece stores phrases, sound choices, role, Relationship, expression,
  effects, levels, Hold/Mute, histories, solo/selection, and the generator
  state. Keep creates a separate, non-overwriting snapshot with captured
  sounds. Recall opens a new piece, restarts at step one, and plays in Still.
  Neither form saves rendered audio or the exact in-progress voice/effect
  state; Record Take is the option for an identical performance.

## First focused change

Previously, Drift replaced 4-7 steps on every unheld track with mostly
independent material. That could erase the relationship the user had found.
Drift now takes a fresh candidate from the existing Main/follower generator
and admits only one or two changes around a shared phrase region. The Main is
processed first, so follower candidates derive from its current phrase.
Held tracks remain untouched. This is a small experiment in coordinated,
gradual movement, not a claim that the musical result is finished.

## Gaps and risks

- An empty piece now has a Discover button that creates Main and starts
  playback. Once tracks exist, the same control is the transport Play/Stop.
- The instrument is the default view: up to eight track rows, each with its
  current sound, pattern, level, Hold, Mute, and Solo. The selected-track editor
  stays at one position while tracks are added. Capture, takes, and export
  expand from Record / Finish instead of occupying the opening view.
- Track rows use compact stateful Hold/Mute/Solo controls. A level change no
  longer switches the selected track, and Remove track is in the selected-track
  editor instead of beside every sound label.
- Sound browsing is within one family; changing families still uses the Kind
  menu. There is no wider cross-family discovery ranking yet.
- Pattern history captures note arrays, not the role, Relationship, random
  state, or other parameters that produced each candidate. Back restores a
  static phrase, which can evolve again if Drift remains enabled.
- Keeps are listed in a simple menu, but there is no preview buffer, Focus
  mode, timed fades, or plug-in hosting. The row-level faders support simultaneous track
  mixing, but precision, ergonomics, and sound quality still need live testing.
  Those larger features should follow the core discovery workflow.
- The UI currently calls some phrase mutators directly while the audio thread
  can render. Sample-library replacement also crosses that boundary. These
  should be addressed before more complex real-time generation or plug-ins.
  Note selection's temporary vector allocation was removed in this pass.

## Listening gate

Compare the old and new Drift behavior with a held Main and two followers.
Try piano/clarinet, guitar/pad, and a more dissonant pairing. Run each for at
least five minutes, then compare Close, Moderate, and Distant. Listen for
recognizable identity, useful overlap, gradual development, and unwanted
repetition. If the gentler Drift is better, the next musical experiment is
slow register or density movement. If it feels inert, adjust the number and
placement of changes before adding new mathematics.

Meander is a conceptual reference for coordinated parts and time-correlated
variation, not an implementation to copy:
https://github.com/knchaffin/Meander
