#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>

class HelpView final : public juce::Component {
public:
 HelpView(){
  addAndMakeVisible(title);
  title.setFont(juce::Font(juce::FontOptions(23.0f)));
  title.setColour(juce::Label::textColourId,juce::Colour(0xffedf2ee));
  addAndMakeVisible(body);
  body.setMultiLine(true,true);
  body.setReadOnly(true);
  body.setCaretVisible(false);
  body.setScrollbarsShown(true);
  body.setPopupMenuEnabled(false);
  body.setBorder({0,0,0,0});
  body.setIndents(4,4);
  body.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff1b2020));
  body.setColour(juce::TextEditor::textColourId,juce::Colour(0xffdce5df));
  body.setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);
  body.setFont(juce::Font(juce::FontOptions(15.5f)));
  for(size_t i=0;i<nav.size();++i){
   auto& button=nav[i];
   addAndMakeVisible(button);
   button.setButtonText(pages()[i].nav);
   button.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff222a29));
   button.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffb7c7bd));
   button.onClick=[this,i]{showPage(i);};
  }
  showPage(0);
 }

 void paint(juce::Graphics& g) override {
  g.fillAll(juce::Colour(0xff1b2020));
  g.setColour(juce::Colour(0xff222a29));
  g.fillRect(0,0,196,getHeight());
  g.setColour(juce::Colour(0xff3d5147));
  g.fillRect(195,0,1,getHeight());
  g.setColour(juce::Colour(0xff9fc4ab));
  g.setFont(12.0f);
  g.drawText("PHRASE / MIX",20,21,155,20,juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff40564a));
  g.fillRect(18,58,160,1);
 }

 void resized() override {
  for(size_t i=0;i<nav.size();++i)nav[i].setBounds(14,76+static_cast<int>(i)*37,168,31);
  title.setBounds(228,24,getWidth()-256,36);
  body.setBounds(228,78,getWidth()-256,getHeight()-104);
 }

private:
 struct Page {const char* nav;const char* title;const char* text;};
 static const std::array<Page,11>& pages(){
  static const std::array<Page,11> items{{
   {"Why Phrase Mix","An instrument for surprising yourself",R"(Phrase Mix is built for starting without a musical idea. Begin with a phrase, listen, try another sound, add a related part, and notice what the parts do together. The interesting result may be something you would not have planned note by note.

It grew from a workflow for Cancelled Futures: connected sequences in PolyPhase, sounds in separate instruments, and performance and recording in AUM. Phrase Mix brings those activities closer together so a discovery can be revisited without reconstructing settings across several apps. It is deliberately not a timeline editor or a replacement for every part of a recording studio. Its central question is: what happens if you keep listening and following the promising accidents?

The Main track establishes a shared musical environment. Followers transform it rather than behaving like unrelated random sequences. A short Main phrase, a sparse sustained follower, and a more distant rhythmic follower can form a piece through their interaction. The sound you choose for each part can change the meaning of the same notes.

You remain the editor by ear. There is no score to satisfy and no requirement to play an instrument. Browse patterns and sounds while the music runs; Hold what you want to protect. When the whole combination clicks, Keep it. Then use the row levels, Mute, and Solo to perform and listen closely. Record a take when the performance feels right.

The design separates musical pattern, sound, and performance controls so you can change one without losing the others. Save piece is for continuing work. Keep is a non-overwriting snapshot of a discovery. A take is actual recorded audio.)"},
   {"First session","From an empty piece to a take",R"(1. Press Discover. With no tracks, it creates a Main track and begins playback. After that, the same button is Play or Stop.

2. Leave the phrase running and try Kind, Sound, the left/right sound buttons, or Random. These change the selected track's sound without making a new pattern.

3. Add a follower with + Track. Select its row and listen to how it relates to Main. Try Role and Relationship, then Generate track until its pattern works with the first part. Those controls guide newly generated notes; changing them alone does not redraw the current pattern.

4. Add more tracks gradually, up to eight. Use Hold on a pattern you want generation to leave alone. Move row levels to bring parts in and out; Mute and Solo help identify each sound.

5. Press Keep when the combination is worth remembering. It captures the current setup without stopping playback. Save piece when you want an editable saved composition that you can load again.

6. Open Record / Finish. Press Record take, perform with the row levels, then press Stop take. Select the take to listen, trim with Start and End, and Export final when ready.

At slow BPM, try longer Length and Articulation, raise Note tail, use Connect for adjacent notes, or choose a sustained sound or Sample Bed. Fewer notes can make more space for the interaction between tracks.)"},
   {"Top controls","Playback and musical direction",R"(Discover / Play / Stop: Discover creates Main and starts playback when the piece is empty. Otherwise Play and Stop control playback. Stopping does not delete tracks or saved material.

BPM: Playback speed, from 10 to 240. It changes the spacing of steps immediately. At low BPM, note duration and sound decay become especially important.

Density: How likely new generated patterns are to contain notes. It does not add or remove notes from the pattern already playing; generate to hear its effect.

Key and Scale: Set the tonal center and note collection for generation. The choices are Minor pentatonic, Natural minor, and Major. These guide future patterns; they do not transpose notes already present.

Shape: Guides the movement of generated notes: Wander, Rise, Fall, Arch, Still, or Leap. It changes generation, not the current notes by itself.

Behavior: Still repeats the current patterns. Drift makes small changes after every four cycles. Renew generates new patterns after every four cycles. One cycle is a pass through the 16-step grid. Held tracks resist automatic Drift and Renew. The mode does not change which sound plays on a track.

Generate all patterns: Creates new patterns for every unheld track using the current musical controls. Sounds, levels, and effects stay in place. The selected track's Generate track button is narrower: it only changes that track.

Shuffle whole piece: A broad reset for exploration. It changes musical controls, sounds, patterns, and much of the mix, and clears Holds. Use Save piece or Keep before pressing it if you want a reliable way back.

Audio device: Opens output and input selection. Choose an input here before using Capture sound.)"},
   {"Tracks and grid","Building and performing the ensemble",R"(+ Track adds one part, up to eight. Track 1 is Main; later tracks are followers. A new follower is generated from the musical context already playing.

Click a track row or its name button to select it. The detailed controls below the grid then belong to that track. Selecting a row alone does not change its sound or pattern. Remove track asks for confirmation and removes the selected track and its pattern; later tracks move up a row.

The 16 tiles show the current phrase. A lit mark is a note; an outlined tile is the playhead. Click a tile to turn that step on or off. Each track has its own phrase. Manual step changes are added to that track's pattern history.

Each row has its own level slider. Use these as performance faders to reveal, balance, or slowly fade parts while the patterns continue. The level changes loudness, not note velocity variation or the generated pattern.

Hold preserves that track's notes when you use Generate all patterns or when Drift/Renew runs automatically. It does not prevent deliberate Generate track, manual tile edits, or Shuffle whole piece.

Mute silences a row without deleting its pattern or changing its level. Solo lets you hear one row by itself, even if its Mute is on, while preserving the other Mute and level settings. Click Solo again to restore the mix. Solo is an audition tool; it does not alter patterns.)"},
   {"Patterns and sounds","Two independent ways to explore",R"(Pattern browsing affects only the selected track. < Pattern and Pattern > revisit earlier candidates. Generate track creates another candidate using the current Role, Relationship, Key, Scale, Shape, and Density. The other tracks, sound choice, levels, and effects remain in place. The history is bounded to the most recent 24 candidates per track. Generating after stepping backward replaces that track's forward history.

Kind chooses a sound family. Made contains built-in synthesized sounds. Instruments uses mapped acoustic samples. Samples contains captured sounds and bundled samples. Strange contains more unusual mapped sounds.

Sound selects one sound from the chosen family. The small < and > buttons move through that family's sounds; Random tries another sound in that family. They do not regenerate the notes. While playing, the track briefly dips in level during a sound change to avoid a hard transition.

< Tried and Tried > revisit sounds you auditioned on the selected track. Changing Kind or picking from the Sound menu also joins this history. Each track keeps its own recent sound history, separate from its pattern history.

A useful method is to find one pattern and audition many sounds against it, then keep one sound while trying several patterns. The same notes can feel like a different piece when a new source plays them. Save piece and Keep preserve the current selections.)"},
   {"Track shaping","Relationship, timing, and expression",R"(Role appears on followers. Shadow stays near Main, Contrast offsets it, Sparse leaves more space, and Free explores farther. Relationship ranges from Close to Distant and controls how strongly a newly generated follower pattern relates to Main. Distant still retains a shared musical context rather than becoming arbitrary noise. Role and Relationship guide generation; press Generate track to hear a changed pattern.

Dynamics changes variation in note intensity for future notes. It is not the row level. Low values keep attacks more even; high values make them less uniform.

Articulation moves from short, detached notes toward longer, more overlapping notes without replacing the pitch pattern. Length chooses a basic note duration: Step, Quarter, Half, or Whole. The audible result also depends on the sound itself.

Note tail extends a track's release, especially useful for slow music. It does not add notes. A short recorded sample can still end when its audio ends.

Connect joins consecutive active steps without a fresh attack where the sound can sustain. It is legato, not a pitch glide. A gap in the pattern starts a new note, and a short sample may still run out before the next step.

Role and Relationship are about which notes are generated. Dynamics, Articulation, Length, Note tail, and Connect are about how those notes are heard. That separation is useful when a good phrase needs a different performance rather than replacement.)"},
   {"Sound and space","Shaping a track and the whole mix",R"(Space send is per track: how much of that track enters the shared space effect. Space return is for the whole piece: how much of that effect returns to the mix. Turn up send on a particular track to place it deeper in the shared room without washing out every part.

Tone changes the selected track's brightness and harmonic balance. Motion adds movement to that sound. Texture adds subtle variation and stereo movement. These change sound character without changing the note pattern.

The WHOLE PIECE controls apply to the final mix. Space return sets the amount of shared ambience. Warmth adds saturation. Brightness adjusts the overall tonal balance. Room size changes the space effect's decay and sense of depth.

The track's row level is its performance volume. Dynamics is note-to-note intensity variation. Space send is how much of that track feeds the room. These are different controls; they can be adjusted independently while listening.

When a mix feels crowded, reduce a follower's row level, choose a sparser Role and regenerate it, or shorten its Articulation. For a low-BPM piece, longer releases and more room can connect sparse events, but a little silence is often part of the music.)"},
   {"Samples","Playback and sample-window controls",R"(Select Samples in Kind to use captured audio or bundled samples. The Play control then chooses how the sample behaves.

Pitch changes playback speed with the note pitch. Retrigger starts the chosen sample window at its original speed on new notes and can repeat it while the note continues. Bed keeps the sample window running as a sustained layer beneath the pattern. These modes are available only for Samples.

The SAMPLE SHAPE controls work on the selected sample. Start moves the beginning of the playback window. Size sets how much of the remaining sample is used. Fade softens the window edges. Rev plays the window backward. These controls do not edit the source audio file.

Samples taken from a microphone recording or from Turn into sound are stored in the current Piece's Captured Sounds folder. A Keep copies the captured sources into its own snapshot so they can be recalled later. A saved Piece refers to its Piece folder rather than embedding the audio in its JSON setup.

If a short sample will not sustain at slow BPM, try Retrigger or Bed, use a longer source, or leave intentional gaps. Note tail and Connect cannot manufacture audio beyond a short sample's contents.)"},
   {"Pieces and Keeps","Continuing work versus preserving a discovery",R"(Piece name identifies the current workspace and its audio folder. Name the Piece before making captures or takes when possible. Manually changing the name keeps its Keep association, but changes the folder used for future audio; old audio is not moved automatically. New name generates a fresh name and starts a separate Keep group for the current sound.

Save piece writes the current setup so you can load it for further editing. Saved pieces lists those setups. Selecting a saved name only selects it; Load piece actually replaces the current setup. Save before loading if the current changes matter. Remove moves the selected saved setup to macOS Trash after confirmation. Its Captured Sounds, Takes, and Finals remain in the Piece folder.

Keep takes a new, non-overwriting snapshot of the tracks, patterns, settings, and captured sources without stopping playback. Keeps lists This Piece first; All Keeps includes other Pieces and older snapshots. Keeps are still available if a saved Piece setup is removed. Move Keep to Trash asks for confirmation and moves that snapshot and its copied sounds to Trash.

Recalling a Keep creates a new Piece rather than overwriting the Keep. It starts in Still mode so the discovered pattern repeats as captured. The recalled Piece is not automatically saved as a new saved setup; use Save piece when you want to return to that branch later.

In short: Save piece is the editable workspace, Keep is a frozen discovery, a take is recorded audio, and a final is an exported listening file.)"},
   {"Record and finish","From live sound to a final WAV",R"(Record / Finish opens the capture and finishing panel. Done closes it without stopping playback.

Capture sound records the selected audio input or microphone. Press again to stop; the recording is saved as a reusable source in this Piece and put on the selected track when possible. The input meter shows activity. Use last assigns the most recent capture to the selected track. Discard deletes that most recent capture file, so use it only when you do not need the sound.

Record take records the app's stereo output as audio. Press Stop take to finish. Takes are stored in the current Piece's Takes folder. Show takes opens that folder in Finder. The take list lets you select a recording; Play take auditions it, and Reveal finds that file. Older shared recordings can also appear in the list.

In the Manage row, edit the take name and press Rename. Delete asks for confirmation and deletes the take audio file. Turn into sound makes a new, processed copy of the selected take in Captured Sounds and puts it on a track. The original take remains.

For a final, select a take, set Start and End in seconds, and press Export final. The app trims, adds short edge fades, normalizes the level, and writes a new stereo WAV in this Piece's Finals folder. It leaves the original take untouched and reveals the exported file in Finder.

The row level sliders, Mute, Solo, and the whole-piece controls remain playable during a take. A take captures the performance, not just the saved pattern.)"},
   {"Audio and tips","Practical notes",R"(Audio device opens the output and microphone/input configuration. If the app is silent, check the selected output, Play/Stop state, row levels, Mute, Solo, and the macOS output volume. If Capture sound records silence, check the input device and macOS microphone permission.

Changing BPM, row levels, sound character, or effects is audible as you play. Key, Scale, Shape, Density, Role, and Relationship mainly guide the next generated pattern. The current tiles do not change just because you moved one of those controls.

Hold is a pattern safeguard, not a save command. Save piece preserves an editable setup. Keep preserves a separate snapshot and copied captured sources. Record take makes an audio performance. None of these replaces the others.

At slower tempos, give sounds enough duration with Length, Articulation, or Note tail; Connect adjacent notes for legato; and try Bed for longer sample textures. If a row disappears in Solo, click Solo again to restore the mix without changing Mute settings.

If an older Keep does not appear under This Piece because its original Piece has no current save, look under All Keeps. Recalling it is still possible. Files moved to macOS Trash through the Piece or Keep cleanup controls can be recovered there until Trash is emptied.

This Help window can stay open while Phrase Mix plays. Choose a topic on the left; scroll the text on the right.)"}
  }};
  return items;
 }

 void showPage(size_t index){
  selected=index;
  title.setText(pages()[index].title,juce::dontSendNotification);
  body.setText(pages()[index].text,juce::dontSendNotification);
  body.moveCaretToTop(false);
  for(size_t i=0;i<nav.size();++i){
   nav[i].setColour(juce::TextButton::buttonColourId,juce::Colour(i==selected?0xff405b4c:0xff222a29));
   nav[i].setColour(juce::TextButton::textColourOffId,juce::Colour(i==selected?0xfff2f7f2:0xffb7c7bd));
  }
 }

 std::array<juce::TextButton,11> nav;
 juce::Label title;
 juce::TextEditor body;
 size_t selected=0;
};

class HelpWindow final : public juce::DocumentWindow {
public:
 HelpWindow():juce::DocumentWindow("Phrase Mix Help",juce::Colour(0xff1b2020),juce::DocumentWindow::allButtons){
  setUsingNativeTitleBar(true);
  setContentOwned(new HelpView(),true);
  setResizable(true,false);
  setResizeLimits(750,480,1500,1100);
  const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
  const auto area=display!=nullptr?display->userArea:juce::Rectangle<int>(0,0,1000,760);
  centreWithSize(std::clamp(area.getWidth()-100,750,920),std::clamp(area.getHeight()-100,480,700));
  setVisible(true);
 }
 void closeButtonPressed() override {setVisible(false);}
};
