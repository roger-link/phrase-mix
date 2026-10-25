#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "HelpView.h"
#include "MidiMapping.h"
#include "AudioReturn.h"
#include "BackingTrack.h"
#include "PluginEffect.h"
#include "AppleIdea.h"
#include "AIPieceSounds.h"
#include <mutex>

class TrackSelectButton final : public juce::TextButton {
public:
 std::function<void()> onRename;
 void setIdentity(int number,const juce::String& name,const juce::String& sound){
  trackNumber=number;trackName=name;trackSound=sound;
  setButtonText(juce::String(number)+". "+name+". Sound: "+sound);
  repaint();
 }
 void paintButton(juce::Graphics& g,bool highlighted,bool down) override {
  auto background=findColour(buttonColourId);
  if(highlighted||down)background=background.brighter(down?0.12f:0.06f);
  const auto bounds=getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(background);g.fillRoundedRectangle(bounds,4.0f);
  const auto foreground=findColour(textColourOffId);
  g.setColour(foreground.withAlpha(getToggleState()?0.65f:0.4f));g.drawRoundedRectangle(bounds,4.0f,1.0f);
  g.setColour(foreground);g.setFont(juce::Font(juce::FontOptions(12.0f,juce::Font::bold)));
  g.drawText(juce::String(trackNumber)+". "+trackName,8,2,getWidth()-16,14,juce::Justification::centredLeft,true);
  g.setColour(foreground.withAlpha(0.78f));g.setFont(10.5f);
  g.drawText(trackSound,8,16,getWidth()-16,11,juce::Justification::centredLeft,true);
  if(hasKeyboardFocus(true)){g.setColour(juce::Colour(0xffb6dfc5));g.drawRoundedRectangle(bounds.reduced(1.0f),3.0f,1.0f);}
 }
 void mouseDoubleClick(const juce::MouseEvent&) override {
  if(onRename)onRename();
 }
 bool keyPressed(const juce::KeyPress& key) override {
  if(key.getKeyCode()==juce::KeyPress::F2Key&&onRename){onRename();return true;}
  return juce::TextButton::keyPressed(key);
 }
private:
 int trackNumber=0;
 juce::String trackName,trackSound;
};

class MainView final : public juce::AudioAppComponent, private juce::Timer, private juce::ChangeListener {
public:
 MainView(){
  engine.seed(static_cast<uint32_t>(juce::Random::getSystemRandom().nextInt()));
  setSize(1120,760);
  fadeTargets.fill(0.0);
  fadeDurations.fill(12.0);
  audioFormats.registerBasicFormats();
  addAndMakeVisible(play);play.setButtonText("Discover");play.onClick=[this]{if(activeTrackCount==0)addNewTrack();const bool p=!engine.playing.load();engine.playing=p;play.setButtonText(p?"Stop":"Play");};
 addAndMakeVisible(generate);generate.setButtonText("Generate all patterns");generate.onClick=[this]{generateAllTrackCandidates();};
  addAndMakeVisible(randomize);randomize.setButtonText("Shuffle whole piece");randomize.onClick=[this]{randomizeAll();};
  addAndMakeVisible(aiPiece);aiPiece.setButtonText("AI piece");aiPiece.setEnabled(false);aiPiece.setTooltip("Checking Apple Intelligence...");aiPiece.onClick=[this]{if(aiProcess)cancelAppleIdea();else showAppleIdeaPrompt(true);};
  addAndMakeVisible(addTrack);addTrack.setButtonText("+ Track");addTrack.onClick=[this]{addNewTrack();};
  addAndMakeVisible(cloneTrack);cloneTrack.setButtonText("Clone");cloneTrack.onClick=[this]{cloneSelectedTrack();};
  addAndMakeVisible(recordToggle);recordToggle.setButtonText("Record / Finish");recordToggle.setClickingTogglesState(true);recordToggle.onClick=[this]{setRecordPanelOpen(recordToggle.getToggleState());};
  addAndMakeVisible(chooseEffect);chooseEffect.setButtonText("Choose effect");chooseEffect.onClick=[this]{showPluginPicker();};
  addAndMakeVisible(editEffect);editEffect.setButtonText("Edit");editEffect.onClick=[this]{showPluginEditor();};
  addAndMakeVisible(bypassEffect);bypassEffect.setClickingTogglesState(true);
  bypassEffect.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff875347));
  bypassEffect.onClick=[this]{pluginEffect.setBypassed(bypassEffect.getToggleState());refreshPluginControls();};
  addAndMakeVisible(removeEffect);removeEffect.setButtonText("Remove");removeEffect.onClick=[this]{clearPluginEffect();};
  refreshPluginControls();
  for(int effectIndex=0;effectIndex<trackEffectCount;++effectIndex){
   addAndMakeVisible(chooseTrackEffect[effectIndex]);chooseTrackEffect[effectIndex].setButtonText("Choose effect");
   chooseTrackEffect[effectIndex].onClick=[this,effectIndex]{showPluginPicker(selectedTrack,effectIndex);};
   addAndMakeVisible(editTrackEffect[effectIndex]);editTrackEffect[effectIndex].setButtonText("Edit");
   editTrackEffect[effectIndex].onClick=[this,effectIndex]{showTrackPluginEditor(effectIndex);};
   addAndMakeVisible(bypassTrackEffect[effectIndex]);bypassTrackEffect[effectIndex].setClickingTogglesState(true);
   bypassTrackEffect[effectIndex].setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff875347));
   bypassTrackEffect[effectIndex].onClick=[this,effectIndex]{
    if(selectedTrack>=0)trackEffectSlots[selectedTrack][effectIndex].effect->setBypassed(bypassTrackEffect[effectIndex].getToggleState());
    refreshTrackPluginControls();
   };
   addAndMakeVisible(trackEffectActions[effectIndex]);trackEffectActions[effectIndex].setButtonText("...");
   trackEffectActions[effectIndex].onClick=[this,effectIndex]{showTrackEffectMenu(effectIndex);};
  }
  refreshTrackPluginControls();
  addAndMakeVisible(chooseInstrument);chooseInstrument.setButtonText("Choose");chooseInstrument.onClick=[this]{showInstrumentPicker();};
  addAndMakeVisible(editInstrument);editInstrument.setButtonText("Edit");editInstrument.onClick=[this]{showInstrumentEditor();};
  addAndMakeVisible(removeInstrument);removeInstrument.setButtonText("Remove");removeInstrument.onClick=[this]{clearInstrument(selectedTrack);};
  refreshInstrumentControls();
  addAndMakeVisible(chooseBacking);chooseBacking.setButtonText("Choose audio...");chooseBacking.onClick=[this]{chooseBackingAudio();};
  addAndMakeVisible(backingTiming);backingTiming.setButtonText("Timing...");backingTiming.onClick=[this]{if(selectedTrack>=0)showAddBackingDialog(backingFiles[selectedTrack],selectedTrack);};
  chooseBacking.setTooltip("Load a complete WAV, AIFF, or supported audio file as a stereo backing track.");
  backingTiming.setTooltip("Enter or restore the recording's original BPM and starting step. This does not stretch audio.");
  addAndMakeVisible(monitorReturn);monitorReturn.setButtonText("Return");monitorReturn.setClickingTogglesState(true);
  monitorReturn.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff3e6851));
  monitorReturn.onClick=[this]{setAudioReturnEnabled(monitorReturn.getToggleState());};
  setupSlider(returnLevel,0,100,70,[this]{audioReturnGain=static_cast<float>(returnLevel.getValue()/100.0);});
  returnLevel.setTextBoxStyle(juce::Slider::TextBoxRight,false,36,22);
  addAndMakeVisible(startFade);startFade.setButtonText("Start fade");startFade.onClick=[this]{
   if(selectedTrack<0||selectedTrack>=activeTrackCount)return;
  if(!engine.playing.load()){status="Start playback before fading";repaint();return;}
   midiMappings.resetPickup(MidiMapping::Target::Level,selectedTrack);
   engine.beginTrackFade(selectedTrack,static_cast<float>(fadeTarget.getValue()/100.0),static_cast<float>(fadeSeconds.getValue()));
   status="Fading track "+juce::String(selectedTrack+1)+" to "+juce::String(static_cast<int>(fadeTarget.getValue()))+"%";
   repaint();
  };
  addAndMakeVisible(cancelFade);cancelFade.setButtonText("Cancel");cancelFade.onClick=[this]{
   if(selectedTrack<0||selectedTrack>=activeTrackCount)return;
   engine.cancelTrackFade(selectedTrack);
   status="Fade stopped on track "+juce::String(selectedTrack+1);
   repaint();
  };
  addAndMakeVisible(recordDone);recordDone.setButtonText("Done");recordDone.onClick=[this]{setRecordPanelOpen(false);};
  addAndMakeVisible(patternBack);patternBack.setButtonText("< Pattern");patternBack.onClick=[this]{restorePatternStep(-1);};
  addAndMakeVisible(trackGenerate);trackGenerate.setButtonText("Generate track");trackGenerate.onClick=[this]{generateSelectedTrackCandidate();};
  addAndMakeVisible(aiIdea);aiIdea.setButtonText("AI idea");aiIdea.setEnabled(false);aiIdea.setTooltip("Checking Apple Intelligence...");aiIdea.onClick=[this]{if(aiProcess)cancelAppleIdea();else showAppleIdeaPrompt();};
  addAndMakeVisible(trackActions);trackActions.setButtonText("Track");trackActions.onClick=[this]{showTrackActionsMenu();};
  addAndMakeVisible(stepPage);
  stepPage.addItem("Follow",1);
  for(int page=0;page<Engine::maxLoopSteps/Engine::steps;++page)stepPage.addItem(juce::String(page+1),page+2);
  stepPage.setSelectedId(1,juce::dontSendNotification);
  stepPage.setTooltip("Follow the selected track's playhead, or hold a page for editing. This does not change playback.");
  stepPage.onChange=[this]{repaint();};
  addAndMakeVisible(patternForward);patternForward.setButtonText("Pattern >");patternForward.onClick=[this]{restorePatternStep(1);};
  addAndMakeVisible(soundPrevious);soundPrevious.setButtonText("<");soundPrevious.onClick=[this]{browseSound(-1);};
  addAndMakeVisible(soundNext);soundNext.setButtonText(">");soundNext.onClick=[this]{browseSound(1);};
  addAndMakeVisible(soundRandom);soundRandom.setButtonText("Random");soundRandom.onClick=[this]{randomSound();};
  addAndMakeVisible(soundBack);soundBack.setButtonText("<");soundBack.onClick=[this]{stepSoundHistory(-1);};
  addAndMakeVisible(soundForward);soundForward.setButtonText(">");soundForward.onClick=[this]{stepSoundHistory(1);};
  addAndMakeVisible(help);help.setButtonText("Help");help.onClick=[this]{showHelp();};
  addAndMakeVisible(keep);keep.setButtonText("Keep");keep.onClick=[this]{saveKeep();};
  addAndMakeVisible(keeps);keeps.setButtonText("Keeps");keeps.onClick=[this]{showKeepsMenu();};
  addAndMakeVisible(recordSound);recordSound.setButtonText("Capture sound");recordSound.onClick=[this]{toggleSoundRecording();};
  addAndMakeVisible(recordMix);recordMix.setButtonText("Record take");recordMix.onClick=[this]{toggleMixRecording();};
  addAndMakeVisible(showRecordings);showRecordings.setButtonText("Show takes");showRecordings.onClick=[this]{showRecordingsFolder();};
  addAndMakeVisible(takeList);takeList.onChange=[this]{selectTakeFromList();};
  addAndMakeVisible(playTake);playTake.setButtonText("Play take");playTake.onClick=[this]{toggleTakePlayback();};
  addAndMakeVisible(revealTake);revealTake.setButtonText("Reveal");revealTake.onClick=[this]{revealSelectedTake();};
  addAndMakeVisible(takeName);takeName.setSelectAllWhenFocused(true);takeName.onReturnKey=[this]{renameSelectedTake();};
  addAndMakeVisible(renameTake);renameTake.setButtonText("Rename");renameTake.onClick=[this]{renameSelectedTake();};
  addAndMakeVisible(deleteTake);deleteTake.setButtonText("Delete");deleteTake.onClick=[this]{confirmDeleteSelectedTake();};
  addAndMakeVisible(addTakeBacking);addTakeBacking.setButtonText("Add backing track");addTakeBacking.onClick=[this]{showAddBackingDialog(selectedTakeFile());};
  addAndMakeVisible(useTakeSound);useTakeSound.setButtonText("Turn into sound");useTakeSound.onClick=[this]{useSelectedTakeAsSound();};
  addAndMakeVisible(exportFinal);exportFinal.setButtonText("Export final");exportFinal.onClick=[this]{exportFinalTake();};
  addAndMakeVisible(useLastSound);useLastSound.setButtonText("Use last");useLastSound.onClick=[this]{useLastRecording();};
  addAndMakeVisible(discardSound);discardSound.setButtonText("Discard");discardSound.onClick=[this]{discardLastRecording();};
  addAndMakeVisible(sceneName);sceneName.setText(makeSceneName(),juce::dontSendNotification);sceneName.setSelectAllWhenFocused(true);
  addAndMakeVisible(trackNameEditor);trackNameEditor.setVisible(false);trackNameEditor.setSelectAllWhenFocused(true);trackNameEditor.setInputRestrictions(48);
  trackNameEditor.onReturnKey=[this]{finishRenameTrack(true);};
  trackNameEditor.onEscapeKey=[this]{finishRenameTrack(false);};
  trackNameEditor.onFocusLost=[this]{finishRenameTrack(true);};
  addAndMakeVisible(pieceActions);pieceActions.setButtonText("Piece");pieceActions.onClick=[this]{showPieceActionsMenu();};
  addAndMakeVisible(save);save.setButtonText("Save piece");save.onClick=[this]{saveScene();};
  addAndMakeVisible(midi);midi.setButtonText("MIDI");midi.onClick=[this]{showMidiMenu();};
  addAndMakeVisible(devices);devices.setButtonText("Audio device");devices.onClick=[this]{
   auto content=std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,0,2,1,2,false,false,true,false);content->setSize(480,390);
   juce::DialogWindow::LaunchOptions options;options.dialogTitle="Audio settings";options.dialogBackgroundColour=juce::Colour(0xff222727);options.content.setOwned(content.release());options.useNativeTitleBar=true;options.resizable=false;options.launchAsync();
  };
  setupSlider(tempo,10,240,96,[this]{engine.bpm=static_cast<float>(tempo.getValue());});
  tempo.setRange(10,240,0.001);tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,66,24);
  tempo.setSkewFactorFromMidPoint(70);
  setupSlider(density,20,100,65,[this]{engine.density=static_cast<float>(density.getValue()/100);});
  addAndMakeVisible(key);const char* keys[]={"C","C♯","D","E♭","E","F","F♯","G","A♭","A","B♭","B"};for(int i=0;i<12;++i)key.addItem(keys[i],i+1);key.setSelectedId(3);key.onChange=[this]{engine.key=key.getSelectedId()-1;};
  addAndMakeVisible(scale);scale.addItem("Minor pentatonic",1);scale.addItem("Natural minor",2);scale.addItem("Major",3);scale.setSelectedId(1);scale.onChange=[this]{engine.scale=scale.getSelectedId()-1;};
  addAndMakeVisible(shape);shape.addItem("Wander",1);shape.addItem("Rise",2);shape.addItem("Fall",3);shape.addItem("Arch",4);shape.addItem("Still",5);shape.addItem("Leap",6);shape.setSelectedId(1);shape.onChange=[this]{engine.shape=shape.getSelectedId()-1;};
  addAndMakeVisible(mode);mode.addItem("Still",1);mode.addItem("Drift",2);mode.addItem("Renew",3);mode.setSelectedId(1);mode.onChange=[this]{engine.mode=mode.getSelectedId()-1;changeEvery.setEnabled(mode.getSelectedId()!=1);};
  setupSlider(changeEvery,1,16,4,[this]{engine.every=static_cast<int>(changeEvery.getValue());});
  changeEvery.setEnabled(false);
  for(int i=0;i<Engine::maxTracks;++i){
   addAndMakeVisible(trackSelect[i]);trackSelect[i].setClickingTogglesState(false);trackSelect[i].setButtonText(juce::String(i+1));trackSelect[i].onClick=[this,i]{selectTrack(i);revealSelectedTrackInspector();};trackSelect[i].onRename=[this,i]{beginRenameTrack(i);};
  }
  for(int i=0;i<Engine::maxTracks;++i){
   addAndMakeVisible(kinds[i]);
   kinds[i].addItem("Made",1);kinds[i].addItem("Instruments",2);kinds[i].addItem("Samples",3);kinds[i].addItem("Strange",4);kinds[i].addItem("Plugin",5);kinds[i].addItem("Backing",6);
   kinds[i].setSelectedId(i==2?4:(i==3?3:1));
   kinds[i].onChange=[this,i]{refreshSoundMenu(i,false,false);queueSoundChoice(i,true);};
   addAndMakeVisible(sounds[i]);
   sounds[i].onChange=[this,i]{queueSoundChoice(i,true);};
   addAndMakeVisible(sources[i]);
   sources[i].addItem("Pluck",1);sources[i].addItem("Low",2);sources[i].addItem("Ring",3);sources[i].addItem("Cloud",4);sources[i].addItem("Glow",5);sources[i].addItem("Reed",6);sources[i].addItem("Dust",7);sources[i].addItem("Bell",8);sources[i].addItem("Library",9);sources[i].addItem("Guitar",10);sources[i].addItem("Violin",11);sources[i].addItem("Bass",12);sources[i].addItem("Keys",13);sources[i].addItem("Instrument",14);
   sources[i].setSelectedId(i+1);
   sources[i].onChange=[this,i]{engine.sourceType[i]=sources[i].getSelectedId()-1;resized();repaint();};
   addAndMakeVisible(materials[i]);
	   materials[i].addItem("Wood",1);materials[i].addItem("Wire",2);materials[i].addItem("Glass",3);materials[i].addItem("Tape",4);materials[i].addItem("Air",5);materials[i].addItem("Metal",6);materials[i].addItem("Organ",7);
	   materials[i].setSelectedId(i==2?6:(i==3?4:i+1));
	   materials[i].onChange=[this,i]{engine.material[i]=materials[i].getSelectedId()-1;};
	   addAndMakeVisible(playModes[i]);
	   playModes[i].addItem("Pitch",1);playModes[i].addItem("Retrigger",2);playModes[i].addItem("Bed",3);
	   playModes[i].setSelectedId(1);
	   playModes[i].onChange=[this,i]{engine.samplePlayMode[i]=playModes[i].getSelectedId()-1;};
	   setupSlider(sampleStarts[i],0,100,0,[this,i]{engine.sampleStart[i]=static_cast<float>(sampleStarts[i].getValue()/100);});
	   setupSlider(sampleSizes[i],2,100,100,[this,i]{engine.sampleSize[i]=static_cast<float>(sampleSizes[i].getValue()/100);});
	   setupSlider(sampleFades[i],0,45,5,[this,i]{engine.sampleFade[i]=static_cast<float>(sampleFades[i].getValue()/100);});
	   addAndMakeVisible(sampleReverses[i]);sampleReverses[i].setButtonText("Rev");sampleReverses[i].onClick=[this,i]{engine.sampleReverse[i]=sampleReverses[i].getToggleState();};
	   addAndMakeVisible(librarySounds[i]);
   librarySounds[i].addItem("All sounds",1);librarySounds[i].setSelectedId(1);
   librarySounds[i].onChange=[this,i]{engine.sampleChoice[i]=librarySounds[i].getSelectedId()-2;};
   addAndMakeVisible(instruments[i]);
   instruments[i].addItem("Bass map",1);instruments[i].addItem("Guitar map",2);instruments[i].addItem("Harmonics map",3);instruments[i].addItem("Reed ghost",4);
   instruments[i].addItem("Flute map",5);instruments[i].addItem("Oboe map",6);instruments[i].addItem("Bassoon map",7);instruments[i].addItem("Recorder haze",8);instruments[i].addItem("Pipe dusk",9);
   instruments[i].setSelectedId(i==1?1:2);
   instruments[i].onChange=[this,i]{engine.instrumentChoice[i]=instruments[i].getSelectedId()-1;};
   addAndMakeVisible(roles[i]);
   roles[i].addItem("Shadow",1);roles[i].addItem("Contrast",2);roles[i].addItem("Sparse",3);roles[i].addItem("Free",4);
   roles[i].setSelectedId(i==0?1:(i%4)+1);
   roles[i].onChange=[this,i]{engine.role[i]=roles[i].getSelectedId()-1;trackSubtitles[i]=i==0?"Main phrase":roleSubtitle(roles[i].getSelectedId()-1);repaint();};
   addAndMakeVisible(lengths[i]);
   lengths[i].addItem("Step",1);lengths[i].addItem("Quarter",2);lengths[i].addItem("Half",3);lengths[i].addItem("Whole",4);
   lengths[i].setSelectedId(i==0?2:(i==1?1:3));
   lengths[i].onChange=[this,i]{engine.noteLength[i]=lengths[i].getSelectedId()-1;};
   addAndMakeVisible(loopLengths[i]);
   loopLengths[i].setRange(1,Engine::maxLoopSteps,1);
   loopLengths[i].setSliderStyle(juce::Slider::IncDecButtons);
   loopLengths[i].setTextBoxStyle(juce::Slider::TextBoxLeft,false,40,24);
   loopLengths[i].setValue(Engine::steps,juce::dontSendNotification);
   loopLengths[i].onValueChange=[this,i]{rememberPattern(i,true);engine.setLoopSteps(i,static_cast<int>(loopLengths[i].getValue()));syncRhythmControls(i);rememberPattern(i,true);resized();repaint();};
   addAndMakeVisible(rhythms[i]);rhythms[i].setName("Rhythm");rhythmHits[i].setName("Hits");rhythmRotations[i].setName("Rotate");rhythms[i].addItem("Free",1);rhythms[i].addItem("Euclidean",2);
   rhythms[i].setSelectedId(1,juce::dontSendNotification);
   rhythms[i].setTooltip("Free uses Density and follower timing. Euclidean spreads an exact number of hits across this track's loop.");
   rhythms[i].onChange=[this,i]{changeRhythm(i);};
   for(auto* control:{&rhythmHits[i],&rhythmRotations[i]}){
    addAndMakeVisible(*control);control->setSliderStyle(juce::Slider::IncDecButtons);
    control->setTextBoxStyle(juce::Slider::TextBoxLeft,false,40,24);
   }
   rhythmHits[i].setTooltip("Exact number of hits, including zero for silence. Changing this reshapes the rhythm while keeping its melodic notes.");
   rhythmRotations[i].setTooltip("Move every hit right by this many steps, wrapping around the loop. Notes move with their hits.");
   syncRhythmControls(i);
   rhythmHits[i].onValueChange=[this,i]{changeRhythm(i);};
   rhythmRotations[i].onValueChange=[this,i]{changeRhythm(i);};
   auto setupRowToggle=[](juce::TextButton& button,const juce::String& title,juce::Colour active){
    button.setButtonText(title);button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff27302f));
    button.setColour(juce::TextButton::buttonOnColourId,active);
    button.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffc1cbc5));
    button.setColour(juce::TextButton::textColourOnId,juce::Colour(0xfff5f8f5));
   };
   addAndMakeVisible(backingLoops[i]);backingLoops[i].setButtonText("Loop recording");backingLoops[i].onClick=[this,i]{backingLoopFlags[i]=backingLoops[i].getToggleState();};
   backingLoops[i].setTooltip("Repeat the complete recording at its original speed. Off plays it once from the beginning.");
   addAndMakeVisible(holds[i]);setupRowToggle(holds[i],"Hold",juce::Colour(0xff3e6851));holds[i].onClick=[this,i]{engine.hold[i]=holds[i].getToggleState();engine.refreshFollowers=true;repaint();};
   addAndMakeVisible(mutes[i]);setupRowToggle(mutes[i],"Mute",juce::Colour(0xff765047));mutes[i].onClick=[this,i]{engine.mute[i]=mutes[i].getToggleState();repaint();};
   addAndMakeVisible(solos[i]);setupRowToggle(solos[i],"Solo",juce::Colour(0xff3f6575));solos[i].onClick=[this,i]{setSoloTrack(solos[i].getToggleState()?i:-1);};
   addAndMakeVisible(focuses[i]);setupRowToggle(focuses[i],"Focus",juce::Colour(0xff566c48));focuses[i].onClick=[this,i]{setFocusTrack(focuses[i].getToggleState()?i:-1);};
   addAndMakeVisible(connects[i]);connects[i].setButtonText("Connect");connects[i].onClick=[this,i]{engine.connect[i]=connects[i].getToggleState();};
   setupSlider(levels[i],0,100,65,[this,i]{setManualTrackLevel(i,levels[i].getValue());});
   setupSlider(rowLevels[i],0,100,65,[this,i]{setManualTrackLevel(i,rowLevels[i].getValue());});
   rowLevels[i].setTextBoxStyle(juce::Slider::TextBoxRight,false,38,22);
   setupSlider(sends[i],0,100,i==3?65:25,[this,i]{engine.send[i]=static_cast<float>(sends[i].getValue()/100);});
   setupSlider(colors[i],0,100,i==2?78:55,[this,i]{engine.color[i]=static_cast<float>(colors[i].getValue()/100);});
   setupSlider(moves[i],0,100,i==3?72:35,[this,i]{engine.move[i]=static_cast<float>(moves[i].getValue()/100);});
   setupSlider(weathers[i],0,100,i==3?64:32,[this,i]{engine.weather[i]=static_cast<float>(weathers[i].getValue()/100);});
   setupSlider(relationships[i],0,100,i==0?0:35+(i%3)*12,[this,i]{engine.relationship[i]=static_cast<float>(relationships[i].getValue()/100);if(!midiDispatching)midiMappings.resetPickup(MidiMapping::Target::Relationship,i);});
   setupSlider(dynamics[i],0,100,i==0?18:28+(i%4)*8,[this,i]{engine.dynamics[i]=static_cast<float>(dynamics[i].getValue()/100);if(!midiDispatching)midiMappings.resetPickup(MidiMapping::Target::Dynamics,i);});
   setupSlider(articulations[i],0,100,i==0?42:(i==1?68:52),[this,i]{engine.articulation[i]=static_cast<float>(articulations[i].getValue()/100);if(!midiDispatching)midiMappings.resetPickup(MidiMapping::Target::Articulation,i);});
   setupSlider(tails[i],0,100,0,[this,i]{engine.tail[i]=static_cast<float>(tails[i].getValue()/100);});
   refreshSoundMenu(i,false);
  }
  setupSlider(room,0,100,60,[this]{engine.room=static_cast<float>(room.getValue()/100);});
  setupSlider(wet,0,100,25,[this]{engine.wet=static_cast<float>(wet.getValue()/100);});
  setupSlider(drive,0,100,18,[this]{engine.drive=static_cast<float>(drive.getValue()/100);});
  setupSlider(filter,0,100,72,[this]{engine.filter=static_cast<float>(filter.getValue()/100);});
  addAndMakeVisible(focusFade);focusFade.setRange(0.1,8.0,0.1);focusFade.setSliderStyle(juce::Slider::LinearHorizontal);focusFade.setTextBoxStyle(juce::Slider::TextBoxRight,false,42,24);focusFade.setValue(1.5);focusFade.onValueChange=[this]{engine.focusFadeSeconds=static_cast<float>(focusFade.getValue());};
  setupSlider(fadeTarget,0,100,0,[this]{if(selectedTrack>=0)fadeTargets[selectedTrack]=fadeTarget.getValue();});
  addAndMakeVisible(fadeSeconds);fadeSeconds.setRange(0.5,120.0,0.5);fadeSeconds.setSkewFactorFromMidPoint(12.0);fadeSeconds.setSliderStyle(juce::Slider::LinearHorizontal);fadeSeconds.setTextBoxStyle(juce::Slider::TextBoxRight,false,50,24);fadeSeconds.setValue(12.0);fadeSeconds.onValueChange=[this]{if(selectedTrack>=0)fadeDurations[selectedTrack]=fadeSeconds.getValue();};
  setupSlider(finalStart,0,1,0,[this]{clampFinalTrim(true);});
  setupSlider(finalEnd,0,1,1,[this]{clampFinalTrim(false);});
  setupHelp();
  loadMidiSettings();
  refreshMidiInputs();
  refreshMidiOutput();
  migrateSingleSceneIfNeeded();
  loadSampleLibrary();
  refreshTakeList(false);
  if(auto plugins=juce::XmlDocument::parse(appDataDirectory().getChildFile("Effects.xml")))pluginEffect.known.recreateFromXml(*plugins);
  pluginEffect.known.addChangeListener(this);
  resized();
  const auto audioState=juce::XmlDocument::parse(appDataDirectory().getChildFile("AudioDevice.xml"));
  setAudioChannels(1,2,audioState.get());
  deviceManager.addChangeListener(this);
  deviceManager.addAudioCallback(&captureTap);
  checkAppleIdeaAvailability();
  startTimerHz(30);
 }
 ~MainView() override {stopTimer();if(aiProcess)aiProcess->kill();aiProcess.reset();if(aiAvailabilityProcess)aiAvailabilityProcess->kill();aiAvailabilityProcess.reset();pluginPickerWindow.reset();pluginEffect.known.removeChangeListener(this);deviceManager.removeChangeListener(this);midiInputs.clear();deviceManager.removeAudioCallback(&captureTap);recordingMix=false;shutdownAudio();pluginEditorWindow.reset();pluginEffect.clear();for(auto& chain:trackEffectSlots)for(auto& slot:chain){slot.editor.reset();slot.effect->clear();}for(auto& slot:trackInstruments){slot.editor.reset();slot.effect->clear();}panicMidiOutput();midiOutput.reset();for(auto& output:trackMidiOutputs)output.reset();mixWriter.reset();recordWriterThread.stopThread(5000);}
 void prepareToPlay(int samplesPerBlock,double rate) override {
  engine.prepare(rate,samplesPerBlock);captureSampleRate.store(rate);midiBlock.ensureSize(8192);for(auto& block:trackMidiBlocks)block.ensureSize(2048);
  returnBuffer.setSize(2,std::max(1,samplesPerBlock),false,false,true);
  backingAudio.prepare(samplesPerBlock);
  pluginEffect.prepare(rate,samplesPerBlock);
  for(auto& chain:trackEffectSlots)for(auto& slot:chain)slot.effect->prepare(rate,samplesPerBlock);
  for(auto& slot:trackInstruments)slot.effect->prepare(rate,samplesPerBlock);
  effectBlockSize=samplesPerBlock;
  const auto* device=deviceManager.getCurrentAudioDevice();
  activeInputChannels=device!=nullptr?device->getActiveInputChannels().countNumberOfSetBits():0;
  if(activeInputChannels.load()<2)audioReturnEnabled=false;
 }
 void releaseResources() override {pluginEffect.release();for(auto& chain:trackEffectSlots)for(auto& slot:chain)slot.effect->release();for(auto& slot:trackInstruments)slot.effect->release();}
 void changeListenerCallback(juce::ChangeBroadcaster* source) override {
  if(source==&pluginEffect.known){
   const auto file=appDataDirectory().getChildFile("Effects.xml");
   file.getParentDirectory().createDirectory();
   if(auto xml=pluginEffect.known.createXml())file.replaceWithText(xml->toString());
   return;
  }
  if(source!=&deviceManager)return;
  const auto file=appDataDirectory().getChildFile("AudioDevice.xml");
  if(const auto state=deviceManager.createStateXml()){
   file.getParentDirectory().createDirectory();
   if(!file.replaceWithText(state->toString()))status="Could not save audio device setup";
  }else if(file.existsAsFile())file.deleteFile();
 }
 void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
  if(!info.buffer||info.buffer->getNumChannels()==0)return;
  const bool addReturn=audioReturnEnabled.load()&&activeInputChannels.load()>=2&&info.buffer->getNumChannels()>=2;
  if(addReturn){
   if(returnBuffer.getNumSamples()<info.numSamples)returnBuffer.setSize(2,info.numSamples,false,false,true);
   for(int channel=0;channel<2;++channel)returnBuffer.copyFrom(channel,0,*info.buffer,channel,info.startSample,info.numSamples);
  }
  Engine::MidiEvents midiEvents;
  bool hasTrackEffect=engine.hostedInstrumentMask.load()!=0;
  if(info.buffer->getNumChannels()>1)for(int track=0;track<activeTrackCount;++track)
   for(const auto& slot:trackEffectSlots[track])
    hasTrackEffect=hasTrackEffect||(slot.effect->hasPlugin()&&!slot.effect->isBypassed());
  engine.render(info.buffer->getWritePointer(0,info.startSample),info.buffer->getNumChannels()>1?info.buffer->getWritePointer(1,info.startSample):nullptr,info.numSamples,&midiEvents,
   hasTrackEffect?[](void* context,int track,float* left,float* right,int count,const Engine::MidiEvents& events){
    auto* view=static_cast<MainView*>(context);
    if(view->engine.hostedInstrumentMask.load()&(1u<<track)){
     auto& notes=view->hostedMidiBlocks[static_cast<size_t>(track)];notes.clear();
     for(int i=0;i<events.count;++i){const auto& event=events.items[static_cast<size_t>(i)];if(event.track==track)
      notes.addEvent(event.on?juce::MidiMessage::noteOn(1,event.note,static_cast<juce::uint8>(event.velocity)):juce::MidiMessage::noteOff(1,event.note),event.sample);}
     view->trackInstruments[track].effect->process(left,right,count,&notes);
    }
    if(!(view->engine.backingTrackMask.load()&(1u<<track)))for(auto& slot:view->trackEffectSlots[track])slot.effect->process(left,right,count);
   }:nullptr,this);
  if(info.buffer->getNumChannels()>1)
   pluginEffect.process(info.buffer->getWritePointer(0,info.startSample),info.buffer->getWritePointer(1,info.startSample),info.numSamples);
  if(midiOutput!=nullptr&&midiEvents.count>0){
   midiBlock.clear();
   const int route=engine.midiOutputTrack.load();
   for(int i=0;i<midiEvents.count;++i){
    const auto& event=midiEvents.items[static_cast<size_t>(i)];
    if(route==-2||(route>=0&&route!=event.track))continue;
    const int channel=event.track+1;
    midiBlock.addEvent(event.on?juce::MidiMessage::noteOn(channel,event.note,static_cast<juce::uint8>(event.velocity)):juce::MidiMessage::noteOff(channel,event.note),event.sample);
   }
   if(!midiBlock.isEmpty())midiOutput->sendBlockOfMessages(midiBlock,juce::Time::getMillisecondCounterHiRes()+10.0,captureSampleRate.load());
  }else if(trackMidiOutputs[0]!=nullptr&&midiEvents.count>0){
   for(auto& block:trackMidiBlocks)block.clear();
   for(int i=0;i<midiEvents.count;++i){
    const auto& event=midiEvents.items[static_cast<size_t>(i)];
    trackMidiBlocks[static_cast<size_t>(event.track)].addEvent(event.on?juce::MidiMessage::noteOn(1,event.note,static_cast<juce::uint8>(event.velocity)):juce::MidiMessage::noteOff(1,event.note),event.sample);
   }
   const double start=juce::Time::getMillisecondCounterHiRes()+10.0;
   for(int track=0;track<Engine::maxTracks;++track)if(trackMidiOutputs[static_cast<size_t>(track)]!=nullptr&&!trackMidiBlocks[static_cast<size_t>(track)].isEmpty()){
    trackMidiOutputs[static_cast<size_t>(track)]->sendBlockOfMessages(trackMidiBlocks[static_cast<size_t>(track)],start,captureSampleRate.load());
   }
  }
  std::array<BackingTracks::Controls,Engine::maxTracks> backingControls{};
  const auto backingMask=engine.backingTrackMask.load();
  const int backingSolo=engine.soloTrack.load(),backingFocus=backingSolo>=0?-1:engine.focusTrack.load();
  for(int track=0;track<Engine::maxTracks;++track){
   auto& control=backingControls[track];control.active=(backingMask&(1u<<track))!=0;
   control.loop=backingLoopFlags[track].load();control.level=engine.level[track].load();
   control.audible=backingSolo>=0?backingSolo==track:!engine.mute[track].load();
   control.focused=backingFocus<0||backingFocus==track;
  }
  backingAudio.render(info.buffer->getWritePointer(0,info.startSample),info.buffer->getNumChannels()>1?info.buffer->getWritePointer(1,info.startSample):nullptr,
   info.numSamples,engine.blockStartSeconds,captureSampleRate.load(),engine.blockPlaying,engine.blockRestarted,backingControls,engine.focusFadeSeconds.load(),
   [](void* context,int track,float* left,float* right,int count){auto* view=static_cast<MainView*>(context);for(auto& slot:view->trackEffectSlots[track])slot.effect->process(left,right,count);},this);
  renderTakePlayback(info);
  if(addReturn){
   const float peak=mixStereoReturn(info.buffer->getWritePointer(0,info.startSample),info.buffer->getWritePointer(1,info.startSample),returnBuffer.getReadPointer(0),returnBuffer.getReadPointer(1),info.numSamples,audioReturnGain.load());
   audioReturnPeak.store(std::max(audioReturnPeak.load(),peak));
  }
  for(int c=2;c<info.buffer->getNumChannels();++c)info.buffer->clear(c,info.startSample,info.numSamples);
  mixWriterInCallback.store(true);
  if(recordingMix.load()){
   recordingTiming.observe(engine.blockTempo,engine.blockStartStep,engine.blockPlaying,info.numSamples,mixRecordSampleRate);
   const float* left=info.buffer->getReadPointer(0,info.startSample);
   const float* right=info.buffer->getNumChannels()>1?info.buffer->getReadPointer(1,info.startSample):left;
   const float* channels[]{left,right};
   if(mixWriter!=nullptr&&mixWriter->write(channels,info.numSamples))mixRecordedFrames.fetch_add(info.numSamples);
   else mixDroppedBlocks.fetch_add(1);
  }
  mixWriterInCallback.store(false);
 }
 void paint(juce::Graphics& g) override {
  g.fillAll(juce::Colour(0xff171b1b));g.setColour(juce::Colour(0xffe9eeeb));g.setFont(20.f);g.drawText("PHRASE / MIX",24,14,300,30,juce::Justification::centredLeft);
  label(g,"BPM",145,62);label(g,"Density",390,62);label(g,"Key",24,112);label(g,"Scale",110,112);label(g,"Shape",310,112);label(g,"Behavior",475,112);label(g,"Piece name",660,112);
  g.setColour(juce::Colour(0xffa5b2ad));g.setFont(12.f);g.drawText("Focus fade",365,169,98,18,juce::Justification::centredLeft);g.drawText("Change every (cycles)",475,169,170,18,juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff1e2423));g.fillRect(0,wholePieceTop,getWidth(),134);
  g.setColour(juce::Colour(0xff3b4943));g.fillRect(24,wholePieceTop,getWidth()-48,1);
  g.setColour(juce::Colour(0xff94bba4));g.setFont(11.f);g.drawText("WHOLE PIECE",24,wholePieceTop+5,160,14,juce::Justification::centredLeft);
  label(g,"Effect plugin",24,294);label(g,"External audio",748,294);label(g,"Return level",890,294);
  g.setColour(juce::Colour(0xffdce5df));g.setFont(13.f);
  g.drawFittedText(pluginEffect.hasPlugin()?pluginEffect.name():pendingEffectName.isNotEmpty()?"Not loaded: "+pendingEffectName:"No effect",
   157,318,274,28,juce::Justification::centredLeft,1);
  g.setColour(juce::Colour(0xff304039));g.fillRoundedRectangle(1090.f,317.f,4.f,20.f,2.f);
  g.setColour(juce::Colour(0xff91c4a2));g.fillRoundedRectangle(1090.f,337.f-20.f*std::clamp(returnMeterLevel,0.0f,1.0f),4.f,20.f*std::clamp(returnMeterLevel,0.0f,1.0f),2.f);
  label(g,"Space return",48,351);label(g,"Warmth",208,351);label(g,"Brightness",368,351);label(g,"Room size",528,351);
  if(status.isNotEmpty()){g.setColour((recordingSound.load()||recordingMix.load())?juce::Colour(0xffff8f7a):juce::Colour(0xffb6dfc5));g.setFont(12.f);g.drawFittedText(status,650,192,getWidth()-675,28,juce::Justification::centredLeft,2);}
  if(recordPanelOpen){
   const int captureY=recordPanelY();
   g.setColour(juce::Colour(0xff222727));g.fillRoundedRectangle(24.f,static_cast<float>(captureY),static_cast<float>(getWidth()-48),162.f,6.f);
   label(g,"Sound source",40,captureY+2);label(g,"Takes",40,captureY+43);label(g,"Manage",40,captureY+83);label(g,"Finish",40,captureY+123);
   label(g,"Start",110,captureY+123);label(g,"End",300,captureY+123);
   g.setColour(juce::Colour(0xff121616));g.fillRoundedRectangle(475.f,static_cast<float>(captureY+16),90.f,8.f,2.f);
   g.setColour(recordingSound.load()?juce::Colour(0xffff8f7a):juce::Colour(0xff94bba4));g.fillRoundedRectangle(475.f,static_cast<float>(captureY+16),90.f*std::clamp(captureMeterLevel,0.0f,1.0f),8.f,2.f);
  }
  if(activeTrackCount>0){
   g.setColour(juce::Colour(0xffa5b2ad));g.setFont(12.f);
   g.drawFittedText("Fade "+juce::String(selectedTrack+1)+". "+trackNames[selectedTrack],24,235,170,28,juce::Justification::centredLeft,1);
   g.drawText("To",205,238,30,22,juce::Justification::centredLeft);
   g.drawText("Over",415,238,40,22,juce::Justification::centredLeft);
   g.setColour(juce::Colour(0xff3b4943));g.fillRect(24,269,getWidth()-48,1);
  }
  const int noteWidth=std::max(10,(getWidth()-600)/16);
  if(activeTrackCount==0){g.setColour(juce::Colour(0xffa5b2ad));g.setFont(16.f);g.drawText("No tracks yet",24,sequenceTop,240,28,juce::Justification::centredLeft);}
  for(int c=0;c<activeTrackCount;++c){int y=sequenceTop+c*sequenceRowHeight;g.setColour(juce::Colour(engine.mute[c].load()&&engine.soloTrack.load()!=c?0xff1b2121:(c==engine.soloTrack.load()?0xff293d45:(c==selectedTrack?0xff26352f:0xff1e2423))));g.fillRoundedRectangle(16.f,static_cast<float>(y-1),static_cast<float>(getWidth()-32),32.f,6.f);
   if(c==selectedTrack){g.setColour(juce::Colour(0xff8fbda3));g.fillRect(16,y+3,3,24);}
   g.setColour(juce::Colour(0xff43514a));g.fillRect(getWidth()-238,y+3,1,26);
   if(kinds[c].getSelectedId()==6){
    g.setColour(juce::Colour(0xff94bba4));g.setFont(12.f);
    g.drawText(backingClips[c]?"Backing - "+timeText(backingClips[c]->duration())+(backingLoopFlags[c].load()?" - Loop":" - Play once"):"Backing audio not loaded",190,y,(getWidth()-600),29,juce::Justification::centredLeft);
    continue;
   }
   const int loop=std::clamp(engine.loopSteps[c].load(),1,Engine::maxLoopSteps);
   const int pageStart=visibleStepPage()*Engine::steps;
   for(int j=0;j<Engine::steps;++j){int x=190+j*noteWidth;const int absoluteStep=pageStart+j;const bool active=absoluteStep<loop;g.setColour(juce::Colour(active?(j%4==0?0xff303b35:0xff222727):0xff1b2020));g.fillRoundedRectangle(static_cast<float>(x),static_cast<float>(y),static_cast<float>(noteWidth-3),29.f,3.f);
    const int n=engine.pageDisplay[c*Engine::maxLoopSteps+absoluteStep].load();if(active&&n>=0){g.setColour(juce::Colour(engine.mute[c].load()&&engine.soloTrack.load()!=c?0xff54645a:0xff94bba4));g.fillRoundedRectangle(static_cast<float>(x+2),static_cast<float>(y+23-std::clamp(n-24,0,60)*0.28),static_cast<float>(noteWidth-7),6.f,2.f);}
    if(absoluteStep==engine.lanePlayheads[c].load()){g.setColour(juce::Colour(0xffb6dfc5));g.drawRoundedRectangle(static_cast<float>(x),static_cast<float>(y),static_cast<float>(noteWidth-3),29.f,3.f,1.5f);}
   }
  }
  if(activeTrackCount==0)return;
  const int panelY=soundPanelY();
  const bool sampleSelected=selectedTrackIsSample();
  const int effectTop=sampleSelected?314:240;
  g.setColour(juce::Colour(0xff222727));g.fillRoundedRectangle(24.f,static_cast<float>(panelY),static_cast<float>(getWidth()-48),static_cast<float>(trackPanelHeight()),8.f);
  g.setColour(juce::Colour(0xffe9eeeb));g.setFont(16.f);g.drawText(juce::String(selectedTrack+1)+". "+trackNames[selectedTrack],48,panelY+11,180,24,juce::Justification::centredLeft);
  if(!selectedTrackIsBacking()){
  label(g,"Loop steps",700,panelY+14);
  if(stepPage.isVisible())label(g,"Page "+juce::String(visibleStepPage()+1),895,panelY+14);
	  label(g,"Kind",48,panelY+43);
   if(kinds[selectedTrack].getSelectedId()==5){
    const auto& slot=trackInstruments[selectedTrack];
    g.setColour(juce::Colour(0xff94bba4));g.setFont(12.f);
    g.drawFittedText(slot.effect->hasPlugin()?slot.effect->name():slot.pendingName.isNotEmpty()?"Not loaded: "+slot.pendingName:"Instrument plugin",228,panelY+43,272,17,juce::Justification::centredLeft,1);
   }else label(g,"Sound",228,panelY+43);
   label(g,"Tried",508,panelY+43);label(g,"Space send",580,panelY+43);label(g,"Tone",700,panelY+43);label(g,"Motion",820,panelY+43);label(g,"Texture",940,panelY+43);
	  g.setColour(juce::Colour(0xff3b4943));g.fillRect(40,panelY+98,getWidth()-80,1);
	  if(selectedTrack>0){label(g,"Role",48,panelY+104);label(g,"Relationship",208,panelY+104);}
	  label(g,"Dynamics",368,panelY+104);label(g,"Articulation",508,panelY+104);label(g,"Note length",648,panelY+104);label(g,"Note tail",788,panelY+104);
   label(g,"Rhythm",48,panelY+160);label(g,"Hits",228,panelY+160);label(g,"Rotate",388,panelY+160);
   g.setColour(juce::Colour(0xff94bba4));g.setFont(12.f);
   g.drawText(engine.rhythmMode[selectedTrack].load()==1?"Exact spacing; Generate changes pitches. Manual edits return to Free.":"Density and follower relationships shape the timing.",548,panelY+179,510,26,juce::Justification::centredLeft);
   if(sampleSelected){
    g.setColour(juce::Colour(0xff3b4943));g.fillRect(40,panelY+218,getWidth()-80,1);
    g.setColour(juce::Colour(0xff94bba4));g.setFont(11.f);g.drawText("SAMPLE SHAPE",48,panelY+220,160,14,juce::Justification::centredLeft);
    label(g,"Start",48,panelY+237);label(g,"Size",168,panelY+237);label(g,"Fade",288,panelY+237);label(g,"Play",508,panelY+237);
   }
  }else{
   label(g,"Kind",48,panelY+43);
   g.setFont(14.f);g.setColour(juce::Colour(0xffe9eeeb));
   g.drawFittedText(backingFiles[selectedTrack].existsAsFile()?backingFiles[selectedTrack].getFileName():"Audio not loaded - choose the recording again",48,panelY+106,getWidth()-120,28,juce::Justification::centredLeft,1);
   const auto& timing=backingTimings[selectedTrack];
   g.setFont(12.f);g.setColour(juce::Colour(0xffa5b2ad));
   g.drawText(timing.canAlign()?"Original tempo: "+juce::String(timing.bpm,1)+" BPM · Start step: "+juce::String(timing.startStep,3):"Original timing unknown or variable. Use Timing to enter a steady BPM and start step.",48,panelY+145,getWidth()-120,24,juce::Justification::centredLeft);
   g.drawText("Full recording at original speed. Play starts it from the beginning; new tracks follow the Piece tempo.",48,panelY+180,getWidth()-120,24,juce::Justification::centredLeft);
  }
   g.setColour(juce::Colour(0xff3b4943));g.fillRect(40,panelY+effectTop-22,getWidth()-80,1);
   label(g,"Track effects (1 then 2)",48,panelY+effectTop-17);
   for(int effectIndex=0;effectIndex<trackEffectCount;++effectIndex){
   const auto& slot=trackEffectSlots[selectedTrack][effectIndex];
   const int rowY=panelY+effectTop+effectIndex*38;
    g.setColour(juce::Colour(0xff94bba4));g.setFont(13.f);g.drawText(juce::String(effectIndex+1),32,rowY,14,28,juce::Justification::centred);
    g.setColour(juce::Colour(slot.effect->hasPlugin()?(slot.effect->isBypassed()?0xffdcab9f:0xffa6cfb4):0xffa5b2ad));
    g.setFont(12.f);
    g.drawText(slot.effect->hasPlugin()?(slot.effect->isBypassed()?"BYPASSED":"ACTIVE"):(slot.pendingName.isNotEmpty()?"NOT LOADED":"EMPTY"),660,rowY,112,28,juce::Justification::centredLeft);
   }
  }
 void resized() override {
  keep.setBounds(getWidth()-500,18,100,24);keeps.setBounds(getWidth()-390,18,90,24);help.setBounds(getWidth()-280,18,70,24);midi.setBounds(getWidth()-202,18,54,24);devices.setBounds(getWidth()-145,18,121,24);play.setBounds(24,76,100,32);tempo.setBounds(145,76,220,32);density.setBounds(390,76,240,32);aiPiece.setBounds(640,76,80,32);randomize.setBounds(getWidth()-390,76,176,32);generate.setBounds(getWidth()-200,76,176,32);focusFade.setBounds(365,188,98,30);changeEvery.setBounds(475,188,165,30);
  key.setBounds(24,135,70,32);scale.setBounds(110,135,185,32);shape.setBounds(310,135,150,32);mode.setBounds(475,135,170,32);
  sceneName.setBounds(660,135,260,32);save.setBounds(930,135,90,32);pieceActions.setBounds(1030,135,65,32);
  addTrack.setBounds(24,188,76,30);recordToggle.setBounds(110,188,170,30);cloneTrack.setBounds(290,188,70,30);
  cloneTrack.setEnabled(selectedTrack>=0&&selectedTrack<activeTrackCount&&activeTrackCount<Engine::maxTracks);
  const bool hasTrack=activeTrackCount>0&&selectedTrack>=0&&selectedTrack<activeTrackCount;
  fadeTarget.setVisible(hasTrack);fadeSeconds.setVisible(hasTrack);startFade.setVisible(hasTrack);cancelFade.setVisible(hasTrack);
  fadeTarget.setBounds(230,233,170,30);fadeSeconds.setBounds(455,233,185,30);startFade.setBounds(660,235,110,28);cancelFade.setBounds(782,235,90,28);
  chooseEffect.setBounds(24,318,120,28);editEffect.setBounds(442,318,64,28);
  bypassEffect.setBounds(514,318,112,28);removeEffect.setBounds(634,318,80,28);
  for(int effectIndex=0;effectIndex<trackEffectCount;++effectIndex){
   chooseTrackEffect[effectIndex].setVisible(hasTrack);editTrackEffect[effectIndex].setVisible(hasTrack);
   bypassTrackEffect[effectIndex].setVisible(hasTrack);
   trackEffectActions[effectIndex].setVisible(hasTrack);
  }
  chooseInstrument.setVisible(hasTrack&&kinds[selectedTrack].getSelectedId()==5);
  editInstrument.setVisible(hasTrack&&kinds[selectedTrack].getSelectedId()==5);
  removeInstrument.setVisible(hasTrack&&kinds[selectedTrack].getSelectedId()==5);
  const int panelY=soundPanelY();
  const int effectTop=selectedTrackIsSample()?314:240;
  for(int effectIndex=0;effectIndex<trackEffectCount;++effectIndex){
   const int rowY=panelY+effectTop+effectIndex*38;
   chooseTrackEffect[effectIndex].setBounds(48,rowY,600,28);
   editTrackEffect[effectIndex].setBounds(790,rowY,64,28);
   bypassTrackEffect[effectIndex].setBounds(866,rowY,120,28);trackEffectActions[effectIndex].setBounds(998,rowY,42,28);
  }
  monitorReturn.setBounds(748,318,92,28);returnLevel.setBounds(890,318,184,28);
  wet.setBounds(48,370,120,28);drive.setBounds(208,370,120,28);filter.setBounds(368,370,120,28);room.setBounds(528,370,120,28);
  const int captureY=recordPanelY();
  recordSound.setBounds(110,captureY+10,124,24);useLastSound.setBounds(244,captureY+10,88,24);discardSound.setBounds(342,captureY+10,82,24);
  recordMix.setBounds(110,captureY+50,104,24);showRecordings.setBounds(224,captureY+50,104,24);takeList.setBounds(338,captureY+50,220,24);playTake.setBounds(568,captureY+50,90,24);revealTake.setBounds(668,captureY+50,80,24);
  takeName.setBounds(110,captureY+90,260,24);renameTake.setBounds(380,captureY+90,80,24);deleteTake.setBounds(470,captureY+90,74,24);useTakeSound.setBounds(554,captureY+90,132,24);
  finalStart.setBounds(150,captureY+130,130,24);finalEnd.setBounds(330,captureY+130,130,24);exportFinal.setBounds(480,captureY+130,120,24);
  addTakeBacking.setBounds(700,captureY+90,160,24);
  recordDone.setBounds(getWidth()-145,captureY+10,121,24);
  std::array<juce::Component*,17> recordControls{&recordSound,&useLastSound,&discardSound,&recordMix,&showRecordings,&takeList,&playTake,&revealTake,&takeName,&renameTake,&deleteTake,&useTakeSound,&finalStart,&finalEnd,&exportFinal,&addTakeBacking,&recordDone};
  for(auto* control:recordControls)control->setVisible(recordPanelOpen);
  for(int c=0;c<Engine::maxTracks;++c){
   const bool active=c<activeTrackCount;
   trackSelect[c].setVisible(active);trackSelect[c].setBounds(24,sequenceTop+1+c*sequenceRowHeight,160,29);
   updateTrackSelectLabel(c);
   rowLevels[c].setVisible(active);rowLevels[c].setBounds(getWidth()-380,sequenceTop+1+c*sequenceRowHeight,128,28);
   focuses[c].setVisible(active);holds[c].setVisible(active);mutes[c].setVisible(active);solos[c].setVisible(active);
   focuses[c].setBounds(getWidth()-242,sequenceTop+2+c*sequenceRowHeight,50,26);holds[c].setBounds(getWidth()-185,sequenceTop+2+c*sequenceRowHeight,50,26);mutes[c].setBounds(getWidth()-128,sequenceTop+2+c*sequenceRowHeight,50,26);solos[c].setBounds(getWidth()-71,sequenceTop+2+c*sequenceRowHeight,50,26);
   trackSelect[c].toFront(false);rowLevels[c].toFront(false);focuses[c].toFront(false);holds[c].toFront(false);mutes[c].toFront(false);solos[c].toFront(false);
  }
  if(editingTrack>=0&&editingTrack<activeTrackCount){trackNameEditor.setBounds(trackSelect[editingTrack].getBounds());trackNameEditor.toFront(false);}
	  for(int c=0;c<Engine::maxTracks;++c){const bool backingSelected=c==selectedTrack&&kinds[c].getSelectedId()==6;const bool selected=c==selectedTrack&&!backingSelected;backingLoops[c].setVisible(backingSelected);const bool followerSelected=selected&&c>0;const bool sampleSelected=selected&&kinds[c].getSelectedId()==3;kinds[c].setVisible(selected||backingSelected);sounds[c].setVisible(selected&&kinds[c].getSelectedId()!=5);sources[c].setVisible(false);materials[c].setVisible(false);playModes[c].setVisible(sampleSelected);sampleStarts[c].setVisible(sampleSelected);sampleSizes[c].setVisible(sampleSelected);sampleFades[c].setVisible(sampleSelected);sampleReverses[c].setVisible(sampleSelected);librarySounds[c].setVisible(false);instruments[c].setVisible(false);roles[c].setVisible(followerSelected);lengths[c].setVisible(selected);loopLengths[c].setVisible(selected);rhythms[c].setVisible(selected);rhythmHits[c].setVisible(selected);rhythmRotations[c].setVisible(selected);levels[c].setVisible(false);sends[c].setVisible(selected);colors[c].setVisible(selected);moves[c].setVisible(selected);weathers[c].setVisible(selected);relationships[c].setVisible(followerSelected);dynamics[c].setVisible(selected);articulations[c].setVisible(selected);tails[c].setVisible(selected);connects[c].setVisible(selected);}
  chooseBacking.setVisible(hasTrack&&selectedTrackIsBacking());backingTiming.setVisible(hasTrack&&selectedTrackIsBacking());
  if(hasTrack){chooseBacking.setBounds(230,panelY+62,140,26);backingTiming.setBounds(385,panelY+62,110,26);backingLoops[selectedTrack].setBounds(515,panelY+62,160,26);}
  if(activeTrackCount==0||selectedTrack<0||selectedTrack>=activeTrackCount){patternBack.setVisible(false);trackGenerate.setVisible(false);patternForward.setVisible(false);aiIdea.setVisible(false);trackActions.setVisible(false);stepPage.setVisible(false);soundPrevious.setVisible(false);soundNext.setVisible(false);soundRandom.setVisible(false);soundBack.setVisible(false);soundForward.setVisible(false);return;}
	  patternBack.setVisible(!selectedTrackIsBacking());trackGenerate.setVisible(!selectedTrackIsBacking());patternForward.setVisible(!selectedTrackIsBacking());aiIdea.setVisible(!selectedTrackIsBacking());trackActions.setVisible(true);patternBack.setBounds(240,panelY+12,92,24);trackGenerate.setBounds(342,panelY+12,126,24);patternForward.setBounds(478,panelY+12,92,24);aiIdea.setBounds(580,panelY+12,88,24);loopLengths[selectedTrack].setBounds(778,panelY+12,108,26);trackActions.setBounds(getWidth()-104,panelY+12,56,24);
  rhythms[selectedTrack].setBounds(48,panelY+180,140,26);
  rhythmHits[selectedTrack].setBounds(228,panelY+180,120,26);
  rhythmRotations[selectedTrack].setBounds(388,panelY+180,120,26);
  int longestLoop=Engine::steps;for(int c=0;c<activeTrackCount;++c)longestLoop=std::max(longestLoop,engine.loopSteps[c].load());
  const int pageCount=(longestLoop+Engine::steps-1)/Engine::steps;
  for(int page=1;page<=Engine::maxLoopSteps/Engine::steps;++page)stepPage.setItemEnabled(page+1,page<=pageCount);
  if(pageCount==1){stepPage.setSelectedId(1,juce::dontSendNotification);followedPage=0;}
  else if(stepPage.getSelectedId()>pageCount+1)stepPage.setSelectedId(pageCount+1,juce::dontSendNotification);
  stepPage.setVisible(pageCount>1&&!selectedTrackIsBacking());stepPage.setBounds(945,panelY+12,60,26);
	  soundPrevious.setVisible(kinds[selectedTrack].getSelectedId()!=5&&!selectedTrackIsBacking());soundNext.setVisible(kinds[selectedTrack].getSelectedId()!=5&&!selectedTrackIsBacking());soundRandom.setVisible(kinds[selectedTrack].getSelectedId()!=5&&!selectedTrackIsBacking());soundBack.setVisible(!selectedTrackIsBacking());soundForward.setVisible(!selectedTrackIsBacking());
	  soundBack.setBounds(508,panelY+62,28,26);soundForward.setBounds(540,panelY+62,28,26);
	  kinds[selectedTrack].setBounds(48,panelY+62,140,26);soundPrevious.setBounds(197,panelY+62,28,26);sounds[selectedTrack].setBounds(230,panelY+62,155,26);soundNext.setBounds(390,panelY+62,28,26);soundRandom.setBounds(425,panelY+62,75,26);
   chooseInstrument.setBounds(230,panelY+62,120,26);editInstrument.setBounds(358,panelY+62,58,26);removeInstrument.setBounds(424,panelY+62,76,26);
	  sends[selectedTrack].setBounds(580,panelY+60,105,28);colors[selectedTrack].setBounds(700,panelY+60,110,28);moves[selectedTrack].setBounds(820,panelY+60,110,28);weathers[selectedTrack].setBounds(940,panelY+60,128,28);
	  if(selectedTrack>0){roles[selectedTrack].setBounds(48,panelY+123,140,26);relationships[selectedTrack].setBounds(208,panelY+121,140,28);}
	  dynamics[selectedTrack].setBounds(368,panelY+121,120,28);articulations[selectedTrack].setBounds(508,panelY+121,120,28);lengths[selectedTrack].setBounds(648,panelY+123,120,26);tails[selectedTrack].setBounds(788,panelY+121,120,28);connects[selectedTrack].setBounds(928,panelY+123,120,26);
	  sampleStarts[selectedTrack].setBounds(48,panelY+255,108,28);sampleSizes[selectedTrack].setBounds(168,panelY+255,108,28);sampleFades[selectedTrack].setBounds(288,panelY+255,108,28);sampleReverses[selectedTrack].setBounds(406,panelY+258,70,22);playModes[selectedTrack].setBounds(508,panelY+255,120,26);
 }
 void mouseDown(const juce::MouseEvent& e) override {
   const int track=trackAt(e.position);
  const int step=stepAt(e.position);
  if(track>=0){
   selectTrack(track);
   if(step>=0){
    const int absoluteStep=visibleStepPage()*Engine::steps+step;
    if(kinds[track].getSelectedId()!=6&&absoluteStep<engine.loopSteps[track].load()){rememberPattern(track,true);setRhythmSettings(track,0,engine.euclideanHits[track].load(),engine.euclideanRotate[track].load());engine.toggleStep(track,absoluteStep);rememberPattern(track,true);}
   }else revealSelectedTrackInspector();
   repaint();
  }
 }
private:
 Engine engine;
 std::unique_ptr<HelpWindow> helpWindow;
 juce::TooltipWindow tooltipWindow{this,700};
 class CaptureTap final : public juce::AudioIODeviceCallback {
 public:
  void start(std::vector<float>* target,int capacity){
   buffer=target;
   maxSamples=capacity;
   writeIndex.store(0);
   active.store(true);
  }
  int stop(){
   active.store(false);
   while(inCallback.load())std::this_thread::yield();
   return std::min(writeIndex.load(),maxSamples);
  }
  bool isActive() const {return active.load();}
  int recordedSamples() const {return std::min(writeIndex.load(),maxSamples);}
  float peak() {return inputPeak.exchange(0.0f);}
  void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,int numInputChannels,float* const* outputChannelData,int numOutputChannels,int numSamples,const juce::AudioIODeviceCallbackContext&) override {
   inCallback.store(true);
   for(int channel=0;channel<numOutputChannels;++channel)if(outputChannelData[channel]!=nullptr)juce::FloatVectorOperations::clear(outputChannelData[channel],numSamples);
   if(!active.load()||buffer==nullptr||numInputChannels<=0||inputChannelData[0]==nullptr){inCallback.store(false);return;}
   int index=writeIndex.load();
   if(index>=maxSamples){active.store(false);inCallback.store(false);return;}
   const int writable=std::min(numSamples,maxSamples-index);
   auto& target=*buffer;
   float blockPeak=0.0f;
   for(int i=0;i<writable;++i){const float value=inputChannelData[0][i];target[static_cast<size_t>(index+i)]=value;blockPeak=std::max(blockPeak,std::abs(value));}
   inputPeak.store(std::max(inputPeak.load(),blockPeak));
   writeIndex.store(index+writable);
   if(index+writable>=maxSamples)active.store(false);
   inCallback.store(false);
  }
  void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
  void audioDeviceStopped() override {active.store(false);}
 private:
  std::atomic<bool> active{false};
  std::atomic<bool> inCallback{false};
  std::atomic<int> writeIndex{0};
  std::atomic<float> inputPeak{0.0f};
  std::vector<float>* buffer=nullptr;
  int maxSamples=0;
 };
 juce::TextButton addTakeBacking,chooseBacking,backingTiming;
 juce::TextButton play,generate,randomize,aiPiece,addTrack,cloneTrack,recordToggle,recordDone,startFade,cancelFade,chooseEffect,editEffect,bypassEffect,removeEffect,chooseInstrument,editInstrument,removeInstrument,monitorReturn,patternBack,trackGenerate,patternForward,aiIdea,trackActions,soundPrevious,soundNext,soundRandom,soundBack,soundForward,help,keep,keeps,recordSound,recordMix,showRecordings,playTake,revealTake,renameTake,deleteTake,useTakeSound,exportFinal,useLastSound,discardSound,pieceActions,save,midi,devices;
 static constexpr int trackEffectCount=2;
 std::array<juce::TextButton,trackEffectCount> chooseTrackEffect,editTrackEffect,bypassTrackEffect,trackEffectActions;
 std::array<TrackSelectButton,Engine::maxTracks> trackSelect;
 juce::TextEditor sceneName,takeName,trackNameEditor;
 int editingTrack=-1;
 juce::String pieceId=juce::Uuid().toString();
 juce::Slider tempo,density,room,wet,drive,filter,focusFade,changeEvery,fadeTarget,fadeSeconds,returnLevel,finalStart,finalEnd;
 std::array<juce::Slider,Engine::maxTracks> rowLevels;
 juce::ComboBox key,scale,shape,mode,takeList,stepPage;
 int followedPage=0;
 int visibleStepPage() const {return stepPage.getSelectedId()==1?followedPage:std::clamp(stepPage.getSelectedId()-2,0,Engine::maxLoopSteps/Engine::steps-1);}
 int selectedTrack=-1;
 int activeTrackCount=0;
 bool recordPanelOpen=false;
 std::array<juce::String,Engine::maxTracks> trackNames{};
 std::array<juce::String,Engine::maxTracks> trackSubtitles{};
 struct PatternCandidate {
  std::array<int,Engine::maxLoopSteps> phrase{};
  int role=1;
  int relationship=0;
  int dynamics=0,articulation=0,motion=0,texture=0,length=1,loopSteps=Engine::steps,rhythm=0,hits=5,rotation=0;
  bool sameControls(const PatternCandidate& other) const {return role==other.role&&relationship==other.relationship&&dynamics==other.dynamics&&articulation==other.articulation&&motion==other.motion&&texture==other.texture&&length==other.length&&loopSteps==other.loopSteps&&rhythm==other.rhythm&&hits==other.hits&&rotation==other.rotation;}
  bool operator==(const PatternCandidate& other) const {return phrase==other.phrase&&sameControls(other);}
 };
 std::array<std::vector<PatternCandidate>,Engine::maxTracks> patternHistory{};
 std::array<int,Engine::maxTracks> patternHistoryIndex{};
 PatternCandidate aiBaseline;
 struct PieceBaseline {
  int count=0,bpm=0,density=0,key=0,scale=0,shape=0,mode=0,space=0,room=0;
  std::array<PatternCandidate,Engine::maxTracks> patterns{};
  std::array<bool,Engine::maxTracks> held{};
 } aiPieceBaseline;
 std::unique_ptr<juce::ChildProcess> aiProcess;
 std::unique_ptr<juce::ChildProcess> aiAvailabilityProcess;
 bool aiAvailable=false;
 bool aiPieceRequest=false;
 bool aiPieceDroneRequested=false;
 juce::String aiAvailabilityReason="Checking Apple Intelligence...";
 juce::uint32 aiLastAvailabilityCheck=0;
 int aiTrack=-1,aiScale=0,aiKey=0;
 juce::String aiTrackName;
 juce::uint32 aiStartedAt=0;
 std::array<double,Engine::maxTracks> fadeTargets{},fadeDurations{};
 std::array<bool,Engine::maxTracks> fadeWasActive{};
 struct SoundChoice {int kind=1,sound=1;bool operator==(const SoundChoice& other) const {return kind==other.kind&&sound==other.sound;}};
 std::array<std::vector<SoundChoice>,Engine::maxTracks> soundHistory{};
 std::array<int,Engine::maxTracks> soundHistoryIndex{};
 struct PendingSoundChange {bool active=false;juce::uint32 due=0;};
 std::array<PendingSoundChange,Engine::maxTracks> pendingSoundChanges{};
 static constexpr int wholePieceTop=270;
 static constexpr int sequenceTop=418;
 static constexpr int sequenceRowHeight=34;
 int soundPanelY() const {return sequenceTop+activeTrackCount*sequenceRowHeight+14;}
 bool selectedTrackIsBacking() const {return selectedTrack>=0&&selectedTrack<activeTrackCount&&kinds[selectedTrack].getSelectedId()==6;}
 bool selectedTrackIsSample() const {return selectedTrack>=0&&selectedTrack<activeTrackCount&&kinds[selectedTrack].getSelectedId()==3;}
 int trackPanelHeight() const {return selectedTrackIsSample()?392:318;}
 int recordPanelY() const {return activeTrackCount==0?sequenceTop+48:soundPanelY()+trackPanelHeight()+12;}
 int contentHeight() const {return recordPanelOpen?std::max(760,recordPanelY()+180):std::max(760,recordPanelY());}
 void setRecordPanelOpen(bool open){
  if(open)refreshTakeList(true);
  recordPanelOpen=open;
  recordToggle.setToggleState(open,juce::dontSendNotification);
  recordToggle.setButtonText(open?"Hide Record / Finish":"Record / Finish");
  setSize(getWidth(),contentHeight());
  resized();
  if(auto* viewport=findParentComponentOfClass<juce::Viewport>())viewport->setViewPosition(0,open?std::max(0,recordPanelY()+180-viewport->getViewHeight()):0);
  repaint();
 }
 void setAudioReturnEnabled(bool enable){
  if(!enable){audioReturnEnabled=false;status="Audio return off";repaint();return;}
  monitorReturn.setToggleState(false,juce::dontSendNotification);
  if(activeInputChannels.load()<2){status="Choose a two-channel input in Audio device";repaint();return;}
  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
   .withTitle("Monitor audio input?")
   .withMessage("Phrase Mix will monitor the selected stereo input and include it in takes. Connect only the processor's output or a virtual loopback source. Keep monitor outputs 1-2 separate from the return input to avoid feedback.")
   .withButton("Monitor").withButton("Cancel");
  juce::Component::SafePointer<MainView> safe(this);
  juce::NativeMessageBox::showAsync(options,[safe](int result){
   if(safe==nullptr||result!=1)return;
   if(safe->activeInputChannels.load()<2){safe->status="Stereo input is unavailable";safe->repaint();return;}
   safe->audioReturnEnabled=true;
   safe->monitorReturn.setToggleState(true,juce::dontSendNotification);
   safe->status="Audio return on";
   safe->repaint();
  });
 }
 void refreshPluginControls(){
  const bool loaded=pluginEffect.hasPlugin();
  editEffect.setEnabled(loaded);
  bypassEffect.setEnabled(loaded);
  removeEffect.setEnabled(loaded||!pendingEffectState.isVoid());
  const bool bypassed=loaded&&pluginEffect.isBypassed();
  bypassEffect.setToggleState(bypassed,juce::dontSendNotification);
  bypassEffect.setButtonText(bypassed?"Bypass ON":"Bypass OFF");
  bypassEffect.setTooltip(!loaded?"Load an effect to use bypass.":bypassed?"Effect is bypassed. Click to turn it back on.":"Effect is active. Click to bypass it.");
  repaint();
 }
 void showPluginPicker(int track=-1,int effectIndex=0){
  if(track>=activeTrackCount||effectIndex<0||effectIndex>=trackEffectCount)return;
  auto* target=track<0?&pluginEffect:trackEffectSlots[track][effectIndex].effect.get();
  if(pluginPickerWindow!=nullptr&&pickerTarget==target){pluginPickerWindow->setVisible(true);pluginPickerWindow->toFront(true);return;}
  pluginPickerWindow.reset();
  pickerTarget=target;
  appDataDirectory().createDirectory();
  juce::Component::SafePointer<MainView> safe(this);
  std::weak_ptr<PluginEffect> trackTarget=track<0?std::weak_ptr<PluginEffect>{}:trackEffectSlots[track][effectIndex].effect;
  auto picker=std::make_unique<PluginPicker>(pluginEffect.formats,pluginEffect.known,
   appDataDirectory().getChildFile("PluginScanGuard.txt"),[safe,track,trackTarget](juce::PluginDescription description){
   if(safe==nullptr)return;
   if(track<0)safe->beginPluginLoad(description,{},false);
   else if(auto effect=trackTarget.lock()){
    const auto [currentTrack,currentIndex]=safe->trackEffectPosition(effect.get());
    if(currentTrack>=0)safe->beginTrackPluginLoad(currentTrack,currentIndex,description,{},false);
   }
  });
  pluginPickerWindow=std::make_unique<PluginPickerWindow>(picker.release());
 }
 void showPluginEditor(){
  if(pluginEditorWindow!=nullptr){pluginEditorWindow->setVisible(true);pluginEditorWindow->toFront(true);return;}
  if(auto* editor=pluginEffect.createEditor())pluginEditorWindow=std::make_unique<PluginEditorWindow>(pluginEffect.name(),editor);
 }
 void clearPluginEffect(){
  ++effectLoadRevision;
  pluginEditorWindow.reset();
  pluginEffect.clear();
  pendingEffectState=juce::var();
  pendingEffectName.clear();
  status="Effect removed";
  refreshPluginControls();
 }
 void beginPluginLoad(const juce::PluginDescription& description,juce::var savedState,bool bypass){
  if(description.isInstrument){status="Choose an audio effect, not an instrument";repaint();return;}
  const unsigned revision=++effectLoadRevision;
  const bool restoring=!savedState.isVoid();
  pluginEditorWindow.reset();
  if(savedState.isVoid()){
   juce::DynamicObject::Ptr target=new juce::DynamicObject();
   if(auto xml=description.createXml())target->setProperty("description",xml->toString());
   target->setProperty("state",juce::String());
   target->setProperty("bypassed",bypass);
   savedState=juce::var(target.get());
  }
  pendingEffectState=savedState;
  pendingEffectName=description.name;
  refreshPluginControls();
  juce::MemoryBlock state;
  if(auto* object=savedState.getDynamicObject())state.fromBase64Encoding(object->getProperty("state").toString());
  status="Loading "+description.name;
  repaint();
  juce::Component::SafePointer<MainView> safe(this);
  pluginEffect.formats.createPluginInstanceAsync(description,captureSampleRate.load(),effectBlockSize.load(),
   [safe,revision,description,state,bypass,restoring](std::unique_ptr<juce::AudioPluginInstance> instance,const juce::String& error){
    if(safe==nullptr||safe->effectLoadRevision!=revision)return;
    if(instance==nullptr){
     safe->status="Effect could not load: "+error;
     if(!restoring){safe->pendingEffectState=juce::var();safe->pendingEffectName.clear();}
     safe->refreshPluginControls();return;
    }
    safe->pluginEditorWindow.reset();
    const auto problem=safe->pluginEffect.replace(std::move(instance),description,state,bypass);
    if(problem.isNotEmpty()){
     safe->status=problem;
     if(!restoring){safe->pendingEffectState=juce::var();safe->pendingEffectName.clear();}
    }
    else{safe->pendingEffectState=juce::var();safe->pendingEffectName.clear();safe->status="Effect: "+description.name;}
    safe->refreshPluginControls();
   });
 }
 void restorePluginEffect(const juce::var& saved){
  ++effectLoadRevision;
  pluginEditorWindow.reset();
  pluginEffect.clear();
  pendingEffectState=saved;
  pendingEffectName.clear();
  refreshPluginControls();
  auto* object=saved.getDynamicObject();
  if(object==nullptr)return;
  auto xml=juce::XmlDocument::parse(object->getProperty("description").toString());
  juce::PluginDescription description;
  if(xml==nullptr||!description.loadFromXml(*xml)){
   status="Saved effect description is invalid";repaint();return;
  }
  pendingEffectName=description.name;
  beginPluginLoad(description,saved,static_cast<bool>(object->getProperty("bypassed")));
 }
 std::pair<int,int> trackEffectPosition(const PluginEffect* effect) const {
  for(int track=0;track<activeTrackCount;++track)
   for(int effectIndex=0;effectIndex<trackEffectCount;++effectIndex)
    if(trackEffectSlots[track][effectIndex].effect.get()==effect)return {track,effectIndex};
  return {-1,-1};
 }
 void refreshTrackPluginControls(){
  const bool valid=selectedTrack>=0&&selectedTrack<activeTrackCount;
  for(int effectIndex=0;effectIndex<trackEffectCount;++effectIndex){
   const auto* slot=valid?&trackEffectSlots[selectedTrack][effectIndex]:nullptr;
   const bool loaded=slot!=nullptr&&slot->effect->hasPlugin();
   editTrackEffect[effectIndex].setEnabled(loaded);
   editTrackEffect[effectIndex].setTooltip(loaded?"Open this track effect's controls.":"Load a track effect to edit it.");
   bypassTrackEffect[effectIndex].setEnabled(loaded);
   const bool bypassed=loaded&&slot->effect->isBypassed();
   bypassTrackEffect[effectIndex].setToggleState(bypassed,juce::dontSendNotification);
   bypassTrackEffect[effectIndex].setButtonText("Bypass");
   bypassTrackEffect[effectIndex].setTooltip(!loaded?"Load a track effect to use bypass.":bypassed?"Track effect is bypassed. Click to turn it back on.":"Track effect is active. Click to bypass it.");
   chooseTrackEffect[effectIndex].setButtonText(loaded?slot->effect->name():(slot!=nullptr&&slot->pendingName.isNotEmpty()?"Not loaded: "+slot->pendingName:"Choose effect"));
   chooseTrackEffect[effectIndex].setTooltip("Choose or replace track effect "+juce::String(effectIndex+1)+". Effects run from 1 to 2.");
   trackEffectActions[effectIndex].setTooltip("Choose, replace, or remove track effect "+juce::String(effectIndex+1)+".");
  }
  repaint();
 }
 void showTrackEffectMenu(int effectIndex){
  const int lane=selectedTrack;
  if(lane<0||lane>=activeTrackCount||effectIndex<0||effectIndex>=trackEffectCount)return;
  const auto& slot=trackEffectSlots[lane][effectIndex];
  const bool loaded=slot.effect->hasPlugin();
  juce::PopupMenu menu;
  menu.addItem(1,loaded?"Replace effect...":"Choose effect...");
  menu.addSeparator();
  menu.addItem(2,"Remove effect",loaded||!slot.pending.isVoid());
  juce::Component::SafePointer<MainView> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(trackEffectActions[effectIndex]),[safe,lane,effectIndex](int result){
   if(safe==nullptr||lane>=safe->activeTrackCount)return;
   if(result==1)safe->showPluginPicker(lane,effectIndex);
   else if(result==2)safe->clearTrackPluginEffect(lane,effectIndex);
  });
 }
 void showTrackPluginEditor(int effectIndex){
  if(selectedTrack<0||selectedTrack>=activeTrackCount)return;
  auto& slot=trackEffectSlots[selectedTrack][effectIndex];
  if(slot.editor!=nullptr){slot.editor->setVisible(true);slot.editor->toFront(true);return;}
  if(auto* editor=slot.effect->createEditor())slot.editor=std::make_unique<PluginEditorWindow>(slot.effect->name(),editor);
 }
 void clearTrackPluginEffect(int track,int effectIndex){
  if(track<0||track>=activeTrackCount)return;
  auto& slot=trackEffectSlots[track][effectIndex];
  ++slot.revision;
  slot.editor.reset();slot.effect->clear();slot.pending=juce::var();slot.pendingName.clear();
  status="Track "+juce::String(track+1)+" effect "+juce::String(effectIndex+1)+" removed";
  refreshTrackPluginControls();
 }
 void beginTrackPluginLoad(int track,int effectIndex,const juce::PluginDescription& description,juce::var savedState,bool bypass){
  if(track<0||track>=activeTrackCount)return;
  if(description.isInstrument){status="Choose an audio effect, not an instrument";repaint();return;}
  auto& slot=trackEffectSlots[track][effectIndex];
  const unsigned revision=++slot.revision;
  const bool restoring=!savedState.isVoid();
  slot.editor.reset();
  if(savedState.isVoid()){
   juce::DynamicObject::Ptr target=new juce::DynamicObject();
   if(auto xml=description.createXml())target->setProperty("description",xml->toString());
   target->setProperty("state",juce::String());target->setProperty("bypassed",bypass);
   savedState=juce::var(target.get());
  }
  slot.pending=savedState;slot.pendingName=description.name;
  refreshTrackPluginControls();
  juce::MemoryBlock state;
  if(auto* object=savedState.getDynamicObject())state.fromBase64Encoding(object->getProperty("state").toString());
  status="Loading track "+juce::String(track+1)+" effect "+juce::String(effectIndex+1)+": "+description.name;repaint();
  juce::Component::SafePointer<MainView> safe(this);
  std::weak_ptr<PluginEffect> destination=slot.effect;
  pluginEffect.formats.createPluginInstanceAsync(description,captureSampleRate.load(),effectBlockSize.load(),
   [safe,destination,revision,description,state,bypass,restoring](std::unique_ptr<juce::AudioPluginInstance> instance,const juce::String& error){
    if(safe==nullptr)return;
    auto effect=destination.lock();
    if(effect==nullptr)return;
    const auto [track,effectIndex]=safe->trackEffectPosition(effect.get());
    if(track<0)return;
    auto& slot=safe->trackEffectSlots[track][effectIndex];
    if(slot.revision!=revision)return;
    const auto problem=instance==nullptr?"Effect could not load: "+error:effect->replace(std::move(instance),description,state,bypass);
    if(problem.isNotEmpty()){
     safe->status=problem;
     if(!restoring){slot.pending=juce::var();slot.pendingName.clear();}
    }else{
     slot.pending=juce::var();slot.pendingName.clear();
     safe->status="Track "+juce::String(track+1)+" effect "+juce::String(effectIndex+1)+": "+description.name;
    }
    safe->refreshTrackPluginControls();
   });
 }
 void restoreTrackPluginEffect(int track,int effectIndex,const juce::var& saved){
  auto& slot=trackEffectSlots[track][effectIndex];
  ++slot.revision;slot.editor.reset();slot.effect->clear();
  slot.pending=saved;slot.pendingName.clear();
  auto* object=saved.getDynamicObject();
  if(object==nullptr)return;
  auto xml=juce::XmlDocument::parse(object->getProperty("description").toString());
  juce::PluginDescription description;
  if(xml==nullptr||!description.loadFromXml(*xml)){
   status="Saved track effect description is invalid";repaint();return;
  }
  slot.pendingName=description.name;
  beginTrackPluginLoad(track,effectIndex,description,saved,static_cast<bool>(object->getProperty("bypassed")));
 }
 int trackInstrumentPosition(const PluginEffect* effect) const {
  for(int track=0;track<activeTrackCount;++track)if(trackInstruments[track].effect.get()==effect)return track;
  return -1;
 }
 void refreshInstrumentControls(){
  const bool valid=selectedTrack>=0&&selectedTrack<activeTrackCount;
  const auto* slot=valid?&trackInstruments[selectedTrack]:nullptr;
  const bool loaded=slot!=nullptr&&slot->effect->hasPlugin();
  editInstrument.setEnabled(loaded);
  removeInstrument.setEnabled(slot!=nullptr&&(loaded||!slot->pending.isVoid()));
  chooseInstrument.setTooltip("Choose an AU or VST3 instrument for this track. Its audio runs through track effects 1 and 2.");
  editInstrument.setTooltip("Open this instrument's controls.");
  removeInstrument.setTooltip("Remove this track's instrument. The Plugin sound source will be silent until you choose another.");
  repaint();
 }
 void showInstrumentPicker(){
  if(selectedTrack<0||selectedTrack>=activeTrackCount)return;
  auto* target=trackInstruments[selectedTrack].effect.get();
  if(pluginPickerWindow!=nullptr&&pickerTarget==target){pluginPickerWindow->setVisible(true);pluginPickerWindow->toFront(true);return;}
  pluginPickerWindow.reset();pickerTarget=target;
  appDataDirectory().createDirectory();
  juce::Component::SafePointer<MainView> safe(this);
  std::weak_ptr<PluginEffect> destination=trackInstruments[selectedTrack].effect;
  auto picker=std::make_unique<PluginPicker>(pluginEffect.formats,pluginEffect.known,
   appDataDirectory().getChildFile("PluginScanGuard.txt"),[safe,destination](juce::PluginDescription description){
    if(safe==nullptr)return;
    if(auto effect=destination.lock()){
     const int track=safe->trackInstrumentPosition(effect.get());
     if(track>=0)safe->beginInstrumentLoad(track,description,{});
    }
   },true);
  pluginPickerWindow=std::make_unique<PluginPickerWindow>(picker.release(),true);
 }
 void showInstrumentEditor(){
  if(selectedTrack<0||selectedTrack>=activeTrackCount)return;
  auto& slot=trackInstruments[selectedTrack];
  if(slot.editor!=nullptr){slot.editor->setVisible(true);slot.editor->toFront(true);return;}
  if(auto* editor=slot.effect->createEditor())slot.editor=std::make_unique<PluginEditorWindow>(slot.effect->name(),editor);
 }
 void clearInstrument(int track){
  if(track<0||track>=activeTrackCount)return;
  auto& slot=trackInstruments[track];
  ++slot.revision;slot.editor.reset();slot.effect->clear();slot.pending=juce::var();slot.pendingName.clear();
  status="Track "+juce::String(track+1)+" instrument removed";
  refreshInstrumentControls();updateTrackSelectLabel(track);repaint();
 }
 void beginInstrumentLoad(int track,const juce::PluginDescription& description,juce::var savedState){
  if(track<0||track>=activeTrackCount)return;
  if(!description.isInstrument){status="Choose an instrument plugin";repaint();return;}
  auto& slot=trackInstruments[track];
  const unsigned revision=++slot.revision;
  const bool restoring=!savedState.isVoid();
  slot.editor.reset();
  if(savedState.isVoid()){
   juce::DynamicObject::Ptr target=new juce::DynamicObject();
   if(auto xml=description.createXml())target->setProperty("description",xml->toString());
   target->setProperty("state",juce::String());
   savedState=juce::var(target.get());
  }
  slot.pending=savedState;slot.pendingName=description.name;
  juce::MemoryBlock state;
  if(auto* object=savedState.getDynamicObject())state.fromBase64Encoding(object->getProperty("state").toString());
  status="Loading track "+juce::String(track+1)+" instrument: "+description.name;
  refreshInstrumentControls();
  juce::Component::SafePointer<MainView> safe(this);
  std::weak_ptr<PluginEffect> destination=slot.effect;
  pluginEffect.formats.createPluginInstanceAsync(description,captureSampleRate.load(),effectBlockSize.load(),
   [safe,destination,revision,description,state,restoring](std::unique_ptr<juce::AudioPluginInstance> instance,const juce::String& error){
    if(safe==nullptr)return;
    auto effect=destination.lock();if(effect==nullptr)return;
    const int track=safe->trackInstrumentPosition(effect.get());if(track<0)return;
    auto& slot=safe->trackInstruments[track];if(slot.revision!=revision)return;
    const auto problem=instance==nullptr?"Instrument could not load: "+error:effect->replace(std::move(instance),description,state,false);
    if(problem.isNotEmpty()){
     safe->status=problem;
     if(!restoring){slot.pending=juce::var();slot.pendingName.clear();}
    }else{
     slot.pending=juce::var();slot.pendingName.clear();
     safe->status="Track "+juce::String(track+1)+" instrument: "+description.name;
    }
    safe->refreshInstrumentControls();safe->updateTrackSelectLabel(track);
   });
 }
 void restoreInstrument(int track,const juce::var& saved){
  auto& slot=trackInstruments[track];
  ++slot.revision;slot.editor.reset();slot.effect->clear();slot.pending=saved;slot.pendingName.clear();
  auto* object=saved.getDynamicObject();if(object==nullptr)return;
  auto xml=juce::XmlDocument::parse(object->getProperty("description").toString());
  juce::PluginDescription description;
  if(xml==nullptr||!description.loadFromXml(*xml)||!description.isInstrument){
   status="Saved instrument description is invalid";repaint();return;
  }
  slot.pendingName=description.name;
  beginInstrumentLoad(track,description,saved);
 }
 void updateTrackSelectLabel(int lane){
  if(lane<0||lane>=Engine::maxTracks)return;
  if(lane>=activeTrackCount){trackSelect[lane].setIdentity(lane+1,{},{});return;}
  const auto& instrument=trackInstruments[lane];
  const auto sound=kinds[lane].getSelectedId()==6?juce::String("Backing audio"):kinds[lane].getSelectedId()==5?(instrument.effect->hasPlugin()?instrument.effect->name():instrument.pendingName.isNotEmpty()?"Not loaded: "+instrument.pendingName:"No instrument"):sounds[lane].getText();
  trackSelect[lane].setIdentity(lane+1,trackNames[lane],sound);
  trackSelect[lane].setTooltip("Select track "+juce::String(lane+1)+": "+trackNames[lane]+". Sound: "+sound+". Double-click to rename.");
 }
 void beginRenameTrack(int lane){
  if(lane<0||lane>=activeTrackCount)return;
  if(editingTrack>=0)finishRenameTrack(true);
  selectTrack(lane);
  editingTrack=lane;
  trackNameEditor.setText(trackNames[lane],false);
  trackNameEditor.setBounds(trackSelect[lane].getBounds());
  trackNameEditor.setVisible(true);
  trackNameEditor.toFront(false);
  trackNameEditor.grabKeyboardFocus();
  trackNameEditor.selectAll();
 }
 void finishRenameTrack(bool apply){
  const int lane=editingTrack;
  if(lane<0)return;
  const auto name=trackNameEditor.getText().trim();
  editingTrack=-1;
  trackNameEditor.setVisible(false);
  if(!apply||lane>=activeTrackCount)return;
  if(name.isEmpty()){status="Track name cannot be empty";repaint();return;}
  if(name==trackNames[lane])return;
  trackNames[lane]=name;
  updateTrackSelectLabel(lane);
  status="Renamed track "+juce::String(lane+1)+": "+name;
  repaint();
 }
 void selectTrack(int index){
  selectedTrack=activeTrackCount>0?std::clamp(index,0,activeTrackCount-1):-1;
  if(selectedTrack>=0){fadeTarget.setValue(fadeTargets[selectedTrack],juce::dontSendNotification);fadeSeconds.setValue(fadeDurations[selectedTrack],juce::dontSendNotification);}
  setSize(getWidth(),contentHeight());
  for(int i=0;i<Engine::maxTracks;++i){
   const bool selected=i==selectedTrack;
   trackSelect[i].setToggleState(selected,juce::dontSendNotification);
   trackSelect[i].setColour(juce::TextButton::buttonColourId,juce::Colour(selected?0xff40584e:0xff222727));
   trackSelect[i].setColour(juce::TextButton::textColourOffId,juce::Colour(selected?0xffe9eeeb:0xffa5b2ad));
  }
  refreshTrackPluginControls();refreshInstrumentControls();refreshAppleIdeaControl();resized();repaint();
 }
 void revealSelectedTrackInspector(){
  if(selectedTrack<0)return;
  auto* viewport=findParentComponentOfClass<juce::Viewport>();
  if(viewport==nullptr)return;
  const int rowTop=sequenceTop+selectedTrack*sequenceRowHeight;
  const int fullInspectorY=std::max(0,soundPanelY()+trackPanelHeight()+12-viewport->getViewHeight());
  const int rowVisibleY=std::max(0,rowTop-20);
  viewport->setViewPosition(0,std::min(fullInspectorY,rowVisibleY));
 }
 void setManualTrackLevel(int lane,double value){
  if(lane<0||lane>=Engine::maxTracks)return;
  if(!midiDispatching)midiMappings.resetPickup(MidiMapping::Target::Level,lane);
  engine.setTrackLevel(lane,static_cast<float>(value/100.0));
  levels[lane].setValue(value,juce::dontSendNotification);
  rowLevels[lane].setValue(value,juce::dontSendNotification);
 }
 void setSoloTrack(int index){
  const int solo=index>=0&&index<activeTrackCount?index:-1;
  if(solo>=0)setFocusTrack(-1);
  for(int i=0;i<Engine::maxTracks;++i)solos[i].setToggleState(i==solo,juce::dontSendNotification);
  engine.soloTrack=solo;
  repaint();
 }
 void setFocusTrack(int index){
  const int focus=index>=0&&index<activeTrackCount?index:-1;
  if(focus>=0)setSoloTrack(-1);
  for(int i=0;i<Engine::maxTracks;++i)focuses[i].setToggleState(i==focus,juce::dontSendNotification);
  engine.focusTrack=focus;
  repaint();
 }
 void addNewTrack(){
  if(activeTrackCount>=Engine::maxTracks)return;
  const int lane=activeTrackCount;
  setTrackCount(activeTrackCount+1,true);
  selectTrack(lane);
  if(auto* viewport=findParentComponentOfClass<juce::Viewport>()){
   const int panelBottom=soundPanelY()+trackPanelHeight()+12;
   if(panelBottom>viewport->getViewPositionY()+viewport->getViewHeight())
    viewport->setViewPosition(0,panelBottom-viewport->getViewHeight());
  }
 }
 void cloneSelectedTrack(){
  if(selectedTrack<0||selectedTrack>=activeTrackCount||activeTrackCount>=Engine::maxTracks)return;
  const int from=selectedTrack;
  const int to=activeTrackCount;
  std::array<juce::var,trackEffectCount> effects;
  for(int i=0;i<trackEffectCount;++i){
   const auto& slot=trackEffectSlots[from][i];
   effects[static_cast<size_t>(i)]=!slot.pending.isVoid()?slot.pending:slot.effect->saveState();
  }
  const auto& sourceInstrument=trackInstruments[from];
  const juce::var instrument=!sourceInstrument.pending.isVoid()?sourceInstrument.pending:sourceInstrument.effect->saveState();
  setTrackCount(to+1,false,true);
  copyTrackSettings(from,to);
  engine.copyTrackData(from,to);
  fadeTargets[to]=fadeTargets[from];fadeDurations[to]=fadeDurations[from];
  patternHistory[to]=patternHistory[from];patternHistoryIndex[to]=patternHistoryIndex[from];
  soundHistory[to]=soundHistory[from];soundHistoryIndex[to]=soundHistoryIndex[from];
  juce::String name=trackNames[from]+" copy";
  const auto base=name;
  int copyNumber=2;
  auto exists=[this,to](const juce::String& candidate){
   for(int i=0;i<to;++i)if(trackNames[i]==candidate)return true;
   return false;
  };
  while(exists(name))name=base+" "+juce::String(copyNumber++);
  trackNames[to]=name;
  trackSubtitles[to]=roleSubtitle(roles[to].getSelectedId()-1);
  for(int i=0;i<trackEffectCount;++i)restoreTrackPluginEffect(to,i,effects[static_cast<size_t>(i)]);
  restoreInstrument(to,instrument);
  engine.activeTracks=activeTrackCount;
  selectTrack(to);
  status="Cloned track "+juce::String(from+1)+" as track "+juce::String(to+1);
  if(auto* viewport=findParentComponentOfClass<juce::Viewport>()){
   const int panelBottom=soundPanelY()+trackPanelHeight()+12;
   if(panelBottom>viewport->getViewPositionY()+viewport->getViewHeight())
    viewport->setViewPosition(0,panelBottom-viewport->getViewHeight());
  }
  repaint();
 }
 void setTrackCount(int count,bool isNewDiscovery=false,bool deferAudioActivation=false){
  if(editingTrack>=0)finishRenameTrack(true);
  const int previousCount=activeTrackCount;
  activeTrackCount=std::clamp(count,0,Engine::maxTracks);
  if(engine.soloTrack.load()>=activeTrackCount)setSoloTrack(-1);
  if(engine.focusTrack.load()>=activeTrackCount)setFocusTrack(-1);
  setSize(getWidth(),contentHeight());
  engine.activeTracks=deferAudioActivation?previousCount:activeTrackCount;
  midiMappings.resetPickup();
  if(activeTrackCount==0)engine.playing=false;
  play.setButtonText(activeTrackCount==0?"Discover":engine.playing.load()?"Stop":"Play");
  if(activeTrackCount==0)selectedTrack=-1;
  else if(selectedTrack<0||selectedTrack>=activeTrackCount)selectedTrack=activeTrackCount-1;
  for(int i=previousCount;i<activeTrackCount;++i){
   fadeTargets[i]=0.0;fadeDurations[i]=12.0;fadeWasActive[i]=false;
   if(trackNames[i].trim().isEmpty())trackNames[i]=makeTrackName();
   if(trackSubtitles[i].trim().isEmpty())trackSubtitles[i]=i==0?"Main phrase":roleSubtitle(roles[i].getSelectedId()-1);
   applyNewTrackDefaults(i,isNewDiscovery);
   soundHistory[i]={{kinds[i].getSelectedId(),sounds[i].getSelectedId()}};
   soundHistoryIndex[i]=0;
   engine.generateTrack(i);
   rememberPattern(i,true);
  }
  for(int i=activeTrackCount;i<Engine::maxTracks;++i){
   auto& instrument=trackInstruments[i];
   ++instrument.revision;instrument.editor.reset();instrument.effect->clear();instrument.pending=juce::var();instrument.pendingName.clear();
   for(auto& slot:trackEffectSlots[i]){
    ++slot.revision;slot.editor.reset();slot.effect->clear();slot.pending=juce::var();slot.pendingName.clear();
   }
   engine.cancelTrackFade(i);fadeWasActive[i]=false;
   pendingSoundChanges[i].active=false;
   engine.browseGain[i]=1.0f;
   soundHistory[i].clear();
   soundHistoryIndex[i]=0;
   engine.clearTrack(i);backingFiles[i]=juce::File();backingClips[i].reset();backingTimings[i]=TakeTiming{};backingLoopFlags[i]=false;backingLoops[i].setToggleState(false,juce::dontSendNotification);
   holds[i].setToggleState(false,juce::sendNotification);
   mutes[i].setToggleState(false,juce::sendNotification);
   solos[i].setToggleState(false,juce::dontSendNotification);
   focuses[i].setToggleState(false,juce::dontSendNotification);
  }
  if(activeTrackCount==0)selectTrack(-1);
  else if(selectedTrack<0||selectedTrack>=activeTrackCount)selectTrack(activeTrackCount-1);
  updateHostedInstrumentMask();
  refreshAppleIdeaControl();
  resized();repaint();
 }
 void copyTrackSettings(int from,int to){
  backingFiles[to]=backingFiles[from];backingClips[to]=backingClips[from];backingTimings[to]=backingTimings[from];
  backingLoopFlags[to]=backingLoopFlags[from].load();backingLoops[to].setToggleState(backingLoopFlags[to].load(),juce::dontSendNotification);
	  setLaneSoundWithoutRole(to,kinds[from].getSelectedId(),sounds[from].getSelectedId());
	  materials[to].setSelectedId(materials[from].getSelectedId(),juce::dontSendNotification);engine.material[to]=materials[to].getSelectedId()-1;
	  playModes[to].setSelectedId(playModes[from].getSelectedId(),juce::dontSendNotification);engine.samplePlayMode[to]=playModes[to].getSelectedId()-1;
	  sampleStarts[to].setValue(sampleStarts[from].getValue());sampleSizes[to].setValue(sampleSizes[from].getValue());sampleFades[to].setValue(sampleFades[from].getValue());
	  sampleReverses[to].setToggleState(sampleReverses[from].getToggleState(),juce::sendNotification);
	  roles[to].setSelectedId(roles[from].getSelectedId());relationships[to].setValue(relationships[from].getValue());dynamics[to].setValue(dynamics[from].getValue());articulations[to].setValue(articulations[from].getValue());tails[to].setValue(tails[from].getValue());
	  lengths[to].setSelectedId(lengths[from].getSelectedId());
  loopLengths[to].setValue(loopLengths[from].getValue(),juce::dontSendNotification);engine.setLoopSteps(to,static_cast<int>(loopLengths[to].getValue()));
  setRhythmSettings(to,engine.rhythmMode[from].load(),engine.euclideanHits[from].load(),engine.euclideanRotate[from].load());
  connects[to].setToggleState(connects[from].getToggleState(),juce::sendNotification);
  levels[to].setValue(levels[from].getValue());sends[to].setValue(sends[from].getValue());colors[to].setValue(colors[from].getValue());moves[to].setValue(moves[from].getValue());weathers[to].setValue(weathers[from].getValue());
  setManualTrackLevel(to,levels[to].getValue());
  holds[to].setToggleState(holds[from].getToggleState(),juce::sendNotification);mutes[to].setToggleState(mutes[from].getToggleState(),juce::sendNotification);
 }
 void confirmRemoveTrack(int lane){
  if(lane<0||lane>=activeTrackCount)return;
  const auto name=trackNames[lane];
  juce::Component::SafePointer<MainView> safe(this);
  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
   .withTitle("Remove track?").withMessage("Remove "+juce::String(lane+1)+". "+name+" and its pattern?")
   .withButton("Cancel").withButton("Remove track").withAssociatedComponent(this);
  juce::NativeMessageBox::showAsync(options,[safe,lane,name](int result){if(result==1&&safe!=nullptr&&lane<safe->activeTrackCount&&safe->trackNames[lane]==name)safe->removeTrackAt(lane);});
 }
 void showTrackActionsMenu(){
  const int lane=selectedTrack;
  if(lane<0||lane>=activeTrackCount)return;
  juce::PopupMenu menu;
  menu.addItem(1,"Rename track...");
  menu.addSeparator();
  menu.addItem(2,"Remove track...");
  juce::Component::SafePointer<MainView> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(trackActions),[safe,lane](int result){
   if(safe==nullptr||lane>=safe->activeTrackCount)return;
   if(result==1)safe->beginRenameTrack(lane);
   else if(result==2)safe->confirmRemoveTrack(lane);
  });
 }
 void removeTrackAt(int lane){
  if(editingTrack>=0)finishRenameTrack(true);
  if(lane<0||lane>=activeTrackCount)return;
  reorderingTracks=true;
  engine.hostedInstrumentMask.store(0);
  for(int i=0;i<activeTrackCount;++i)trackInstruments[i].effect->requestAllNotesOff();
  pluginPickerWindow.reset();pickerTarget=nullptr;
  for(int i=0;i<activeTrackCount;++i){
   engine.cancelTrackFade(i);
   const double current=std::round(engine.level[i].load()*100.0f);
   levels[i].setValue(current,juce::dontSendNotification);
   rowLevels[i].setValue(current,juce::dontSendNotification);
   fadeWasActive[i]=false;
  }
  for(int i=0;i<activeTrackCount;++i){pendingSoundChanges[i].active=false;engine.browseGain[i]=1.0f;}
  const int solo=engine.soloTrack.load();
  if(solo>=0)setSoloTrack(solo==lane?-1:(solo>lane?solo-1:solo));
  const int focus=engine.focusTrack.load();
  if(focus>=0)setFocusTrack(focus==lane?-1:(focus>lane?focus-1:focus));
  for(int i=lane;i<activeTrackCount-1;++i){
   copyTrackSettings(i+1,i);
   engine.copyTrackData(i+1,i);
   fadeTargets[i]=fadeTargets[i+1];fadeDurations[i]=fadeDurations[i+1];
   patternHistory[i]=patternHistory[i+1];
   patternHistoryIndex[i]=patternHistoryIndex[i+1];
   soundHistory[i]=soundHistory[i+1];
   soundHistoryIndex[i]=soundHistoryIndex[i+1];
   trackNames[i]=trackNames[i+1];
   trackSubtitles[i]=trackSubtitles[i+1];
  }
  const int last=activeTrackCount-1;
  std::array<TrackEffectSlot,trackEffectCount> removed;
  TrackInstrumentSlot removedInstrument;
  {
   const juce::ScopedLock audioLock(deviceManager.getAudioCallbackLock());
   removed=std::move(trackEffectSlots[lane]);
   removedInstrument=std::move(trackInstruments[lane]);
   for(int i=lane;i<last;++i)trackEffectSlots[i]=std::move(trackEffectSlots[i+1]);
   for(int i=lane;i<last;++i)trackInstruments[i]=std::move(trackInstruments[i+1]);
   trackEffectSlots[last]=std::array<TrackEffectSlot,trackEffectCount>{};
   trackInstruments[last]=TrackInstrumentSlot{};
  }
  for(auto& slot:removed)slot.editor.reset();
  removedInstrument.editor.reset();
  fadeTargets[last]=0.0;fadeDurations[last]=12.0;
  trackNames[last].clear();
  trackSubtitles[last].clear();
  engine.clearTrack(last);backingFiles[last]=juce::File();backingClips[last].reset();backingTimings[last]=TakeTiming{};backingLoopFlags[last]=false;
  patternHistory[last].clear();
  patternHistoryIndex[last]=0;
  soundHistory[last].clear();
  soundHistoryIndex[last]=0;
  activeTrackCount=std::max(0,activeTrackCount-1);
  engine.activeTracks=activeTrackCount;
  reorderingTracks=false;
  updateHostedInstrumentMask();
  midiMappings.resetPickup();
  if(activeTrackCount==0)engine.playing=false;
  play.setButtonText(activeTrackCount==0?"Discover":engine.playing.load()?"Stop":"Play");
  if(activeTrackCount==0)selectTrack(-1);
  else if(selectedTrack>=activeTrackCount)selectTrack(activeTrackCount-1);
  else selectTrack(std::min(lane,activeTrackCount-1));
  refreshTrackPluginControls();
  refreshInstrumentControls();
  resized();repaint();
 }
 void syncRhythmControls(int lane){
  const int length=engine.loopSteps[lane].load();
  rhythms[lane].setSelectedId(engine.rhythmMode[lane].load()+1,juce::dontSendNotification);
  rhythmHits[lane].setRange(0,length,1);rhythmRotations[lane].setRange(0,std::max(1,length-1),1);
  rhythmHits[lane].setValue(engine.euclideanHits[lane].load(),juce::dontSendNotification);
  rhythmRotations[lane].setValue(engine.euclideanRotate[lane].load(),juce::dontSendNotification);
  const bool euclidean=engine.rhythmMode[lane].load()==1;
  rhythmHits[lane].setEnabled(euclidean);rhythmRotations[lane].setEnabled(euclidean&&length>1);
 }
 void setRhythmSettings(int lane,int rhythm,int hits,int rotation,bool apply=false){
  engine.setRhythm(lane,rhythm,hits,rotation,apply);syncRhythmControls(lane);
 }
 void changeRhythm(int lane){
  rememberPattern(lane,true);
  setRhythmSettings(lane,rhythms[lane].getSelectedId()-1,static_cast<int>(rhythmHits[lane].getValue()),static_cast<int>(rhythmRotations[lane].getValue()),true);
  rememberPattern(lane,true);repaint();
 }
 PatternCandidate currentPattern(int lane) const {
  return {engine.getTrackPattern(lane),roles[lane].getSelectedId(),static_cast<int>(relationships[lane].getValue()),static_cast<int>(dynamics[lane].getValue()),static_cast<int>(articulations[lane].getValue()),static_cast<int>(moves[lane].getValue()),static_cast<int>(weathers[lane].getValue()),lengths[lane].getSelectedId(),engine.loopSteps[lane].load(),engine.rhythmMode[lane].load(),engine.euclideanHits[lane].load(),engine.euclideanRotate[lane].load()};
 }
 void rememberPattern(int lane,bool discardForward){
  if(lane<0||lane>=Engine::maxTracks)return;
  const PatternCandidate candidate=currentPattern(lane);
  auto& history=patternHistory[lane];
  int& index=patternHistoryIndex[lane];
  if(discardForward&&index+1<static_cast<int>(history.size()))history.erase(history.begin()+index+1,history.end());
  if(!history.empty()&&history.back()==candidate){index=static_cast<int>(history.size())-1;return;}
  history.push_back(candidate);
  constexpr int maxHistory=24;
  if(static_cast<int>(history.size())>maxHistory)history.erase(history.begin());
  index=static_cast<int>(history.size())-1;
 }
 void restorePatternStep(int direction){
  if(selectedTrack<0||selectedTrack>=activeTrackCount||selectedTrackIsBacking())return;
  auto& history=patternHistory[selectedTrack];
  if(history.empty()){rememberPattern(selectedTrack,false);return;}
  int& index=patternHistoryIndex[selectedTrack];
  index=std::clamp(index+direction,0,static_cast<int>(history.size())-1);
  const auto& candidate=history[static_cast<size_t>(index)];
  roles[selectedTrack].setSelectedId(candidate.role);
  relationships[selectedTrack].setValue(candidate.relationship);
  dynamics[selectedTrack].setValue(candidate.dynamics);
  articulations[selectedTrack].setValue(candidate.articulation);
  moves[selectedTrack].setValue(candidate.motion);
  weathers[selectedTrack].setValue(candidate.texture);
  lengths[selectedTrack].setSelectedId(candidate.length);
  loopLengths[selectedTrack].setValue(candidate.loopSteps,juce::dontSendNotification);
  engine.setLoopSteps(selectedTrack,candidate.loopSteps);
  setRhythmSettings(selectedTrack,candidate.rhythm,candidate.hits,candidate.rotation);
  engine.setTrackPattern(selectedTrack,candidate.phrase);
  status="Restored pattern "+juce::String(index+1)+" / "+juce::String(static_cast<int>(history.size()))+" on track "+juce::String(selectedTrack+1);
  repaint();
 }
 void generateSelectedTrackCandidate(){
  if(selectedTrack<0||selectedTrack>=activeTrackCount||selectedTrackIsBacking())return;
  rememberPattern(selectedTrack,true);
  engine.generateTrack(selectedTrack);
  rememberPattern(selectedTrack,true);
  status="Generated pattern "+juce::String(patternHistoryIndex[selectedTrack]+1)+" / "+juce::String(static_cast<int>(patternHistory[selectedTrack].size()))+" on track "+juce::String(selectedTrack+1);
  repaint();
 }
 void generateAllTrackCandidates(){
  for(int i=0;i<activeTrackCount;++i){
   if(holds[i].getToggleState()||kinds[i].getSelectedId()==6)continue;
   rememberPattern(i,true);
   engine.generateTrack(i);
   rememberPattern(i,true);
  }
  status="Generated patterns for unheld tracks";
  repaint();
 }
 juce::File appleIdeaHelper() const {
  return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("PhraseMixIdeaHelper");
 }
 PieceBaseline currentPieceBaseline() const {
  PieceBaseline baseline;
  baseline.count=activeTrackCount;
  baseline.bpm=static_cast<int>(tempo.getValue());baseline.density=static_cast<int>(density.getValue());
  baseline.key=key.getSelectedId();baseline.scale=scale.getSelectedId();baseline.shape=shape.getSelectedId();
  baseline.mode=mode.getSelectedId();baseline.space=static_cast<int>(wet.getValue());baseline.room=static_cast<int>(room.getValue());
  for(int track=0;track<activeTrackCount;++track){
   baseline.patterns[track]=currentPattern(track);
   baseline.held[track]=holds[track].getToggleState();
  }
  return baseline;
 }
 bool matchesPieceBaseline(const PieceBaseline& baseline) const {
  const auto now=currentPieceBaseline();
  if(now.count!=baseline.count||now.bpm!=baseline.bpm||now.density!=baseline.density
   ||now.key!=baseline.key||now.scale!=baseline.scale||now.shape!=baseline.shape
   ||now.mode!=baseline.mode||now.space!=baseline.space||now.room!=baseline.room)return false;
  for(int track=0;track<now.count;++track){
   if(now.held[track]!=baseline.held[track]||!now.patterns[track].sameControls(baseline.patterns[track]))return false;
   if((baseline.mode==1||baseline.held[track])&&now.patterns[track].phrase!=baseline.patterns[track].phrase)return false;
  }
  return true;
 }
 void refreshAppleIdeaControl(){
  aiIdea.setEnabled(aiProcess!=nullptr?!aiPieceRequest:(aiAvailable&&selectedTrack>=0&&selectedTrack<activeTrackCount));
  aiIdea.setTooltip(aiProcess!=nullptr?(aiPieceRequest?"Another AI request is running":"Cancel the current AI idea"):aiAvailable
   ?"Ask Apple's on-device model for a phrase and expression settings for this track":aiAvailabilityReason);
  aiPiece.setEnabled(aiProcess!=nullptr?aiPieceRequest:aiAvailable);
  aiPiece.setTooltip(aiProcess!=nullptr?(aiPieceRequest?"Cancel the current AI request":"Another AI request is running"):aiAvailable
   ?"Suggest a musical arrangement, adding complementary tracks when needed":aiAvailabilityReason);
 }
 void checkAppleIdeaAvailability(){
  if(aiAvailabilityProcess||aiProcess)return;
  aiLastAvailabilityCheck=juce::Time::getMillisecondCounter();
  const auto helper=appleIdeaHelper();
  if(!helper.existsAsFile()){
   aiAvailable=false;aiAvailabilityReason="AI idea helper is not installed in this app build";
   refreshAppleIdeaControl();return;
  }
  aiAvailabilityProcess=std::make_unique<juce::ChildProcess>();
  if(!aiAvailabilityProcess->start(juce::StringArray{helper.getFullPathName(),"--availability"},juce::ChildProcess::wantStdOut)){
   aiAvailabilityProcess.reset();aiAvailable=false;aiAvailabilityReason="Could not check Apple Intelligence";
   refreshAppleIdeaControl();return;
  }
  if(!aiAvailable){aiAvailabilityReason="Checking Apple Intelligence...";refreshAppleIdeaControl();}
 }
 void finishAppleIdeaAvailability(){
  const auto result=juce::JSON::parse(aiAvailabilityProcess->readAllProcessOutput());
  aiAvailabilityProcess.reset();
  const auto* object=result.getDynamicObject();
  const auto ready=object!=nullptr?object->getProperty("available"):juce::var();
  aiAvailable=ready.isBool()&&static_cast<bool>(ready);
  aiAvailabilityReason=aiAvailable?"":object!=nullptr?object->getProperty("error").toString():"Could not check Apple Intelligence";
  if(aiAvailabilityReason.isEmpty()&&!aiAvailable)aiAvailabilityReason="Apple's on-device model is unavailable";
  refreshAppleIdeaControl();
 }
 void showAppleIdeaPrompt(bool piece=false){
  if(!aiAvailable||(!piece&&(selectedTrack<0||selectedTrack>=activeTrackCount||selectedTrackIsBacking())))return;
  auto* dialog=new juce::AlertWindow(piece?"AI piece":"AI track for "+trackNames[selectedTrack],
   piece?"Describe a direction. Existing tracks stay; complementary tracks may be added. Keep your piece first if you want it back.":"Describe the phrase or sound you want to explore.",juce::MessageBoxIconType::NoIcon);
  dialog->addTextEditor("direction","A spacious, slowly evolving phrase","Direction");
  dialog->addButton("Create idea",1,juce::KeyPress(juce::KeyPress::returnKey));
  dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
  const juce::Component::SafePointer<MainView> safe(this);
  dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,piece](int result){
   if(result==1&&safe!=nullptr)safe->startAppleIdea(dialog->getTextEditorContents("direction"),piece);
  }),true);
 }
 void cancelAppleIdea(){
  if(aiProcess)aiProcess->kill();
  aiProcess.reset();aiTrack=-1;aiPieceRequest=false;aiPieceDroneRequested=false;aiIdea.setButtonText("AI idea");aiPiece.setButtonText("AI piece");
  refreshAppleIdeaControl();
  status="AI idea cancelled";repaint();
 }
 void startAppleIdea(juce::String direction,bool piece){
  if(!aiAvailable||(!piece&&(selectedTrack<0||selectedTrack>=activeTrackCount||selectedTrackIsBacking()))||aiProcess)return;
  direction=direction.trim().substring(0,240);
  if(direction.isEmpty()){status="Describe an idea first";repaint();return;}
  const auto helper=appleIdeaHelper();
  if(!helper.existsAsFile()){status="AI idea helper is not installed in this app build";repaint();return;}
  aiPieceRequest=piece;aiPieceBaseline=currentPieceBaseline();
  const auto lowerDirection=direction.toLowerCase();
  aiPieceDroneRequested=piece&&(direction.containsWholeWordIgnoreCase("drone")||direction.containsWholeWordIgnoreCase("drones"))
   &&!lowerDirection.contains("no drone")&&!lowerDirection.contains("not a drone")
   &&!lowerDirection.contains("without drone")&&!lowerDirection.contains("without a drone");
  aiTrack=-1;aiTrackName.clear();aiScale=scale.getSelectedId()-1;aiKey=key.getSelectedId()-1;
  juce::String context;
  if(!piece){
   aiTrack=selectedTrack;aiTrackName=trackNames[aiTrack];aiBaseline=currentPattern(aiTrack);
   const auto phrase=engine.getTrackPhrase(aiTrack);
   juce::StringArray notes;
   for(const int note:phrase)notes.add(juce::String(note));
   context="Track "+juce::String(aiTrack+1)+(aiTrack==0?" (main phrase)":" (follower)")
    +", key "+key.getText()+", scale "+scale.getText()+", current MIDI notes ["+notes.joinIntoString(",")+"]"
    +", current role "+roles[aiTrack].getText()+", relationship "+juce::String(static_cast<int>(relationships[aiTrack].getValue()))
    +". Scale degrees must be -1 for rest or 0 through "+juce::String(aiScale==0?9:13)+" for a note.";
  }
  juce::String pieceContext="Current piece: "+juce::String(activeTrackCount)+" tracks, "+juce::String(static_cast<int>(tempo.getValue()))
   +" BPM, density "+juce::String(static_cast<int>(density.getValue()))+"%, key "+key.getText()+", scale "+scale.getText()+". ";
  for(int lane=0;lane<activeTrackCount;++lane)
   pieceContext+="Track "+juce::String(lane+1)+" "+trackNames[lane]+" ("+(lane==0?"main":roles[lane].getText())
    +(holds[lane].getToggleState()?", held":", unheld")+"). ";
  juce::StringArray arguments{helper.getFullPathName()};
  if(piece)arguments.add("--piece");
  arguments.add(direction);
  arguments.add(piece?pieceContext:context);
  aiProcess=std::make_unique<juce::ChildProcess>();
  if(!aiProcess->start(arguments,juce::ChildProcess::wantStdOut)){
   aiProcess.reset();aiTrack=-1;aiPieceRequest=false;aiPieceDroneRequested=false;status="Could not start Apple's on-device model";repaint();return;
  }
  aiStartedAt=juce::Time::getMillisecondCounter();
  (piece?aiPiece:aiIdea).setButtonText("Cancel AI");refreshAppleIdeaControl();
  status=piece?"Making a whole-piece idea...":"Making an AI idea for track "+juce::String(aiTrack+1)+"...";repaint();
 }
 void reviewApplePiece(ApplePieceIdea idea,bool fallback=false){
  idea.makePerformable();
  int available=0;
  for(int lane=0;lane<activeTrackCount;++lane)if(!holds[lane].getToggleState()&&kinds[lane].getSelectedId()!=6)++available;
  const int layers=idea.form==1?idea.trackCount:0;
  const int target=idea.targetTrackCount(activeTrackCount,Engine::maxTracks,available);
  if(idea.form==1&&available+target-activeTrackCount<layers){status="Release a held track to make room for the drone";repaint();return;}
  const int added=target-activeTrackCount;
  int changing=0;
  for(int lane=0;lane<activeTrackCount;++lane)if(!holds[lane].getToggleState()&&kinds[lane].getSelectedId()!=6)++changing;
  const juce::String keys[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
  const juce::String scales[]={"minor pentatonic","natural minor","major"};
  const juce::String roleNames[]={"Shadow","Contrast","Sparse","Free"};
  const juce::String paletteNames[]={"Soft and open","Bright and chiming","Acoustic and warm","Dark and drifting"};
  const juce::String sceneNames[]={"familiar","textured","contrasting"};
  juce::StringArray newParts;
  for(int lane=activeTrackCount;lane<target;++lane)
   newParts.add("track "+juce::String(lane+1)+" "+(idea.form==1?"Drone":lane==0?"Main":roleNames[idea.roleForFollower(lane-1)]));
  const juce::String additions=added>0?"Add "+juce::String(added)+" "+(added==1?"track":"tracks")+": "+newParts.joinIntoString(", ")+".\n":"No tracks added or removed.\n";
  const juce::String structure=idea.form==1
   ?"Drone: "+juce::String(layers)+" continuous "+(layers==1?"layer":"layers")+"; nearby notes wander every four cycles. "+juce::String(idea.droneMutedCount(available))+" other unheld "+(idea.droneMutedCount(available)==1?"track":"tracks")+" muted and held, not deleted.\n"
   :"Main motif: "+juce::String(idea.mainNoteCount)+" notes.\n";
  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
   .withTitle(fallback?"Apply basic drone?":"Apply AI piece idea?")
   .withMessage(juce::String(fallback?"Apple Intelligence could not make an idea, so this is a basic drone.\n":"")
     +"Tempo "+juce::String(idea.tempo)+" BPM, density "+juce::String(idea.density)+"%, "+keys[idea.key]+" "+scales[idea.scale]
     +". Space "+juce::String(idea.spaceReturn)+"%, Room "+juce::String(idea.roomSize)+"%.\n"
     +structure+"Palette: "+paletteNames[idea.palette]+", "+sceneNames[idea.soundScene]+".\n"
     +additions+(idea.form==1?"Sustain unheld drone layers. Refresh built-ins selects sustaining sounds; kept plugins depend on their own sustain, and selected samples use Bed.":"Rework "+juce::String(changing)+" existing unheld "+(changing==1?"track":"tracks")+".")+" Held tracks, plugin chains, captured samples, and levels stay. Shared Space and Room change.\n"
     +"Keep the current piece first if you may want it back.")
   .withButton("Cancel").withButton("Keep sounds").withButton("Refresh built-ins").withAssociatedComponent(this);
  const auto baseline=aiPieceBaseline;
  const juce::Component::SafePointer<MainView> safe(this);
  juce::NativeMessageBox::showAsync(options,[safe,idea,baseline](int result){
   if((result==1||result==2)&&safe!=nullptr)safe->applyApplePiece(idea,baseline,result==2);
  });
  status=fallback?"Review the basic drone before applying":"Review the AI piece idea before applying";repaint();
 }
 void finishAppleIdea(){
  if(!aiProcess)return;
  const juce::String output=aiProcess->readAllProcessOutput();
  aiProcess.reset();aiIdea.setButtonText("AI idea");aiPiece.setButtonText("AI piece");
  const bool piece=aiPieceRequest;aiPieceRequest=false;
  const bool droneRequested=aiPieceDroneRequested;aiPieceDroneRequested=false;
  refreshAppleIdeaControl();
  const int track=aiTrack;aiTrack=-1;
  if(piece?!matchesPieceBaseline(aiPieceBaseline):(track<0||track>=activeTrackCount||trackNames[track]!=aiTrackName||scale.getSelectedId()-1!=aiScale||key.getSelectedId()-1!=aiKey||!(currentPattern(track)==aiBaseline))){
   status=piece?"The piece changed while AI was working; idea was not applied":"Track changed while AI was working; idea was not applied";repaint();return;
  }
  const auto parsed=juce::JSON::parse(output);
  const auto* object=parsed.getDynamicObject();
  if(object==nullptr){
   if(piece&&droneRequested){reviewApplePiece(ApplePieceIdea::fallbackDrone(key.getSelectedId()-1),true);return;}
   status="Apple's model returned an unreadable idea";repaint();return;
  }
  const auto error=object->getProperty("error").toString();
  if(error.isNotEmpty()){
   if(piece&&droneRequested){reviewApplePiece(ApplePieceIdea::fallbackDrone(key.getSelectedId()-1),true);return;}
   status=error;repaint();return;
  }
  auto integer=[object](const char* name,int& value){
   const auto item=object->getProperty(name);
   if(!item.isInt()&&!item.isInt64())return false;
   value=static_cast<int>(item);return true;
  };
  if(piece){
   auto pieceError=[this,droneRequested](const juce::String& message){
    if(droneRequested)reviewApplePiece(ApplePieceIdea::fallbackDrone(key.getSelectedId()-1),true);
    else {status=message;repaint();}
   };
   ApplePieceIdea idea;
   if(!integer("form",idea.form)){pieceError("Apple's model omitted the piece form");return;}
   if(droneRequested)idea.form=1;
   if(!integer("trackCount",idea.trackCount)
    ||!integer("follower1Role",idea.followerRoles[0])
    ||!integer("follower2Role",idea.followerRoles[1])
    ||!integer("follower3Role",idea.followerRoles[2])
    ||!integer("mainNoteCount",idea.mainNoteCount)||!integer("palette",idea.palette)
    ||!integer("soundScene",idea.soundScene)
    ||!integer("tempo",idea.tempo)||!integer("density",idea.density)||!integer("key",idea.key)
    ||!integer("scale",idea.scale)||!integer("shape",idea.shape)||!integer("spaceReturn",idea.spaceReturn)
    ||!integer("roomSize",idea.roomSize)||!integer("relationship",idea.relationship)
    ||!integer("dynamics",idea.dynamics)||!integer("articulation",idea.articulation)
    ||!integer("motion",idea.motion)||!integer("texture",idea.texture)||!integer("length",idea.length)
    ||!idea.isValid()){
    pieceError("Apple's model returned a piece idea outside the available controls");return;
   }
   const auto steps=object->getProperty("mainSteps");
   const auto* values=steps.getArray();
   if(values==nullptr||values->size()!=Engine::steps){pieceError("Apple's model did not return a 16-step main motif");return;}
   for(int step=0;step<Engine::steps;++step){
    const auto& value=values->getReference(step);
    if(!value.isInt()&&!value.isInt64()){pieceError("Apple's model returned an invalid motif step");return;}
    const int degree=static_cast<int>(value);
    idea.mainDegrees[static_cast<size_t>(step)]=degree<0?-1:degree%(idea.scale==0?10:14);
   }
   if(!idea.hasPlayablePlan()){pieceError("Apple's model did not make a playable main motif");return;}
   reviewApplePiece(idea);return;
  }
  const auto steps=object->getProperty("steps");
  const auto* values=steps.getArray();
  if(values==nullptr||values->size()!=Engine::steps){status="Apple's model did not return 16 steps";repaint();return;}
  AppleIdea idea;
  if(!integer("noteCount",idea.noteCount)){status="Apple's model omitted the note count";repaint();return;}
  for(int i=0;i<Engine::steps;++i){
   const auto& item=values->getReference(i);
   if(!item.isInt()&&!item.isInt64()){status="Apple's model returned an invalid step";repaint();return;}
   idea.degrees[static_cast<size_t>(i)]=static_cast<int>(item);
  }
  if(!integer("role",idea.role)||!integer("relationship",idea.relationship)||!integer("dynamics",idea.dynamics)
   ||!integer("articulation",idea.articulation)||!integer("motion",idea.motion)||!integer("texture",idea.texture)
   ||!integer("length",idea.length)||!idea.isValid(aiScale)){
   status="Apple's model returned an idea outside this track's range";repaint();return;
  }
  idea.makePerformable();
  rememberPattern(track,true);
  if(track>0){roles[track].setSelectedId(idea.role+1);relationships[track].setValue(idea.relationship);}
  dynamics[track].setValue(idea.dynamics);articulations[track].setValue(idea.articulation);
  moves[track].setValue(idea.motion);weathers[track].setValue(idea.texture);lengths[track].setSelectedId(idea.length+1);
  setRhythmSettings(track,0,engine.euclideanHits[track].load(),engine.euclideanRotate[track].load());
  engine.setTrackPhrase(track,idea.midiPhrase(aiKey,aiScale));
  rememberPattern(track,true);
  status="AI idea applied to track "+juce::String(track+1)+"; use < Pattern to return";repaint();
 }
 void applyApplePiece(const ApplePieceIdea& idea,const PieceBaseline& baseline,bool refreshSounds){
  if(!matchesPieceBaseline(baseline)){
   status="The piece changed before the AI idea was applied";repaint();return;
  }
  const int previousCount=activeTrackCount;
  int available=0;
  for(int lane=0;lane<previousCount;++lane)if(!holds[lane].getToggleState()&&kinds[lane].getSelectedId()!=6)++available;
  const int layers=idea.form==1?idea.trackCount:0;
  const int target=idea.targetTrackCount(previousCount,Engine::maxTracks,available);
  if(idea.form==1&&available+target-previousCount<layers){status="Release a held track to make room for the drone";repaint();return;}
  tempo.setValue(idea.tempo);density.setValue(idea.density);
  key.setSelectedId(idea.key+1);scale.setSelectedId(idea.scale+1);shape.setSelectedId(idea.shape+1);
  mode.setSelectedId(idea.form==1?2:1);
  if(idea.form==1)changeEvery.setValue(4);
  wet.setValue(idea.spaceReturn);room.setValue(idea.roomSize);
  if(target>previousCount)setTrackCount(target,false,true);
  int changing=0;
  int droneLayer=0;
  for(int lane=0;lane<activeTrackCount;++lane){
   if(holds[lane].getToggleState()||kinds[lane].getSelectedId()==6)continue;
   if(idea.form==1&&droneLayer>=layers){
    mutes[lane].setToggleState(true,juce::sendNotification);
    holds[lane].setToggleState(true,juce::sendNotification);
    continue;
   }
   if(idea.form==1)mutes[lane].setToggleState(false,juce::sendNotification);
   rememberPattern(lane,true);
   setRhythmSettings(lane,0,engine.euclideanHits[lane].load(),engine.euclideanRotate[lane].load());
   const int role=lane>0?idea.roleForFollower(lane-1):0;
   if(lane>0){
    roles[lane].setSelectedId(role+1);
    const int offset=role==0?-18:role==1?18:role==2?-8:12;
    relationships[lane].setValue(std::clamp(idea.relationship+offset,0,100));
   }
   dynamics[lane].setValue(std::clamp(idea.dynamics+(role==1?8:role==2?-8:0),0,100));
   articulations[lane].setValue(idea.articulation);
   moves[lane].setValue(std::clamp(idea.motion+(role==3?14:role==2?-12:0),0,100));
   weathers[lane].setValue(std::clamp(idea.texture+(lane>0&&role==3?12:lane>0&&role==0?-8:0),0,100));
   const int length=idea.form==1?3:lane>0&&role==2?std::max(idea.length,2):lane>0&&role==1?std::max(0,idea.length-1):idea.length;
   lengths[lane].setSelectedId(length+1);
   if(idea.form==1)connects[lane].setToggleState(true,juce::sendNotification);
   const bool added=lane>=previousCount;
   const int kind=kinds[lane].getSelectedId();
   if(added||(refreshSounds&&(kind==1||kind==2||kind==4))){
    const auto recipe=idea.form==1?AIPieceSounds::chooseDrone(idea.palette,idea.soundScene,droneLayer)
     :AIPieceSounds::choose(idea.palette,idea.soundScene,lane,role);
    setLaneSoundWithoutRole(lane,recipe.kind,recipe.sound);
    colors[lane].setValue(recipe.color);
    sends[lane].setValue(recipe.send);
    tails[lane].setValue(recipe.tail);
    if(added)setManualTrackLevel(lane,recipe.level);
    if(added){soundHistory[lane].clear();soundHistoryIndex[lane]=0;}
    rememberSoundChoice(lane);
   }
   if(idea.form==1){
    if(kinds[lane].getSelectedId()==3)playModes[lane].setSelectedId(3);
    engine.setTrackPhrase(lane,idea.dronePhrase(droneLayer));
    ++droneLayer;
   }else if(lane==0)engine.setTrackPhrase(0,idea.mainPhrase());
   else{
    const auto main=engine.getTrackPhrase(0);
    if(std::any_of(main.begin(),main.end(),[](int note){return note>=0;}))
     engine.setTrackPhrase(lane,idea.followerPhrase(main,role,lane-1));
    else engine.generateTrack(lane);
   }
   rememberPattern(lane,true);
   ++changing;
  }
  if(target>previousCount)engine.activateTracks(activeTrackCount);
  if(previousCount==0)selectTrack(0);
  status="AI piece: "+juce::String(target-previousCount)+" added, "+juce::String(changing)+(idea.form==1?" drone layers":" phrases arranged")
   +(refreshSounds?", built-in sounds refreshed":"");
  repaint();
 }
 int trackAt(juce::Point<float> p) const {const int t=static_cast<int>((p.y-sequenceTop)/sequenceRowHeight);return p.y>=sequenceTop&&t>=0&&t<activeTrackCount&&p.y<sequenceTop+t*sequenceRowHeight+29?t:-1;}
 int stepAt(juce::Point<float> p) const {const int noteWidth=std::max(10,(getWidth()-600)/Engine::steps);const int s=static_cast<int>((p.x-190)/noteWidth);return p.x>=190&&s>=0&&s<Engine::steps&&p.x<190+s*noteWidth+noteWidth-3?s:-1;}
 std::array<juce::TextButton,Engine::maxTracks> holds,mutes,solos,focuses;
 std::array<juce::ToggleButton,Engine::maxTracks> connects,sampleReverses;
 std::array<juce::ComboBox,Engine::maxTracks> kinds,sounds,sources,materials,playModes,roles,lengths,librarySounds,instruments,rhythms;
 std::array<juce::Slider,Engine::maxTracks> levels,sends,colors,moves,weathers,relationships,dynamics,articulations,tails,sampleStarts,sampleSizes,sampleFades,loopLengths,rhythmHits,rhythmRotations;
 juce::String status;
 juce::AudioFormatManager audioFormats;
 juce::StringArray sampleDisplayNames,sampleGroups;
 CaptureTap captureTap;
 juce::AudioBuffer<float> returnBuffer;
 PluginEffect pluginEffect;
 struct TrackEffectSlot {
  std::shared_ptr<PluginEffect> effect=std::make_shared<PluginEffect>();
  std::unique_ptr<PluginEditorWindow> editor;
  juce::var pending;
  juce::String pendingName;
  unsigned revision=0;
 };
 std::array<std::array<TrackEffectSlot,trackEffectCount>,Engine::maxTracks> trackEffectSlots;
 struct TrackInstrumentSlot {
  std::shared_ptr<PluginEffect> effect=std::make_shared<PluginEffect>(true);
  std::unique_ptr<PluginEditorWindow> editor;
  juce::var pending;
  juce::String pendingName;
  unsigned revision=0;
 };
 std::array<TrackInstrumentSlot,Engine::maxTracks> trackInstruments;
 std::array<juce::MidiBuffer,Engine::maxTracks> hostedMidiBlocks;
 bool reorderingTracks=false;
 std::unique_ptr<PluginPickerWindow> pluginPickerWindow;
 PluginEffect* pickerTarget=nullptr;
 std::unique_ptr<PluginEditorWindow> pluginEditorWindow;
 juce::var pendingEffectState;
 juce::String pendingEffectName;
 unsigned effectLoadRevision=0;
 std::atomic<int> effectBlockSize{512};
 std::atomic<bool> audioReturnEnabled{false};
 std::atomic<float> audioReturnGain{0.7f},audioReturnPeak{0.0f};
 std::atomic<int> activeInputChannels{0};
 float returnMeterLevel=0.0f;
 std::vector<float> captureBuffer;
 juce::File lastRecordingFile;
 juce::File lastMixFile;
 juce::Array<juce::File> takeFiles;
 double selectedTakeDuration=1.0;
 double selectedTakeRate=48000.0;
 juce::CriticalSection takePlaybackLock;
 std::vector<float> takePlaybackBuffer;
 std::atomic<bool> takePlaying{false};
 int takePlaybackFrames=0;
 int takePlaybackPosition=0;
 BackingTracks backingAudio;
 BackingTracks::Clips backingClips{};
 std::array<juce::File,Engine::maxTracks> backingFiles{};
 std::array<TakeTiming,Engine::maxTracks> backingTimings{};
 std::array<std::atomic<bool>,Engine::maxTracks> backingLoopFlags{};
 std::array<juce::ToggleButton,Engine::maxTracks> backingLoops;
 std::unique_ptr<juce::FileChooser> backingFileChooser;
 TakeTiming recordingTiming;
 juce::TimeSliceThread recordWriterThread{"Take writer"};
 std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> mixWriter;
 juce::File recordingMixFile;
 std::atomic<bool> mixWriterInCallback{false};
 std::atomic<int64_t> mixRecordedFrames{0};
 std::atomic<int> mixDroppedBlocks{0};
 bool mixFinishPending=false;
 std::atomic<bool> recordingMix{false};
 double mixRecordSampleRate=48000.0;
 int lastRecordingSoundId=0;
 float captureMeterLevel=0.0f;
 std::atomic<bool> recordingSound{false};
 std::atomic<double> captureSampleRate{48000.0};
 struct MidiQueuedEvent {int inputIndex,channel;bool note;int number,value;};
 struct MidiInputConnection final : juce::MidiInputCallback {
  MainView& owner;
  juce::String identifier;
  int index;
  std::unique_ptr<juce::MidiInput> input;
  MidiInputConnection(MainView& view,juce::String id,int inputIndex):owner(view),identifier(std::move(id)),index(inputIndex){}
  ~MidiInputConnection() override {if(input!=nullptr)input->stop();}
  void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& message) override {
   if(!message.isController()&&!message.isNoteOnOrOff())return;
   const MidiQueuedEvent event{index,message.getChannel(),message.isNoteOnOrOff(),message.isController()?message.getControllerNumber():message.getNoteNumber(),message.isController()?message.getControllerValue():(message.isNoteOn()?message.getVelocity():0)};
   std::unique_lock<std::mutex> lock(owner.midiQueueMutex,std::try_to_lock);
   if(lock.owns_lock()&&owner.midiQueueCount<owner.midiQueue.size())owner.midiQueue[owner.midiQueueCount++]=event;
  }
 };
 MidiMapping midiMappings;
 bool midiDispatching=false;
 juce::String midiOutputIdentifier;
 int midiOutputTrack=0;
 std::unique_ptr<juce::MidiOutput> midiOutput;
 juce::MidiBuffer midiBlock;
 std::array<std::unique_ptr<juce::MidiOutput>,Engine::maxTracks> trackMidiOutputs{};
 std::array<juce::MidiBuffer,Engine::maxTracks> trackMidiBlocks{};
 juce::StringArray enabledMidiDevices;
 std::vector<std::unique_ptr<MidiInputConnection>> midiInputs;
 std::mutex midiQueueMutex;
 std::array<MidiQueuedEvent,128> midiQueue{};
 size_t midiQueueCount=0;
 juce::File midiSettingsFile() const {return appDataDirectory().getChildFile("MidiControls.json");}
 static juce::String midiTargetName(MidiMapping::Target target,int track){
  const char* names[]={"Level","Mute","Hold","Focus","Relationship","Dynamics","Articulation","Generate track","Pattern back","Pattern forward","Keep","Play / Stop"};
  const auto name=juce::String(names[static_cast<int>(target)]);
  return track>=0?"Track "+juce::String(track+1)+" "+name:name;
 }
 void refreshMidiInputs(){
  for(auto& connection:midiInputs)if(connection->input!=nullptr)connection->input->stop();
  {std::lock_guard<std::mutex> lock(midiQueueMutex);midiQueueCount=0;}
  midiInputs.clear();
  for(const auto& device:juce::MidiInput::getAvailableDevices()){
   if(!enabledMidiDevices.contains(device.identifier))continue;
   auto connection=std::make_unique<MidiInputConnection>(*this,device.identifier,static_cast<int>(midiInputs.size()));
   connection->input=juce::MidiInput::openDevice(device.identifier,connection.get());
   if(connection->input!=nullptr){midiInputs.push_back(std::move(connection));midiInputs.back()->input->start();}
  }
 }
 void panicMidiOutput(){
  if(midiOutput!=nullptr){
   midiOutput->clearAllPendingMessages();
   for(int channel=1;channel<=Engine::maxTracks;++channel)midiOutput->sendMessageNow(juce::MidiMessage::allNotesOff(channel));
  }
  for(auto& output:trackMidiOutputs)if(output!=nullptr){
   output->clearAllPendingMessages();
   output->sendMessageNow(juce::MidiMessage::allNotesOff(1));
  }
 }
 bool refreshMidiOutput(){
  std::unique_ptr<juce::MidiOutput> next;
  std::array<std::unique_ptr<juce::MidiOutput>,Engine::maxTracks> nextTracks{};
  if(midiOutputIdentifier=="virtual-tracks"){
   for(int track=0;track<Engine::maxTracks;++track){
    nextTracks[static_cast<size_t>(track)]=juce::MidiOutput::createNewDevice("Phrase Mix Track "+juce::String(track+1));
    if(nextTracks[static_cast<size_t>(track)]==nullptr){status="Could not create MIDI track ports";return false;}
   }
  }else if(midiOutputIdentifier=="virtual")next=juce::MidiOutput::createNewDevice("Phrase Mix");
  else if(midiOutputIdentifier.isNotEmpty())next=juce::MidiOutput::openDevice(midiOutputIdentifier);
  if(midiOutputIdentifier.isNotEmpty()&&midiOutputIdentifier!="virtual-tracks"&&next==nullptr){status="Could not open MIDI output";return false;}
  if(next!=nullptr)next->startBackgroundThread();
  for(auto& output:nextTracks)if(output!=nullptr)output->startBackgroundThread();
  {
   const juce::ScopedLock lock(deviceManager.getAudioCallbackLock());
   engine.midiOutputTrack=-2;
   panicMidiOutput();
   midiOutput=std::move(next);
   trackMidiOutputs=std::move(nextTracks);
   engine.midiResetRequested=true;
   engine.midiOutputTrack=trackMidiOutputs[0]!=nullptr?-1:midiOutput!=nullptr?midiOutputTrack:-2;
  }
  return true;
 }
 void saveMidiSettings(){
  juce::DynamicObject::Ptr root=new juce::DynamicObject();
  juce::Array<juce::var> devicesArray,mappingArray;
  for(const auto& id:enabledMidiDevices)devicesArray.add(id);
  for(const auto& binding:midiMappings.all()){
   juce::DynamicObject::Ptr item=new juce::DynamicObject();
   item->setProperty("device",juce::String(binding.address.device));
   item->setProperty("channel",binding.address.channel);
   item->setProperty("note",binding.address.note);
   item->setProperty("number",binding.address.number);
   item->setProperty("target",static_cast<int>(binding.target));
   item->setProperty("track",binding.track);
   mappingArray.add(juce::var(item.get()));
  }
  root->setProperty("devices",juce::var(devicesArray));
  root->setProperty("mappings",juce::var(mappingArray));
  root->setProperty("outputDevice",midiOutputIdentifier);
  root->setProperty("outputTrack",midiOutputTrack);
  const auto file=midiSettingsFile();
  file.getParentDirectory().createDirectory();
  if(!file.replaceWithText(juce::JSON::toString(juce::var(root.get()),true)))status="Could not save MIDI mappings";
 }
 void loadMidiSettings(){
  const auto file=midiSettingsFile();
  if(!file.existsAsFile())return;
  const auto data=juce::JSON::parse(file);
  const auto* root=data.getDynamicObject();
  if(root==nullptr)return;
  midiOutputIdentifier=root->getProperty("outputDevice").toString();
  midiOutputTrack=std::clamp(static_cast<int>(root->getProperty("outputTrack")), -1,Engine::maxTracks-1);
  if(auto* devicesArray=root->getProperty("devices").getArray())for(const auto& id:*devicesArray)enabledMidiDevices.addIfNotAlreadyThere(id.toString());
  if(auto* mappingArray=root->getProperty("mappings").getArray())for(const auto& entry:*mappingArray){
   const auto* item=entry.getDynamicObject();
   if(item==nullptr)continue;
   const int target=static_cast<int>(item->getProperty("target"));
   const int track=static_cast<int>(item->getProperty("track"));
   const int channel=static_cast<int>(item->getProperty("channel"));
   const int number=static_cast<int>(item->getProperty("number"));
   const auto device=item->getProperty("device").toString();
   if(target<0||target>static_cast<int>(MidiMapping::Target::Play)||track<-1||track>=Engine::maxTracks||channel<1||channel>16||number<0||number>127||device.isEmpty())continue;
   if((target<=static_cast<int>(MidiMapping::Target::Articulation))!=(track>=0))continue;
   if(target==static_cast<int>(MidiMapping::Target::Relationship)&&track==0)continue;
   midiMappings.add({device.toStdString(),channel,static_cast<bool>(item->getProperty("note")),number},static_cast<MidiMapping::Target>(target),track);
  }
 }
 void showMidiMenu(){
  const auto available=juce::MidiInput::getAvailableDevices();
  const auto outputs=juce::MidiOutput::getAvailableDevices();
  juce::PopupMenu menu,inputs,learn,remove,outputDevices,outputTracks;
  outputDevices.addItem(10,"Off",true,midiOutputIdentifier.isEmpty());
  outputDevices.addItem(11,"Phrase Mix virtual port",true,midiOutputIdentifier=="virtual");
  outputDevices.addItem(12,"Separate track ports (1-8)",true,midiOutputIdentifier=="virtual-tracks");
  for(int i=0;i<outputs.size();++i)outputDevices.addItem(3000+i,outputs[i].name,true,midiOutputIdentifier==outputs[i].identifier);
  outputTracks.addItem(20,"All tracks (channels 1-8)",true,midiOutputTrack==-1);
  for(int i=0;i<Engine::maxTracks;++i)outputTracks.addItem(21+i,"Track "+juce::String(i+1)+" (channel "+juce::String(i+1)+")",true,midiOutputTrack==i);
  menu.addSubMenu("Send notes to",outputDevices);
  menu.addSubMenu("Send from",outputTracks,midiOutputIdentifier!="virtual-tracks");
  menu.addSeparator();
  if(available.isEmpty())inputs.addItem(99,"No MIDI inputs connected",false);
  for(int i=0;i<available.size();++i)inputs.addItem(100+i,available[i].name,true,enabledMidiDevices.contains(available[i].identifier));
  menu.addSubMenu("Input devices",inputs);
  menu.addItem(2,"Refresh inputs");
  menu.addSeparator();
  std::vector<std::pair<MidiMapping::Target,int>> targets;
  auto addTarget=[&](juce::PopupMenu& destination,MidiMapping::Target target,int track){
   const int id=1000+static_cast<int>(targets.size());
   targets.emplace_back(target,track);
   destination.addItem(id,midiTargetName(target,track));
  };
  addTarget(learn,MidiMapping::Target::Play,-1);
  addTarget(learn,MidiMapping::Target::Keep,-1);
  addTarget(learn,MidiMapping::Target::Generate,-1);
  addTarget(learn,MidiMapping::Target::Back,-1);
  addTarget(learn,MidiMapping::Target::Forward,-1);
  for(int i=0;i<Engine::maxTracks;++i){
   juce::PopupMenu trackMenu;
   addTarget(trackMenu,MidiMapping::Target::Level,i);
   addTarget(trackMenu,MidiMapping::Target::Mute,i);
   addTarget(trackMenu,MidiMapping::Target::Hold,i);
   addTarget(trackMenu,MidiMapping::Target::Focus,i);
   if(i>0)addTarget(trackMenu,MidiMapping::Target::Relationship,i);
   addTarget(trackMenu,MidiMapping::Target::Dynamics,i);
   addTarget(trackMenu,MidiMapping::Target::Articulation,i);
   learn.addSubMenu("Track "+juce::String(i+1),trackMenu);
  }
  menu.addSubMenu("Learn control",learn,!midiInputs.empty());
  if(midiMappings.learning())menu.addItem(1,"Cancel MIDI Learn");
  for(size_t i=0;i<midiMappings.all().size();++i){
   const auto& binding=midiMappings.all()[i];
   const auto source=juce::String(binding.address.note?"Note ":"CC ")+juce::String(binding.address.number)+" ch "+juce::String(binding.address.channel);
   remove.addItem(2000+static_cast<int>(i),midiTargetName(binding.target,binding.track)+"  :  "+source);
  }
  menu.addSubMenu("Remove mapping",remove,!midiMappings.all().empty());
  juce::Component::SafePointer<MainView> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(midi),[safe,available,outputs,targets](int result){
   if(safe==nullptr)return;
   if(result==10||result==11||result==12||(result>=3000&&result<3000+outputs.size())){
    const auto previous=safe->midiOutputIdentifier;
    safe->midiOutputIdentifier=result==10?juce::String():result==11?juce::String("virtual"):result==12?juce::String("virtual-tracks"):outputs[result-3000].identifier;
    if(safe->midiOutputIdentifier==previous&&(safe->midiOutput!=nullptr||safe->trackMidiOutputs[0]!=nullptr||previous.isEmpty()))return;
    if(safe->refreshMidiOutput()){
     safe->saveMidiSettings();
     safe->status=safe->midiOutputIdentifier.isEmpty()?"MIDI output off":safe->midiOutputIdentifier=="virtual-tracks"?"Sending each track to its own MIDI port":"Sending notes to "+safe->midiOutput->getName();
    }else safe->midiOutputIdentifier=previous;
   }else if(result>=20&&result<=28){
    safe->midiOutputTrack=result==20?-1:result-21;
    safe->engine.midiOutputTrack=safe->trackMidiOutputs[0]!=nullptr?-1:safe->midiOutput!=nullptr?safe->midiOutputTrack:-2;
    safe->saveMidiSettings();safe->status=safe->midiOutputTrack==-1?"Sending all tracks on separate MIDI channels":"Sending track "+juce::String(safe->midiOutputTrack+1);
   }else if(result==1){safe->midiMappings.cancelLearn();safe->status="MIDI Learn cancelled";}
   else if(result==2){safe->refreshMidiInputs();safe->status="MIDI inputs refreshed";}
   else if(result>=100&&result<100+available.size()){
    const auto id=available[result-100].identifier;
    if(safe->enabledMidiDevices.contains(id))safe->enabledMidiDevices.removeString(id);
    else safe->enabledMidiDevices.add(id);
    safe->refreshMidiInputs();safe->saveMidiSettings();
    safe->status=available[result-100].name+(safe->enabledMidiDevices.contains(id)?" enabled":" disabled");
   }else if(result>=1000&&result<1000+static_cast<int>(targets.size())){
    const auto [target,track]=targets[static_cast<size_t>(result-1000)];
    safe->midiMappings.learn(target,track);
    safe->status="Move a MIDI control for "+midiTargetName(target,track);
   }else if(result>=2000){
    safe->midiMappings.remove(static_cast<size_t>(result-2000));safe->saveMidiSettings();safe->status="MIDI mapping removed";
   }
   safe->repaint();
  });
 }
 float mappedValue(MidiMapping::Target target,int track) const {
  if(track<0||track>=activeTrackCount)return 0.0f;
  switch(target){
   case MidiMapping::Target::Level:return engine.level[track].load();
   case MidiMapping::Target::Relationship:return static_cast<float>(relationships[track].getValue()/100.0);
   case MidiMapping::Target::Dynamics:return static_cast<float>(dynamics[track].getValue()/100.0);
   case MidiMapping::Target::Articulation:return static_cast<float>(articulations[track].getValue()/100.0);
   default:return 0.0f;
  }
 }
 void applyMidiAction(const MidiMapping::Action& action){
  const int track=action.track;
  const bool trackTarget=static_cast<int>(action.target)<=static_cast<int>(MidiMapping::Target::Articulation);
  if(trackTarget&&(track<0||track>=activeTrackCount))return;
  switch(action.target){
   case MidiMapping::Target::Level:midiDispatching=true;setManualTrackLevel(track,std::round(action.value*100.0f));midiDispatching=false;break;
   case MidiMapping::Target::Mute:mutes[track].triggerClick();break;
   case MidiMapping::Target::Hold:holds[track].triggerClick();break;
   case MidiMapping::Target::Focus:focuses[track].triggerClick();break;
   case MidiMapping::Target::Relationship:midiDispatching=true;relationships[track].setValue(std::round(action.value*100.0f));midiDispatching=false;break;
   case MidiMapping::Target::Dynamics:midiDispatching=true;dynamics[track].setValue(std::round(action.value*100.0f));midiDispatching=false;break;
   case MidiMapping::Target::Articulation:midiDispatching=true;articulations[track].setValue(std::round(action.value*100.0f));midiDispatching=false;break;
   case MidiMapping::Target::Generate:generateSelectedTrackCandidate();break;
   case MidiMapping::Target::Back:restorePatternStep(-1);break;
   case MidiMapping::Target::Forward:restorePatternStep(1);break;
   case MidiMapping::Target::Keep:saveKeep();break;
   case MidiMapping::Target::Play:play.triggerClick();break;
  }
 }
 void processMidiQueue(){
  std::array<MidiQueuedEvent,128> events{};
  size_t count=0;
  {std::lock_guard<std::mutex> lock(midiQueueMutex);count=midiQueueCount;std::copy_n(midiQueue.begin(),count,events.begin());midiQueueCount=0;}
  for(size_t i=0;i<count;++i){
   const auto& queued=events[i];
   if(queued.inputIndex<0||queued.inputIndex>=static_cast<int>(midiInputs.size()))continue;
   const MidiMapping::Event event{{midiInputs[static_cast<size_t>(queued.inputIndex)]->identifier.toStdString(),queued.channel,queued.note,queued.number},queued.value};
   const auto pending=midiMappings.pendingTarget();
   const auto action=midiMappings.receive(event,[this](MidiMapping::Target target,int track){return mappedValue(target,track);});
   if(pending.has_value()&&!midiMappings.learning()){
    saveMidiSettings();
    status="Mapped "+midiTargetName(pending->first,pending->second)+" on channel "+juce::String(event.address.channel);
   }
   if(action.has_value())applyMidiAction(*action);
  }
 }
 void timerCallback() override {
  if(stepPage.getSelectedId()==1&&selectedTrack>=0&&selectedTrack<activeTrackCount&&engine.playing.load()){
   const int head=engine.lanePlayheads[selectedTrack].load();
   if(head>=0)followedPage=head/Engine::steps;
  }
  if(aiAvailabilityProcess&&!aiAvailabilityProcess->isRunning())finishAppleIdeaAvailability();
  if(aiProcess){
   if(!aiProcess->isRunning())finishAppleIdea();
   else if(juce::Time::getMillisecondCounter()-aiStartedAt>120000){cancelAppleIdea();status="AI idea timed out";repaint();}
  }
  if(!aiAvailabilityProcess&&!aiProcess&&juce::Time::getMillisecondCounter()-aiLastAvailabilityCheck>=30000)checkAppleIdeaAvailability();
  processMidiQueue();
  engine.reclaimSoundBanks();backingAudio.reclaim();
  if(monitorReturn.getToggleState()!=audioReturnEnabled.load()){
   monitorReturn.setToggleState(audioReturnEnabled.load(),juce::dontSendNotification);
   if(!audioReturnEnabled.load())status="Stereo input disconnected; return off";
  }
  returnMeterLevel=std::max(audioReturnPeak.exchange(0.0f),returnMeterLevel*0.82f);
  chooseEffect.setTooltip("Scan and load an AU or VST3 audio effect on Phrase Mix's stereo mix.");
  editEffect.setTooltip("Open the effect's controls.");
  removeEffect.setTooltip("Remove the effect from this Piece.");
  monitorReturn.setTooltip("Monitor external audio input on outputs 1-2 and include it in takes.");
  returnLevel.setTooltip("Level of the stereo return in monitoring and takes.");
  for(int i=0;i<activeTrackCount;++i){
   const bool fading=engine.isTrackFadeActive(i);
   if(fading||fadeWasActive[i]){
    const double current=std::round(engine.level[i].load()*100.0f);
    levels[i].setValue(current,juce::dontSendNotification);
    rowLevels[i].setValue(current,juce::dontSendNotification);
   }
   fadeWasActive[i]=fading;
  }
  startFade.setEnabled(selectedTrack>=0&&engine.playing.load());
  cancelFade.setEnabled(selectedTrack>=0&&engine.isTrackFadeActive(selectedTrack));
  const auto now=juce::Time::getMillisecondCounter();
  for(int i=0;i<Engine::maxTracks;++i)if(pendingSoundChanges[i].active&&static_cast<juce::int32>(now-pendingSoundChanges[i].due)>=0){
   pendingSoundChanges[i].active=false;
   applySoundChoice(i);
   engine.browseGain[i]=1.0f;
  }
  const float peak=captureTap.peak();
  captureMeterLevel=std::max(peak,captureMeterLevel*0.82f);
  useLastSound.setEnabled(lastRecordingFile.existsAsFile());
  discardSound.setEnabled(lastRecordingFile.existsAsFile()&&!recordingSound.load());
	  recordSound.setEnabled(!recordingMix.load());
	  recordMix.setEnabled(!recordingSound.load());
	  playTake.setEnabled(takeList.getSelectedId()>0&&!recordingMix.load());
	  patternBack.setEnabled(selectedTrack>=0&&selectedTrack<activeTrackCount&&patternHistoryIndex[selectedTrack]>0);
	  trackGenerate.setEnabled(selectedTrack>=0&&selectedTrack<activeTrackCount);
	  refreshAppleIdeaControl();
	  patternForward.setEnabled(selectedTrack>=0&&selectedTrack<activeTrackCount&&patternHistoryIndex[selectedTrack]+1<static_cast<int>(patternHistory[selectedTrack].size()));
	  const bool hasSound=selectedTrack>=0&&selectedTrack<activeTrackCount;
	  soundPrevious.setEnabled(hasSound&&sounds[selectedTrack].getNumItems()>1);
	  soundNext.setEnabled(hasSound&&sounds[selectedTrack].getNumItems()>1);
	  soundRandom.setEnabled(hasSound&&sounds[selectedTrack].getNumItems()>1);
	  soundBack.setEnabled(hasSound&&soundHistoryIndex[selectedTrack]>0);
	  soundForward.setEnabled(hasSound&&soundHistoryIndex[selectedTrack]+1<static_cast<int>(soundHistory[selectedTrack].size()));
	  revealTake.setEnabled(takeList.getSelectedId()>0);
	  renameTake.setEnabled(takeList.getSelectedId()>0&&takeName.getText().trim().isNotEmpty());
	  deleteTake.setEnabled(takeList.getSelectedId()>0&&!recordingMix.load());
	  useTakeSound.setEnabled(takeList.getSelectedId()>0&&!recordingMix.load());
   addTakeBacking.setEnabled(takeList.getSelectedId()>0&&!recordingMix.load()&&activeTrackCount<Engine::maxTracks);
	  exportFinal.setEnabled(takeList.getSelectedId()>0);
  if(recordingSound.load()){
   const double seconds=static_cast<double>(captureTap.recordedSamples())/std::max(1.0,captureSampleRate.load());
   status="Recording "+juce::String(seconds,1)+"s";
   if(!captureTap.isActive()){
    recordingSound.store(false);
    recordSound.setButtonText("Capture sound");
    finishSoundRecording();
   }
  }else if(recordingMix.load()){
   const double seconds=static_cast<double>(mixRecordedFrames.load())/std::max(1.0,mixRecordSampleRate);
   status="Recording take "+timeText(seconds);
  }
  if(mixFinishPending&&!mixWriterInCallback.load())finishMixRecording();
  repaint();
 }
 void setLaneSoundWithoutRole(int lane,int kind,int sound){
  pendingSoundChanges[lane].active=false;
  engine.browseGain[lane]=1.0f;
  kinds[lane].setSelectedId(kind,juce::dontSendNotification);
  refreshSoundMenu(lane,false,false);
  sounds[lane].setSelectedId(sound,juce::dontSendNotification);
  if(sounds[lane].getSelectedId()==0)sounds[lane].setSelectedId(1,juce::dontSendNotification);
  applySoundChoice(lane);
 }
 void applyNewTrackDefaults(int lane,bool isNewDiscovery){
  backingFiles[lane]=juce::File();backingClips[lane].reset();backingTimings[lane]=TakeTiming{};backingLoopFlags[lane]=false;backingLoops[lane].setToggleState(false,juce::dontSendNotification);
  holds[lane].setToggleState(false,juce::sendNotification);
  mutes[lane].setToggleState(false,juce::sendNotification);
  solos[lane].setToggleState(false,juce::dontSendNotification);
  connects[lane].setToggleState(false,juce::sendNotification);
  loopLengths[lane].setValue(Engine::steps,juce::dontSendNotification);engine.setLoopSteps(lane,Engine::steps);
  setRhythmSettings(lane,0,5,0);
  juce::Random r;
  static constexpr int presets[][2]={{2,6},{2,3},{2,1},{4,2},{4,1},{1,4},{1,8},{1,3},{1,6}};

  constexpr int presetCount=static_cast<int>(sizeof(presets)/sizeof(presets[0]));
  int presetIndex=r.nextInt(presetCount);
  if(isNewDiscovery){
   const auto lastPresetFile=appDataDirectory().getChildFile("LastDiscoverySound.txt");
   const int previous=lastPresetFile.existsAsFile()?lastPresetFile.loadFileAsString().getIntValue():-1;
   if(previous>=0&&previous<presetCount){
    presetIndex=r.nextInt(presetCount-1);
    if(presetIndex>=previous)++presetIndex;
   }
   if(lastPresetFile.getParentDirectory().createDirectory())lastPresetFile.replaceWithText(juce::String(presetIndex));
  }
  const auto& preset=presets[presetIndex];
	  setLaneSoundWithoutRole(lane,preset[0],preset[1]);
	  materials[lane].setSelectedId(1+r.nextInt(7));
	  playModes[lane].setSelectedId(1,juce::dontSendNotification);engine.samplePlayMode[lane]=0;
	  sampleStarts[lane].setValue(0);sampleSizes[lane].setValue(100);sampleFades[lane].setValue(5);sampleReverses[lane].setToggleState(false,juce::sendNotification);
  lengths[lane].setSelectedId(1+r.nextInt(4));
  roles[lane].setSelectedId(lane==0?1:1+r.nextInt(4));
  relationships[lane].setValue(lane==0?0:20+r.nextInt(61));
  dynamics[lane].setValue(12+r.nextInt(48));
  articulations[lane].setValue(25+r.nextInt(56));
  tails[lane].setValue(0);
  trackSubtitles[lane]=lane==0?"Main phrase":roleSubtitle(roles[lane].getSelectedId()-1);
  levels[lane].setValue(48+r.nextInt(28));
  sends[lane].setValue(18+r.nextInt(48));
  colors[lane].setValue(30+r.nextInt(55));
  moves[lane].setValue(22+r.nextInt(62));
  weathers[lane].setValue(18+r.nextInt(58));
 }
 void addGroupedSamplesToMenu(juce::ComboBox& menu,bool includeAll){
  if(includeAll)menu.addItem("All samples",1);
  static constexpr const char* order[]={"Captured here","Real instruments","Loops","Objects + textures","Harmonics","Older captures","Other sounds"};
  for(auto* group:order){
   bool hasGroup=false;
   for(int i=0;i<sampleGroups.size();++i)if(sampleGroups[i]==group){hasGroup=true;break;}
   if(!hasGroup)continue;
   menu.addSectionHeading(group);
   for(int i=0;i<sampleDisplayNames.size();++i)if(sampleGroups[i]==group)menu.addItem(sampleDisplayNames[i],i+2);
  }
 }
 void refreshSoundMenu(int lane,bool preserve,bool updateEngine=true){
  const int previous=preserve?sounds[lane].getSelectedId():0;
  sounds[lane].clear(juce::dontSendNotification);
  switch(kinds[lane].getSelectedId()){
	   case 2:
	    sounds[lane].addItem("Bass",1);sounds[lane].addItem("Guitar",2);sounds[lane].addItem("Flute",3);sounds[lane].addItem("Oboe",4);sounds[lane].addItem("Bassoon",5);sounds[lane].addItem("Piano",6);sounds[lane].addItem("Nylon guitar",7);
    break;
   case 3:
    addGroupedSamplesToMenu(sounds[lane],true);
    break;
   case 4:
    sounds[lane].addItem("Harmonics",1);sounds[lane].addItem("Reed ghost",2);sounds[lane].addItem("Recorder haze",3);sounds[lane].addItem("Pipe dusk",4);
    break;
   case 6:
    sounds[lane].addItem("Backing audio",1);break;
   case 5:
    sounds[lane].addItem("Instrument plugin",1);
    break;
   default:
    sounds[lane].addItem("Pluck",1);sounds[lane].addItem("Low",2);sounds[lane].addItem("Ring",3);sounds[lane].addItem("Cloud",4);
    sounds[lane].addItem("Glow",5);sounds[lane].addItem("Reed",6);sounds[lane].addItem("Dust",7);sounds[lane].addItem("Bell",8);
    sounds[lane].addItem("Guitar",10);sounds[lane].addItem("Violin",11);sounds[lane].addItem("Bass",12);sounds[lane].addItem("Keys",13);
    break;
  }
  sounds[lane].setSelectedId(previous>0?previous:1,juce::dontSendNotification);
  if(sounds[lane].getSelectedId()==0)sounds[lane].setSelectedId(1,juce::dontSendNotification);
  if(updateEngine)applySoundChoice(lane);
  if(lane==selectedTrack)setSize(getWidth(),contentHeight());
  resized();repaint();
 }
 void applySoundChoice(int lane){
  const int kind=kinds[lane].getSelectedId();
  const int sound=sounds[lane].getSelectedId();
  const bool wasHosted=(engine.hostedInstrumentMask.load()&(1u<<lane))!=0;
  if(wasHosted&&kind!=5)trackInstruments[lane].effect->requestAllNotesOff();
  if(kind==5||kind==6){
   engine.sourceType[lane]=0;
  }else if(kind==2){
   engine.sourceType[lane]=13;
	   static constexpr int realInstrumentIds[]={0,1,4,5,6,9,10};
	   engine.instrumentChoice[lane]=realInstrumentIds[static_cast<size_t>(std::clamp(sound-1,0,6))];
   sources[lane].setSelectedId(14,juce::dontSendNotification);
   instruments[lane].setSelectedId(engine.instrumentChoice[lane].load()+1,juce::dontSendNotification);
  }else if(kind==3){
   engine.sourceType[lane]=8;
   engine.sampleChoice[lane]=sound-2;
   sources[lane].setSelectedId(9,juce::dontSendNotification);
   librarySounds[lane].setSelectedId(sound,juce::dontSendNotification);
  }else if(kind==4){
   engine.sourceType[lane]=13;
   static constexpr int ghostInstrumentIds[]={2,3,7,8};
   engine.instrumentChoice[lane]=ghostInstrumentIds[static_cast<size_t>(std::clamp(sound-1,0,3))];
   sources[lane].setSelectedId(14,juce::dontSendNotification);
   instruments[lane].setSelectedId(engine.instrumentChoice[lane].load()+1,juce::dontSendNotification);
  }else{
   engine.sourceType[lane]=std::clamp(sound-1,0,12);
   sources[lane].setSelectedId(sound,juce::dontSendNotification);
  }
  engine.soundRevision[lane].fetch_add(1);
  updateHostedInstrumentMask();
  updateTrackSelectLabel(lane);
 }
 void updateHostedInstrumentMask(){
  if(reorderingTracks)return;
  uint32_t mask=0;
  for(int i=0;i<activeTrackCount;++i)if(kinds[i].getSelectedId()==5)mask|=1u<<i;
  engine.hostedInstrumentMask.store(mask);
  uint32_t backingMask=0;BackingTracks::Clips clips{};
  for(int i=0;i<activeTrackCount;++i)if(kinds[i].getSelectedId()==6){backingMask|=1u<<i;clips[i]=backingClips[i];}
  backingAudio.publish(clips);engine.backingTrackMask.store(backingMask);
 }
 bool validSoundChoice(SoundChoice choice) const {
  if(choice.kind==1)return (choice.sound>=1&&choice.sound<=8)||(choice.sound>=10&&choice.sound<=13);
  if(choice.kind==2)return choice.sound>=1&&choice.sound<=7;
  if(choice.kind==3)return choice.sound>=1&&choice.sound<=sampleDisplayNames.size()+1;
  if(choice.kind==4)return choice.sound>=1&&choice.sound<=4;
  if(choice.kind==5||choice.kind==6)return choice.sound==1;
  return false;
 }
 void rememberSoundChoice(int lane){
  const SoundChoice choice{kinds[lane].getSelectedId(),sounds[lane].getSelectedId()};
  if(!validSoundChoice(choice))return;
  auto& history=soundHistory[lane];
  int& index=soundHistoryIndex[lane];
  if(!history.empty()&&history[static_cast<size_t>(std::clamp(index,0,static_cast<int>(history.size())-1))]==choice)return;
  if(index+1<static_cast<int>(history.size()))history.erase(history.begin()+index+1,history.end());
  history.push_back(choice);
  if(history.size()>24)history.erase(history.begin());
  index=static_cast<int>(history.size())-1;
 }
 void queueSoundChoice(int lane,bool remember){
  if(remember)rememberSoundChoice(lane);
  updateTrackSelectLabel(lane);
  if(engine.playing.load()&&lane<activeTrackCount){
   engine.browseGain[lane]=0.0f;
   pendingSoundChanges[lane]={true,juce::Time::getMillisecondCounter()+65};
  }else{
   pendingSoundChanges[lane].active=false;
   applySoundChoice(lane);
   engine.browseGain[lane]=1.0f;
  }
  repaint();
 }
 bool selectSoundChoice(int lane,SoundChoice choice,bool remember){
  if(!validSoundChoice(choice))return false;
  if(kinds[lane].getSelectedId()!=choice.kind){
   kinds[lane].setSelectedId(choice.kind,juce::dontSendNotification);
   refreshSoundMenu(lane,false,false);
  }
  sounds[lane].setSelectedId(choice.sound,juce::dontSendNotification);
  if(sounds[lane].getSelectedId()!=choice.sound)return false;
  queueSoundChoice(lane,remember);
  return true;
 }
 std::vector<int> availableSoundIds(int lane) const {
  std::vector<int> ids;
  for(int i=0;i<sounds[lane].getNumItems();++i){const int id=sounds[lane].getItemId(i);if(id>0)ids.push_back(id);}
  return ids;
 }
 void browseSound(int direction){
  const int lane=selectedTrack;
  if(lane<0||lane>=activeTrackCount)return;
  const auto ids=availableSoundIds(lane);
  if(ids.size()<2)return;
  const auto it=std::find(ids.begin(),ids.end(),sounds[lane].getSelectedId());
  const int index=it==ids.end()?0:static_cast<int>(it-ids.begin());
  const int next=(index+direction+static_cast<int>(ids.size()))%static_cast<int>(ids.size());
  selectSoundChoice(lane,{kinds[lane].getSelectedId(),ids[static_cast<size_t>(next)]},true);
 }
 void randomSound(){
  const int lane=selectedTrack;
  if(lane<0||lane>=activeTrackCount)return;
  const auto ids=availableSoundIds(lane);
  if(ids.size()<2)return;
  const int current=sounds[lane].getSelectedId();
  juce::Random random;
  const int start=random.nextInt(static_cast<int>(ids.size()));
  for(size_t offset=0;offset<ids.size();++offset){
   const int id=ids[(static_cast<size_t>(start)+offset)%ids.size()];
   if(id!=current){selectSoundChoice(lane,{kinds[lane].getSelectedId(),id},true);return;}
  }
 }
 void stepSoundHistory(int direction){
  const int lane=selectedTrack;
  if(lane<0||lane>=activeTrackCount)return;
  auto& history=soundHistory[lane];
  for(int index=soundHistoryIndex[lane]+direction;index>=0&&index<static_cast<int>(history.size());index+=direction){
   if(selectSoundChoice(lane,history[static_cast<size_t>(index)],false)){
    soundHistoryIndex[lane]=index;
    return;
   }
  }
 }
 void setKindAndSound(int lane,int kind,int sound){
  setLaneSoundWithoutRole(lane,kind,sound);
 }
 void migrateOldSourceToKindSound(int lane,int source,int instrument,int librarySound){
  if(source==9)setKindAndSound(lane,3,librarySound>0?librarySound:1);
  else if(source==14){
   if(instrument==3)setKindAndSound(lane,4,2);
   else if(instrument==2)setKindAndSound(lane,4,1);
   else setKindAndSound(lane,2,std::clamp(instrument,0,1)+1);
  }else setKindAndSound(lane,1,std::clamp(source,1,13));
 }
 void randomizeAll(){
  juce::Random r;
  tempo.setValue(72+r.nextInt(68));
  density.setValue(35+r.nextInt(51));
  key.setSelectedId(1+r.nextInt(12));
  scale.setSelectedId(1+r.nextInt(3));
  shape.setSelectedId(1+r.nextInt(6));
  mode.setSelectedId(1+r.nextInt(3));
  changeEvery.setValue(1+r.nextInt(8));
  for(int i=0;i<activeTrackCount;++i){
   if(kinds[i].getSelectedId()==6)continue;
   kinds[i].setSelectedId(1+r.nextInt(4),juce::dontSendNotification);
   refreshSoundMenu(i,false,false);
   const auto ids=availableSoundIds(i);
   if(!ids.empty())sounds[i].setSelectedId(ids[static_cast<size_t>(r.nextInt(static_cast<int>(ids.size())))],juce::dontSendNotification);
   pendingSoundChanges[i].active=false;
   engine.browseGain[i]=1.0f;
   applySoundChoice(i);
   rememberSoundChoice(i);
   materials[i].setSelectedId(1+r.nextInt(7));
   lengths[i].setSelectedId(1+r.nextInt(4));
   roles[i].setSelectedId(i==0?1:1+r.nextInt(4));
   relationships[i].setValue(i==0?0:15+r.nextInt(71));
   dynamics[i].setValue(8+r.nextInt(63));
   articulations[i].setValue(18+r.nextInt(73));
   tails[i].setValue(r.nextInt(61));
   trackSubtitles[i]=i==0?"Main phrase":roleSubtitle(roles[i].getSelectedId()-1);
   levels[i].setValue(45+r.nextInt(41));
   sends[i].setValue(10+r.nextInt(66));
   colors[i].setValue(20+r.nextInt(71));
   moves[i].setValue(15+r.nextInt(81));
   weathers[i].setValue(15+r.nextInt(76));
   holds[i].setToggleState(false,juce::sendNotification);
   mutes[i].setToggleState(false,juce::sendNotification);
  }
  room.setValue(35+r.nextInt(51));
  wet.setValue(12+r.nextInt(49));
  drive.setValue(r.nextInt(46));
  filter.setValue(35+r.nextInt(61));
  generateAllTrackCandidates();
 }
 juce::File sceneDirectory() const {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Phrase Mix").getChildFile("Scenes");
 }
 juce::File appDataDirectory() const {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Phrase Mix");
 }
 juce::String currentPieceName() const {
  const auto name=sceneName.getText().trim();
  return name.isEmpty()?"Untitled Piece":name;
 }
 static juce::String legacyPieceId(const juce::String& name){
  return "legacy:"+juce::File::createLegalFileName(name.trim()).toLowerCase();
 }
 juce::File currentPieceDirectory() const {
  return appDataDirectory().getChildFile("Pieces").getChildFile(juce::File::createLegalFileName(currentPieceName()));
 }
 juce::File legacySampleDirectory() const {
  return appDataDirectory().getChildFile("Samples");
 }
 juce::File legacyTakeDirectory() const {
  return appDataDirectory().getChildFile("Recordings");
 }
 juce::String makeSceneName() const {
  static constexpr const char* a[]={"Amber","Hidden","Slow","Silver","Moss","Glass","Soft","Bright","Deep","Quiet","Long","Blue"};
  static constexpr const char* b[]={"Weather","Rings","Cloud","Wire","Garden","Drift","Memory","Field","Glow","Dust","Water","Organ"};
  juce::Random r;
  return juce::String(a[r.nextInt(12)])+" "+b[r.nextInt(12)]+" "+juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M");
 }
 juce::String makeTrackName() const {
  static constexpr const char* a[]={"Glass","Moss","Hidden","Soft","Bright","Long","Blue","Silver","Warm","Low","Quiet","Wild","Dust","Amber","Slow","Green"};
  static constexpr const char* b[]={"Wire","Field","Cloud","Ring","Organ","Tide","Glow","Thread","Bell","Garden","Drift","Key","Reed","Stone","Weather","Spark"};
  juce::Random r;
  return juce::String(a[r.nextInt(16)])+" "+b[r.nextInt(16)];
 }
 static juce::String roleSubtitle(int role){
  switch(std::clamp(role,0,3)){
   case 0:return "Shadow follower";
   case 1:return "Contrast follower";
   case 2:return "Sparse follower";
   default:return "Free follower";
  }
 }
 juce::String makeSoundName() const {
  static constexpr const char* a[]={"Room","Glass","Pocket","Table","Soft","Hidden","Close","Warm","Silver","Quiet","Door","Hand","Low","Bright","Dust","Night"};
  static constexpr const char* b[]={"Tap","Breath","Tone","Grain","Knock","Thread","Field","Cloud","Ring","Wood","Air","Pulse","Flicker","Step","Hum","Spark"};
  juce::Random r;
  return juce::String(a[r.nextInt(16)])+" "+b[r.nextInt(16)]+" "+juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M.%S");
 }
 juce::String makeMixName() const {
  static constexpr const char* a[]={"Long","Hidden","Glass","Silver","Bright","Soft","Deep","Quiet","Amber","Blue","Slow","Warm"};
  static constexpr const char* b[]={"Take","Weather","Field","Drift","Rings","Cloud","Garden","Memory","Glow","Organ","Water","Wire"};
  juce::Random r;
  return juce::String(a[r.nextInt(12)])+" "+b[r.nextInt(12)]+" "+juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M.%S");
 }
 juce::String timeText(double seconds) const {
  const int total=static_cast<int>(std::max(0.0,seconds));
  return juce::String(total/60)+":"+juce::String(total%60).paddedLeft('0',2);
 }
 juce::File userSampleDirectory() const {
  return currentPieceDirectory().getChildFile("Captured Sounds");
 }
 juce::File mixRecordingDirectory() const {
  return currentPieceDirectory().getChildFile("Takes");
 }
 juce::File finalDirectory() const {
  return currentPieceDirectory().getChildFile("Finals");
 }
 juce::File backingDirectory() const {return currentPieceDirectory().getChildFile("Backing Tracks");}
 static juce::File timingFile(const juce::File& file){return file.getSiblingFile(file.getFileName()+".timing.json");}
 static juce::var timingObject(const TakeTiming& timing){
  juce::DynamicObject::Ptr object=new juce::DynamicObject();
  object->setProperty("version",1);object->setProperty("bpm",timing.bpm);object->setProperty("startStep",timing.startStep);
  object->setProperty("steadyTempo",timing.steady);object->setProperty("startedPlaying",timing.startedPlaying);
  return juce::var(object.get());
 }
 static TakeTiming parseTiming(const juce::var& value){
  TakeTiming timing;
  if(auto* object=value.getDynamicObject()){
   timing.bpm=static_cast<double>(object->getProperty("bpm"));timing.startStep=static_cast<double>(object->getProperty("startStep"));
   timing.steady=static_cast<bool>(object->getProperty("steadyTempo"));timing.startedPlaying=static_cast<bool>(object->getProperty("startedPlaying"));
  }
  if(!std::isfinite(timing.bpm)||!std::isfinite(timing.startStep)){timing=TakeTiming{};}
  return timing;
 }
 static bool writeTakeTiming(const juce::File& file,const TakeTiming& timing){return timingFile(file).replaceWithText(juce::JSON::toString(timingObject(timing),true));}
 std::shared_ptr<const BackingTracks::Clip> readBackingClip(const juce::File& file){
  std::unique_ptr<juce::AudioFormatReader> reader(file.existsAsFile()?audioFormats.createReaderFor(file):nullptr);
  if(reader==nullptr||reader->lengthInSamples<1||reader->sampleRate<=0||reader->numChannels<1){status="Could not read backing audio: "+file.getFileName();return {};}
  if(reader->lengthInSamples>2147483647){status="This backing recording is too large to load into memory";return {};}
  try{
   const int frames=static_cast<int>(reader->lengthInSamples);
   juce::AudioBuffer<float> decoded(2,frames);
   if(!reader->read(&decoded,0,frames,0,true,true)){status="Could not read the complete backing recording";return {};}
   auto clip=std::make_shared<BackingTracks::Clip>();clip->rate=reader->sampleRate;
   clip->left.assign(decoded.getReadPointer(0),decoded.getReadPointer(0)+frames);
   const int right=reader->numChannels>1?1:0;
   clip->right.assign(decoded.getReadPointer(right),decoded.getReadPointer(right)+frames);
   return clip;
  }catch(const std::bad_alloc&){status="Not enough memory to load this complete backing recording";return {};}
 }
 void chooseBackingAudio(){
  const int lane=selectedTrack;
  if(lane<0||lane>=activeTrackCount||recordingMix.load())return;
  const auto name=trackNames[lane];
  backingFileChooser=std::make_unique<juce::FileChooser>("Choose backing audio",mixRecordingDirectory(),"*.wav;*.aif;*.aiff;*.flac;*.mp3");
  const juce::Component::SafePointer<MainView> safe(this);
  backingFileChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,lane,name](const juce::FileChooser& chooser){
   if(safe==nullptr||lane>=safe->activeTrackCount||safe->trackNames[lane]!=name)return;
   const auto file=chooser.getResult();if(file.existsAsFile())safe->showAddBackingDialog(file,lane);
  });
 }
 void showAddBackingDialog(const juce::File& file,int replace=-1){
  if(recordingMix.load()||mixFinishPending){status="Finish recording the take first";repaint();return;}
  if(replace>=activeTrackCount)return;
  if(!file.existsAsFile()){status="Select a recorded take, or choose a backing audio file";repaint();return;}
  if(replace<0&&activeTrackCount>=Engine::maxTracks){status="Remove a track to make room for backing audio";repaint();return;}
  const TakeTiming timing=replace>=0&&backingFiles[replace]==file?backingTimings[replace]:parseTiming(juce::JSON::parse(timingFile(file)));
  auto* dialog=new juce::AlertWindow(replace<0?"Add backing track":"Backing timing",
   (timing.canAlign()?"Recorded timing is available. Restore it to align new patterns with the recording.":"This recording has no reliable steady-tempo timing. Enter its original BPM if known; start step 0 assumes it began on the first beat.")
   +juce::String(replace<0?"\nAdds the complete recording as a new track and mutes existing tracks. You can unmute them individually.":"\nTiming applies to the whole Piece. The audio always plays at its original speed."),juce::MessageBoxIconType::NoIcon);
  dialog->addTextEditor("bpm",timing.canAlign()?juce::String(timing.bpm,3):juce::String(),"Original BPM (optional)");
  dialog->addTextEditor("start",timing.canAlign()?juce::String(timing.startStep,6):juce::String("0"),"Starting step (optional; 0 = beginning)");
  dialog->addButton(replace<0?"Add + use timing":"Use timing",1,timing.canAlign()?juce::KeyPress(juce::KeyPress::returnKey):juce::KeyPress());
  dialog->addButton(replace<0?"Add only":"Keep current timing",2,!timing.canAlign()?juce::KeyPress(juce::KeyPress::returnKey):juce::KeyPress());
  dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
  const juce::Component::SafePointer<MainView> safe(this);
  const auto piece=pieceId;const auto replacedName=replace>=0?trackNames[replace]:juce::String();
  dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,file,replace,piece,replacedName,timing](int result){
   if(safe==nullptr||result==0)return;
   if(safe->pieceId!=piece||(replace>=0&&(replace>=safe->activeTrackCount||safe->trackNames[replace]!=replacedName))){safe->status="The Piece changed; choose the backing track again";safe->repaint();return;}
   TakeTiming chosen=timing;
   if(result==1){
    chosen.bpm=dialog->getTextEditorContents("bpm").getDoubleValue();chosen.startStep=dialog->getTextEditorContents("start").getDoubleValue();chosen.steady=true;chosen.startedPlaying=true;
    if(!chosen.canAlign()){safe->status="Enter an original BPM from 10 to 240, or choose Add only";safe->repaint();return;}
   }
   safe->addBackingFile(file,chosen,result==1,replace);
  }),true);
 }
 bool addBackingFile(const juce::File& file,const TakeTiming& timing,bool useTiming,int replace=-1){
  if(recordingMix.load()||mixFinishPending){status="Finish take recording before changing backing audio";repaint();return false;}
  if(replace<0&&activeTrackCount>=Engine::maxTracks){status="All eight tracks are in use";repaint();return false;}
  if(replace<-1||replace>=activeTrackCount)return false;
  auto clip=readBackingClip(file);if(!clip){repaint();return false;}
  const auto dir=backingDirectory();if(!dir.createDirectory().wasOk()){status="Could not create the backing audio folder";repaint();return false;}
  auto target=file;
  if(file.getParentDirectory()!=dir){target=uniqueSiblingFile(dir.getChildFile(file.getFileName()));if(!file.copyFileTo(target)){status="Could not copy the complete backing audio";repaint();return false;}}
  const bool timingSaved=writeTakeTiming(target,timing);
  const int previousCount=activeTrackCount;
  const int lane=replace>=0?replace:previousCount;
  engine.playing=false;stopTakePlayback();
  if(replace<0)setTrackCount(previousCount+1,false,true);
  backingFiles[lane]=target;backingClips[lane]=std::move(clip);backingTimings[lane]=timing;
  backingLoopFlags[lane]=false;backingLoops[lane].setToggleState(false,juce::dontSendNotification);
  setLaneSoundWithoutRole(lane,6,1);trackNames[lane]=file.getFileNameWithoutExtension();trackSubtitles[lane]="Backing recording";
  setManualTrackLevel(lane,100);holds[lane].setToggleState(true,juce::sendNotification);mutes[lane].setToggleState(false,juce::sendNotification);
  if(replace<0){setSoloTrack(-1);setFocusTrack(-1);for(int i=0;i<previousCount;++i)mutes[i].setToggleState(true,juce::sendNotification);}
  if(useTiming&&timing.canAlign()){tempo.setValue(timing.bpm,juce::sendNotificationSync);engine.transportStartStep=timing.startStep;}
  engine.activateTracks(activeTrackCount);engine.restartFromBeginning();updateHostedInstrumentMask();selectTrack(lane);
  play.setButtonText("Play");status="Backing ready: "+file.getFileName()+" · "+timeText(backingClips[lane]->duration());
  if(!timingSaved)status+=" (could not save audio timing sidecar)";
  resized();repaint();return true;
 }

 void renderTakePlayback(const juce::AudioSourceChannelInfo& info){
  if(!takePlaying.load())return;
  const juce::ScopedTryLock lock(takePlaybackLock);
  if(!lock.isLocked())return;
  if(takePlaybackBuffer.empty()||takePlaybackPosition>=takePlaybackFrames){takePlaying.store(false);return;}
  float* left=info.buffer->getWritePointer(0,info.startSample);
  float* right=info.buffer->getNumChannels()>1?info.buffer->getWritePointer(1,info.startSample):left;
  for(int i=0;i<info.numSamples;++i){
   if(takePlaybackPosition>=takePlaybackFrames){takePlaying.store(false);break;}
   const size_t index=static_cast<size_t>(takePlaybackPosition*2);
   const float l=takePlaybackBuffer[index];
   const float r=takePlaybackBuffer[index+1];
   left[i]=std::tanh(left[i]+l);
   if(right)right[i]=std::tanh(right[i]+r);
   ++takePlaybackPosition;
  }
 }
 void refreshTakeList(bool keepSelection){
  const auto selected=keepSelection?takeList.getText():juce::String();
  takeFiles.clear();
  takeList.clear(juce::dontSendNotification);
  juce::Array<juce::File> files;
 const auto dir=mixRecordingDirectory();
  if(dir.isDirectory())dir.findChildFiles(files,juce::File::findFiles,false,"*.wav;*.aif;*.aiff");
  const auto legacyDir=legacyTakeDirectory();
  if(legacyDir.isDirectory()&&legacyDir!=dir)legacyDir.findChildFiles(files,juce::File::findFiles,false,"*.wav;*.aif;*.aiff");
  files.sort();
  for(int i=files.size();--i>=0;)takeFiles.add(files.getReference(i));
  int id=1;
  for(auto& file:takeFiles)takeList.addItem(file.getFileNameWithoutExtension(),id++);
	  if(selected.isNotEmpty())takeList.setText(selected,juce::dontSendNotification);
	  if(takeList.getSelectedId()==0&&takeFiles.size()>0)takeList.setSelectedId(1,juce::dontSendNotification);
	  updateSelectedTakeDetails(false);
	  const auto file=selectedTakeFile();
	  takeName.setText(file.existsAsFile()?file.getFileNameWithoutExtension():juce::String(),juce::dontSendNotification);
	 }
	 juce::File selectedTakeFile() const {
	  const int index=takeList.getSelectedId()-1;
	  return index>=0&&index<takeFiles.size()?takeFiles.getReference(index):juce::File();
	 }
	 void selectTakeNamed(const juce::String& name){
	  for(int i=0;i<takeFiles.size();++i)if(takeFiles.getReference(i).getFileNameWithoutExtension()==name){
	   takeList.setSelectedId(i+1,juce::dontSendNotification);
	   updateSelectedTakeDetails(false);
	   takeName.setText(name,juce::dontSendNotification);
	   return;
	  }
	 }
	 juce::File uniqueSiblingFile(const juce::File& desired) const {
	  if(!desired.exists())return desired;
	  const auto dir=desired.getParentDirectory();
	  const auto base=desired.getFileNameWithoutExtension();
	  const auto ext=desired.getFileExtension();
	  for(int i=2;i<999;++i){
	   const auto candidate=dir.getChildFile(base+" "+juce::String(i)+ext);
	   if(!candidate.exists())return candidate;
	  }
	  return dir.getNonexistentChildFile(base,ext);
	 }
	 void selectTakeFromList(){
	  stopTakePlayback();
	  updateSelectedTakeDetails(true);
	  const auto file=selectedTakeFile();
	  takeName.setText(file.existsAsFile()?file.getFileNameWithoutExtension():juce::String(),juce::dontSendNotification);
	  if(file.existsAsFile())status="Take: "+file.getFileNameWithoutExtension()+" · "+timeText(selectedTakeDuration);
	  repaint();
	 }
 void updateSelectedTakeDetails(bool keepStart){
  const auto file=selectedTakeFile();
  selectedTakeDuration=1.0;
  selectedTakeRate=48000.0;
  if(file.existsAsFile()){
   std::unique_ptr<juce::AudioFormatReader> reader(audioFormats.createReaderFor(file));
   if(reader!=nullptr&&reader->lengthInSamples>1){
    selectedTakeRate=reader->sampleRate;
    selectedTakeDuration=static_cast<double>(reader->lengthInSamples)/std::max(1.0,reader->sampleRate);
   }
  }
  const double previousStart=keepStart?finalStart.getValue():0.0;
  finalStart.setRange(0.0,std::max(0.1,selectedTakeDuration-0.1),0.1);
  finalEnd.setRange(0.1,selectedTakeDuration,0.1);
  finalStart.setValue(std::min(previousStart,std::max(0.0,selectedTakeDuration-0.1)),juce::dontSendNotification);
  finalEnd.setValue(selectedTakeDuration,juce::dontSendNotification);
 }
 void clampFinalTrim(bool startChanged){
  const double minLength=0.1;
  if(startChanged&&finalStart.getValue()>finalEnd.getValue()-minLength)finalStart.setValue(std::max(0.0,finalEnd.getValue()-minLength),juce::dontSendNotification);
  if(!startChanged&&finalEnd.getValue()<finalStart.getValue()+minLength)finalEnd.setValue(std::min(selectedTakeDuration,finalStart.getValue()+minLength),juce::dontSendNotification);
 }
 void showRecordingsFolder(){
 const auto dir=mixRecordingDirectory();
  dir.createDirectory();
  dir.revealToUser();
  status="Showing takes folder";
  repaint();
 }
	 void revealSelectedTake(){
	  const auto file=selectedTakeFile();
	  if(!file.existsAsFile()){status="No take selected";repaint();return;}
	  file.revealToUser();
	  status="Showing: "+file.getFileNameWithoutExtension();
	  repaint();
	 }
	 void renameSelectedTake(){
	  const auto file=selectedTakeFile();
	  if(!file.existsAsFile()){status="No take selected";repaint();return;}
	  const auto safe=juce::File::createLegalFileName(takeName.getText().trim());
	  if(safe.isEmpty()){status="Name the take first";repaint();return;}
	  auto target=file.getParentDirectory().getChildFile(safe).withFileExtension(file.getFileExtension());
	  if(target==file){status="Take already has that name";repaint();return;}
	  target=uniqueSiblingFile(target);
	  stopTakePlayback();
	  const bool ok=file.moveFileTo(target);
   const bool timingOk=!ok||!timingFile(file).existsAsFile()||timingFile(file).moveFileTo(timingFile(target));
	  refreshTakeList(false);
	  selectTakeNamed(target.getFileNameWithoutExtension());
	  status=ok?"Renamed take: "+target.getFileNameWithoutExtension():"Could not rename take";
   if(ok&&!timingOk)status+=" (timing file could not be renamed)";
	  repaint();
	 }
	 void confirmDeleteSelectedTake(){
	  const auto file=selectedTakeFile();
	  if(!file.existsAsFile())return;
	  juce::Component::SafePointer<MainView> safe(this);
	  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
	   .withTitle("Delete take?").withMessage("Delete "+file.getFileNameWithoutExtension()+"? This removes its audio file.")
	   .withButton("Cancel").withButton("Delete take").withAssociatedComponent(this);
	  juce::NativeMessageBox::showAsync(options,[safe,file](int result){if(result==1&&safe!=nullptr&&safe->selectedTakeFile()==file)safe->deleteSelectedTake();});
	 }
	 void deleteSelectedTake(){
	  const auto file=selectedTakeFile();
	  if(!file.existsAsFile()){status="No take selected";repaint();return;}
	  const auto name=file.getFileNameWithoutExtension();
	  stopTakePlayback();
	  const bool ok=file.deleteFile();
   if(ok)timingFile(file).deleteFile();
	  refreshTakeList(false);
	  takeName.setText(selectedTakeFile().existsAsFile()?selectedTakeFile().getFileNameWithoutExtension():juce::String(),juce::dontSendNotification);
	  status=ok?"Deleted take: "+name:"Could not delete take";
	  repaint();
	 }
	 void useSelectedTakeAsSound(){
	  const auto file=selectedTakeFile();
	  if(!file.existsAsFile()){status="No take selected";repaint();return;}
	  std::unique_ptr<juce::AudioFormatReader> reader(audioFormats.createReaderFor(file));
	  if(reader==nullptr||reader->lengthInSamples<=1){status="Could not read take";repaint();return;}
	  const int frames=static_cast<int>(std::min<juce::int64>(reader->lengthInSamples,static_cast<juce::int64>(reader->sampleRate*60.0)));
	  juce::AudioBuffer<float> source(static_cast<int>(std::max<unsigned int>(2,reader->numChannels)),frames);
	  reader->read(&source,0,frames,0,true,true);
	  juce::AudioBuffer<float> mono(1,frames);
	  float midPeak=0.0f,leftPeak=0.0f,rightPeak=0.0f;
	  for(int i=0;i<frames;++i){
	   const float left=source.getSample(0,i);
	   const float right=source.getSample(source.getNumChannels()>1?1:0,i);
	   const float mid=(left+right)*0.5f;
	   mono.setSample(0,i,mid);
	   midPeak=std::max(midPeak,std::abs(mid));
	   leftPeak=std::max(leftPeak,std::abs(left));
	   rightPeak=std::max(rightPeak,std::abs(right));
	  }
	  if(midPeak<std::max(leftPeak,rightPeak)*0.35f){
	   const int channel=rightPeak>leftPeak?1:0;
	   for(int i=0;i<frames;++i)mono.setSample(0,i,source.getSample(std::min(channel,source.getNumChannels()-1),i));
	   midPeak=std::max(leftPeak,rightPeak);
	  }
	  if(midPeak<0.00001f){status="Take is silent";repaint();return;}
	  const float gain=std::min(18.0f,0.9f/midPeak);
	  for(int i=0;i<frames;++i)mono.setSample(0,i,std::tanh(mono.getSample(0,i)*gain*1.05f)*0.92f);
	  auto target=userSampleDirectory().getChildFile(juce::File::createLegalFileName(file.getFileNameWithoutExtension()+" Source")).withFileExtension(".wav");
	  target.getParentDirectory().createDirectory();
	  target=uniqueSiblingFile(target);
	  juce::WavAudioFormat format;
	  std::unique_ptr<juce::FileOutputStream> stream(target.createOutputStream());
	  if(stream==nullptr||!stream->openedOk()){status="Could not save sound";repaint();return;}
	  std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),reader->sampleRate,1,24,{},0));
	  if(writer==nullptr){status="Could not write sound";repaint();return;}
	  stream.release();
	  if(!writer->writeFromAudioSampleBuffer(mono,0,frames)){status="Could not write sound";repaint();return;}
	  lastRecordingFile=target;
	  loadSampleLibrary();
	  lastRecordingSoundId=soundIdForFile(target);
	  if(activeTrackCount==0)addNewTrack();
	  if(selectedTrack>=0&&selectedTrack<activeTrackCount&&lastRecordingSoundId>=2){
	   setLaneSoundWithoutRole(selectedTrack,3,lastRecordingSoundId);
	   rememberSoundChoice(selectedTrack);
	   playModes[selectedTrack].setSelectedId(3,juce::dontSendNotification);
	   engine.samplePlayMode[selectedTrack]=2;
	   sampleStarts[selectedTrack].setValue(0);
	   sampleSizes[selectedTrack].setValue(100);
	   sampleFades[selectedTrack].setValue(5);
	   sampleReverses[selectedTrack].setToggleState(false,juce::sendNotification);
	   lengths[selectedTrack].setSelectedId(4);
	   levels[selectedTrack].setValue(std::max(78.0,levels[selectedTrack].getValue()));
	   trackSubtitles[selectedTrack]="Turned from take";
	  }
	  status="Turned into sound: "+target.getFileNameWithoutExtension();
	  repaint();
	 }
 void stopTakePlayback(){
  takePlaying.store(false);
  playTake.setButtonText("Play take");
 }
 void toggleTakePlayback(){
  if(takePlaying.load()){stopTakePlayback();status="Stopped take";repaint();return;}
  const auto file=selectedTakeFile();
  if(!file.existsAsFile()){refreshTakeList(false);status="No take selected";repaint();return;}
  std::unique_ptr<juce::AudioFormatReader> reader(audioFormats.createReaderFor(file));
  if(reader==nullptr||reader->lengthInSamples<=1){status="Could not read take";repaint();return;}
  const int frames=static_cast<int>(std::min<juce::int64>(reader->lengthInSamples,static_cast<juce::int64>(reader->sampleRate*60.0*20.0)));
  juce::AudioBuffer<float> buffer(static_cast<int>(std::max<unsigned int>(2,reader->numChannels)),frames);
  reader->read(&buffer,0,frames,0,true,true);
  {
   const juce::ScopedLock lock(takePlaybackLock);
   takePlaybackBuffer.assign(static_cast<size_t>(frames*2),0.0f);
   for(int i=0;i<frames;++i){
    takePlaybackBuffer[static_cast<size_t>(i*2)]=buffer.getSample(0,i)*0.95f;
    takePlaybackBuffer[static_cast<size_t>(i*2+1)]=buffer.getSample(buffer.getNumChannels()>1?1:0,i)*0.95f;
   }
   takePlaybackFrames=frames;
   takePlaybackPosition=0;
   takePlaying.store(true);
  }
  playTake.setButtonText("Stop take");
  status="Playing: "+file.getFileNameWithoutExtension();
  repaint();
 }
 void exportFinalTake(){
  const auto file=selectedTakeFile();
  if(!file.existsAsFile()){status="No take selected";repaint();return;}
  std::unique_ptr<juce::AudioFormatReader> reader(audioFormats.createReaderFor(file));
  if(reader==nullptr||reader->lengthInSamples<=1){status="Could not read take";repaint();return;}
  const double rate=reader->sampleRate;
  const int startFrame=std::clamp(static_cast<int>(finalStart.getValue()*rate),0,static_cast<int>(reader->lengthInSamples-1));
  const int endFrame=std::clamp(static_cast<int>(finalEnd.getValue()*rate),startFrame+1,static_cast<int>(reader->lengthInSamples));
  const int frames=endFrame-startFrame;
  if(frames<static_cast<int>(rate*0.1)){status="Final is too short";repaint();return;}
  juce::AudioBuffer<float> buffer(2,frames);
  reader->read(&buffer,0,frames,startFrame,true,true);
  if(reader->numChannels<2)buffer.copyFrom(1,0,buffer,0,0,frames);
  const int fade=std::min(frames/2,static_cast<int>(rate*0.01));
  float peak=0.0f;
  for(int i=0;i<frames;++i){
   const float fadeIn=fade>0&&i<fade?static_cast<float>(i)/static_cast<float>(fade):1.0f;
   const float fadeOut=fade>0&&i>=frames-fade?static_cast<float>(frames-i-1)/static_cast<float>(fade):1.0f;
   const float fadeGain=std::clamp(std::min(fadeIn,fadeOut),0.0f,1.0f);
   for(int ch=0;ch<2;++ch){
    const float value=buffer.getSample(ch,i)*fadeGain;
    buffer.setSample(ch,i,value);
    peak=std::max(peak,std::abs(value));
   }
  }
  if(peak<0.00001f){status="Final is silent";repaint();return;}
  const float gain=std::min(12.0f,0.89f/peak);
  const float limiterScale=1.0f/std::tanh(1.15f);
  for(int i=0;i<frames;++i)for(int ch=0;ch<2;++ch){
   const float value=buffer.getSample(ch,i)*gain;
   buffer.setSample(ch,i,std::tanh(value*1.15f)*limiterScale*0.94f);
  }
  const auto base=file.getFileNameWithoutExtension()+" Final "+juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M.%S");
  const auto out=uniqueSiblingFile(finalDirectory().getChildFile(juce::File::createLegalFileName(base)).withFileExtension(".wav"));
  out.getParentDirectory().createDirectory();
  juce::WavAudioFormat format;
  std::unique_ptr<juce::FileOutputStream> stream(out.createOutputStream());
  if(stream==nullptr||!stream->openedOk()){status="Could not save final";repaint();return;}
  std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,2,24,{},0));
  if(writer==nullptr){status="Could not write final";repaint();return;}
  stream.release();
  if(!writer->writeFromAudioSampleBuffer(buffer,0,frames)){status="Could not write final";repaint();return;}
  writer.reset();
  auto timing=parseTiming(juce::JSON::parse(timingFile(file)));
  if(timing.canAlign())timing.startStep+=static_cast<double>(startFrame)/rate*timing.bpm*4.0/60.0;
  const bool timingSaved=writeTakeTiming(out,timing);
  out.revealToUser();
  status="Final exported: "+out.getFileNameWithoutExtension();
  if(!timingSaved)status+=" (could not save timing)";
  repaint();
 }
 int soundIdForFile(const juce::File& file) const {
  const auto displayName=file.getFileNameWithoutExtension().replaceCharacter('_',' ').replaceCharacter('-',' ');
  const int index=sampleDisplayNames.indexOf(displayName);
  return index>=0?index+2:0;
 }
 void useLastRecording(){
  if(!lastRecordingFile.existsAsFile()){status="No recording yet";repaint();return;}
  loadSampleLibrary();
  lastRecordingSoundId=soundIdForFile(lastRecordingFile);
  if(lastRecordingSoundId<2){status="Could not find recording";repaint();return;}
  if(activeTrackCount==0)addNewTrack();
  if(selectedTrack>=0&&selectedTrack<activeTrackCount){setLaneSoundWithoutRole(selectedTrack,3,lastRecordingSoundId);rememberSoundChoice(selectedTrack);}
  status="Using: "+lastRecordingFile.getFileNameWithoutExtension();
  repaint();
 }
 void discardLastRecording(){
  if(!lastRecordingFile.existsAsFile()){status="No recording to discard";repaint();return;}
  const int discardedId=lastRecordingSoundId;
  for(int i=0;i<activeTrackCount;++i)if(kinds[i].getSelectedId()==3&&sounds[i].getSelectedId()==discardedId){setLaneSoundWithoutRole(i,3,1);rememberSoundChoice(i);}
  const bool ok=lastRecordingFile.deleteFile();
  const auto name=lastRecordingFile.getFileNameWithoutExtension();
  lastRecordingFile=juce::File();
  lastRecordingSoundId=0;
  loadSampleLibrary();
  status=ok?"Discarded: "+name:"Could not discard recording";
  repaint();
 }
 void toggleSoundRecording(){
  if(recordingMix.load()){status="Stop take recording first";repaint();return;}
  if(recordingSound.load()){
   recordingSound.store(false);
   recordSound.setButtonText("Capture sound");
   captureTap.stop();
   finishSoundRecording();
   return;
  }
  const double rate=std::max(8000.0,captureSampleRate.load());
  const int maxSamples=static_cast<int>(rate*30.0);
  captureBuffer.assign(static_cast<size_t>(maxSamples),0.0f);
  captureTap.start(&captureBuffer,maxSamples);
  recordingSound.store(true);
  recordSound.setButtonText("Stop");
  status="Recording 0.0s";
  repaint();
 }
 void toggleMixRecording(){
  if(mixFinishPending)return;
  if(recordingSound.load()){status="Stop sound recording first";repaint();return;}
  if(recordingMix.load()){
   recordingMix.store(false);
   mixFinishPending=true;
   recordMix.setButtonText("Finishing");
   return;
  }
  mixRecordSampleRate=std::max(8000.0,captureSampleRate.load());
  recordingMixFile=uniqueSiblingFile(mixRecordingDirectory().getChildFile(juce::File::createLegalFileName(makeMixName())).withFileExtension(".wav"));
  recordingMixFile.getParentDirectory().createDirectory();
  juce::WavAudioFormat format;
  std::unique_ptr<juce::FileOutputStream> stream(recordingMixFile.createOutputStream());
  if(stream==nullptr||!stream->openedOk()){status="Could not save take";repaint();return;}
  std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),mixRecordSampleRate,2,24,{},0));
  if(writer==nullptr){status="Could not write take";repaint();return;}
  stream.release();
  if(!recordWriterThread.isThreadRunning()&&!recordWriterThread.startThread()){status="Could not start take writer";repaint();return;}
  mixWriter=std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(),recordWriterThread,static_cast<int>(mixRecordSampleRate*2.0));
  stopTakePlayback();
  {
   const juce::ScopedLock audioLock(deviceManager.getAudioCallbackLock());
   recordingTiming=TakeTiming{};mixRecordedFrames.store(0);mixDroppedBlocks.store(0);
   if(!engine.playing.load()){engine.restartFromBeginning();engine.playing=true;play.setButtonText("Stop");}
   recordingMix.store(true);
  }
  recordMix.setButtonText("Stop take");
  status="Recording take 0:00";
  repaint();
 }
 void finishMixRecording(){
  mixFinishPending=false;
  mixWriter.reset();
  recordMix.setButtonText("Record take");
  if(mixRecordedFrames.load()<static_cast<int64_t>(mixRecordSampleRate*0.25)){
   recordingMixFile.deleteFile();status="Take too short";repaint();return;
  }
  const bool timingSaved=writeTakeTiming(recordingMixFile,recordingTiming);
  lastMixFile=recordingMixFile;
  refreshTakeList(false);
  selectTakeNamed(recordingMixFile.getFileNameWithoutExtension());
  status="Saved take: "+recordingMixFile.getFileNameWithoutExtension();
  if(!timingSaved)status+=" (could not save timing)";
  if(mixDroppedBlocks.load()>0)status+=" (audio dropped: "+juce::String(mixDroppedBlocks.load())+" blocks)";
  repaint();
 }
 void finishSoundRecording(){
  const int samples=std::min(captureTap.stop(),static_cast<int>(captureBuffer.size()));
  const double rate=std::max(8000.0,captureSampleRate.load());
  if(samples<static_cast<int>(rate*0.15)){status="Recording too short";repaint();return;}
  float peak=0.0f;
  for(int i=0;i<samples;++i)peak=std::max(peak,std::abs(captureBuffer[static_cast<size_t>(i)]));
  if(peak<0.001f){status="Recording was quiet";repaint();return;}
  juce::AudioBuffer<float> buffer(1,samples);
  const float gain=std::min(8.0f,0.82f/peak);
  for(int i=0;i<samples;++i)buffer.setSample(0,i,captureBuffer[static_cast<size_t>(i)]*gain);
  const auto name=makeSoundName();
  const auto file=userSampleDirectory().getChildFile(juce::File::createLegalFileName(name)).withFileExtension(".wav");
  file.getParentDirectory().createDirectory();
  juce::WavAudioFormat format;
  std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
  if(stream==nullptr||!stream->openedOk()){status="Could not save recording";repaint();return;}
  std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,1,24,{},0));
  if(writer==nullptr){status="Could not write recording";repaint();return;}
  stream.release();
  if(!writer->writeFromAudioSampleBuffer(buffer,0,samples)){status="Could not write recording";repaint();return;}
  lastRecordingFile=file;
  loadSampleLibrary();
  lastRecordingSoundId=soundIdForFile(file);
  if(selectedTrack>=0&&selectedTrack<activeTrackCount&&lastRecordingSoundId>=2){
   setLaneSoundWithoutRole(selectedTrack,3,lastRecordingSoundId);
   rememberSoundChoice(selectedTrack);
  }
  status="Recorded: "+file.getFileNameWithoutExtension();
  repaint();
 }
 juce::File fileForSceneName(const juce::String& name) const {
  const auto safe=juce::File::createLegalFileName(name.trim().isEmpty()?makeSceneName():name.trim());
  const auto full=sceneDirectory().getChildFile(safe+".json");
  if(full.existsAsFile())return full;
  // Earlier saves treated a dotted timestamp as an extension. Reuse that file
  // only when its stored name matches, so distinct dotted names cannot collide.
  const auto legacy=sceneDirectory().getChildFile(safe).withFileExtension(".json");
  if(legacy.existsAsFile()){
   const auto data=juce::JSON::parse(legacy);
   if(auto* root=data.getDynamicObject())if(root->getProperty("pieceName").toString().trim()==name.trim())return legacy;
  }
  return full;
 }
 juce::String savedPieceName(const juce::File& file) const {
  const auto data=juce::JSON::parse(file);
  if(auto* root=data.getDynamicObject()){
   const auto name=root->getProperty("pieceName").toString().trim();
   if(name.isNotEmpty())return name;
  }
  return file.getFileNameWithoutExtension();
 }
 void showPieceActionsMenu(){
  juce::Array<juce::File> files;
  sceneDirectory().findChildFiles(files,juce::File::findFiles,false,"*.json");
  files.sort();
  const int count=files.size();
  juce::PopupMenu menu;
  menu.addSectionHeader("Open saved Piece");
  for(int i=0;i<count;++i)menu.addItem(i+1,savedPieceName(files.getReference(i)));
  if(count==0)menu.addItem(2,"No saved Pieces",false);
  menu.addSeparator();
  menu.addItem(count+1,"New name for current setup");
  juce::PopupMenu removeMenu;
  for(int i=0;i<count;++i)removeMenu.addItem(count+2+i,savedPieceName(files.getReference(i)));
  menu.addSubMenu("Move saved Piece to Trash...",removeMenu,count>0);
  juce::Component::SafePointer<MainView> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pieceActions),[safe,files,count](int result){
   if(safe==nullptr)return;
   if(result>0&&result<=count)safe->loadScene(files.getReference(result-1));
   else if(result==count+1){
    safe->pieceId=juce::Uuid().toString();
    safe->sceneName.setText(safe->makeSceneName(),juce::dontSendNotification);
    safe->status="New Piece name ready; save when you want to keep this setup";
    safe->repaint();
   }else if(result>=count+2&&result<=2*count+1)safe->confirmRemoveSavedPiece(files.getReference(result-count-2));
  });
 }
 void migrateSingleSceneIfNeeded(){
  const auto oldFile=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Phrase Mix").getChildFile("Scene.json");
  const auto scenes=sceneDirectory();
  juce::Array<juce::File> existing;scenes.findChildFiles(existing,juce::File::findFiles,false,"*.json");
  if(oldFile.existsAsFile()&&existing.isEmpty()){
   scenes.createDirectory();
   oldFile.copyFileTo(scenes.getChildFile("Restored Scene "+juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M")).withFileExtension(".json"));
  }
 }
 void confirmRemoveSavedPiece(const juce::File& file){
  if(!file.existsAsFile()){status="That saved piece is no longer available";repaint();return;}
  juce::Component::SafePointer<MainView> safe(this);
  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
   .withTitle("Remove saved Piece?")
   .withMessage("Move the saved setup '"+file.getFileNameWithoutExtension()+"' to Trash? Captured sounds, takes, and finals stay in this Piece's folder. The current music keeps playing.")
   .withButton("Cancel").withButton("Move to Trash").withAssociatedComponent(this);
  juce::NativeMessageBox::showAsync(options,[safe,file](int result){if(result==1&&safe!=nullptr)safe->removeSavedPieceFile(file);});
 }
 void removeSavedPieceFile(const juce::File& file){
  if(!file.existsAsFile()){status="That saved piece is no longer available";repaint();return;}
  if(!file.moveToTrash()){status="Could not move saved piece to Trash";repaint();return;}
  status="Saved piece moved to Trash: "+file.getFileNameWithoutExtension();
  repaint();
 }
 juce::String sampleGroupForFile(const juce::File& file,const juce::String& rawName) const {
  const auto parent=file.getParentDirectory();
  if(parent==userSampleDirectory())return "Captured here";
  if(parent==legacySampleDirectory())return "Older captures";
  const auto name=rawName.toLowerCase();
  if(name.contains("piano")||name.contains("guitar")||name.contains("meta_nylon")||name.contains("bass")||name.contains("flute")||name.contains("oboe")||name.contains("bassoon")||name.contains("clarinet")||name.contains("recorder")||name.contains("organ"))return "Real instruments";
  if(name.contains("loop"))return "Loops";
  if(name.contains("harm"))return "Harmonics";
  if(name.contains("drum")||name.contains("hihat")||name.contains("bone")||name.contains("coin")||name.contains("chain")||name.contains("metal")||name.contains("glass")||name.contains("bowl")||name.contains("anvil")||name.contains("lid")||name.contains("sword")||name.contains("thunder")||name.contains("teeth")||name.contains("pitchfork")||name.contains("canvas")||name.contains("frame")||name.contains("medieval")||name.contains("antlers")||name.contains("last")||name.contains("scrape")||name.contains("cyborganic")||name.contains("alien"))return "Objects + textures";
  return "Other sounds";
 }
 void loadSampleLibrary(){
  const auto previousSampleNames=sampleDisplayNames;
  std::array<int,Engine::maxTracks> previousSampleSelections{};
  for(int i=0;i<Engine::maxTracks;++i)previousSampleSelections[i]=sounds[i].getSelectedId();
  std::vector<Engine::Sample> samples;
  juce::Array<juce::File> files;
  auto bundledDir=juce::File::getSpecialLocation(juce::File::currentApplicationFile).getChildFile("Contents").getChildFile("Resources").getChildFile("Samples");
  if(!bundledDir.isDirectory())bundledDir=juce::File::getCurrentWorkingDirectory().getChildFile("Assets").getChildFile("Samples");
  if(bundledDir.isDirectory())bundledDir.findChildFiles(files,juce::File::findFiles,false,"*.wav;*.aif;*.aiff");
  const auto legacyDir=legacySampleDirectory();
  if(legacyDir.isDirectory())legacyDir.findChildFiles(files,juce::File::findFiles,false,"*.wav;*.aif;*.aiff");
  const auto userDir=userSampleDirectory();
  if(userDir.isDirectory()&&userDir!=legacyDir)userDir.findChildFiles(files,juce::File::findFiles,false,"*.wav;*.aif;*.aiff");
  files.sort();
  for(auto& menu:librarySounds)menu.clear(juce::dontSendNotification);
  juce::StringArray loadedNames;
  sampleDisplayNames.clear();
  sampleGroups.clear();
  for(auto& file:files){
   std::unique_ptr<juce::AudioFormatReader> reader(audioFormats.createReaderFor(file));
   if(reader==nullptr||reader->lengthInSamples<=1)continue;
   const int length=static_cast<int>(std::min<juce::int64>(reader->lengthInSamples,static_cast<juce::int64>(reader->sampleRate*8.0)));
   juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels),length);
   reader->read(&buffer,0,length,0,true,true);
   Engine::Sample sample;sample.rate=reader->sampleRate;sample.mono.resize(static_cast<size_t>(length));
   for(int i=0;i<length;++i){
    float value=0;
    for(int ch=0;ch<buffer.getNumChannels();++ch)value+=buffer.getSample(ch,i);
    sample.mono[static_cast<size_t>(i)]=value/static_cast<float>(std::max(1,buffer.getNumChannels()));
   }
   samples.push_back(std::move(sample));
   const auto rawName=file.getFileNameWithoutExtension();
   loadedNames.add(rawName);
   auto displayName=rawName.replaceCharacter('_',' ').replaceCharacter('-',' ');
   sampleDisplayNames.add(displayName);
   sampleGroups.add(sampleGroupForFile(file,rawName));
  }
  for(auto& menu:librarySounds)addGroupedSamplesToMenu(menu,true);
  for(auto& menu:librarySounds)menu.setSelectedId(1,juce::dontSendNotification);
	  std::vector<Engine::MappedInstrument> mapped(11);
	  mapped[5].gain=8.0f;
  auto sampleIndex=[&](const juce::String& name){return loadedNames.indexOf(name);};
  auto addZone=[&](int instrument,const juce::String& name,int root,int low,int high){
   const int index=sampleIndex(name);
   if(index>=0) mapped[static_cast<size_t>(instrument)].zones.push_back({index,root,low,high});
  };
  addZone(0,"bass_a_major_loop",45,24,46);
  addZone(0,"bass_c_minor_loop",48,47,72);
  addZone(1,"guitar_db_minor_loop",61,40,63);
  addZone(1,"guitar_fs_major_loop",66,64,88);
  addZone(2,"d-harm",62,48,63);
  addZone(2,"e-harm",64,64,67);
  addZone(2,"a-harm",69,68,88);
  addZone(3,"clarinet_d2_sus",50,24,51);
  addZone(3,"clarinet_f2_sus",53,52,55);
  addZone(3,"clarinet_as2_sus",58,56,60);
  addZone(3,"clarinet_d3_sus",62,61,63);
  addZone(3,"clarinet_f3_sus",65,64,67);
  addZone(3,"clarinet_as3_sus",70,68,72);
  addZone(3,"clarinet_d4_sus",74,73,75);
  addZone(3,"clarinet_f4_sus",77,76,79);
  addZone(3,"clarinet_as4_sus",82,80,84);
  addZone(3,"clarinet_d5_sus",86,85,88);
  addZone(4,"flute_c3_sus",60,48,62);
  addZone(4,"flute_e3_sus",64,63,66);
  addZone(4,"flute_a3_sus",69,67,70);
  addZone(4,"flute_c4_sus",72,71,74);
  addZone(4,"flute_e4_sus",76,75,78);
  addZone(4,"flute_a4_sus",81,79,88);
  addZone(5,"oboe_as2_sus",58,48,60);
  addZone(5,"oboe_d3_sus",62,61,63);
  addZone(5,"oboe_f3_sus",65,64,67);
  addZone(5,"oboe_as3_sus",70,68,72);
  addZone(5,"oboe_d4_sus",74,73,75);
  addZone(5,"oboe_f4_sus",77,76,88);
  addZone(6,"bassoon_as0_sus",34,12,37);
  addZone(6,"bassoon_f1_sus",41,38,44);
  addZone(6,"bassoon_c2_sus",48,45,51);
  addZone(6,"bassoon_g2_sus",55,52,57);
  addZone(6,"bassoon_c3_sus",60,58,64);
  addZone(6,"bassoon_a3_sus",69,65,88);
  addZone(7,"recorder_c3_sus",60,48,62);
  addZone(7,"recorder_e3_sus",64,63,66);
  addZone(7,"recorder_gs3_sus",68,67,70);
  addZone(7,"recorder_c4_sus",72,71,74);
  addZone(7,"recorder_e4_sus",76,75,88);
  addZone(8,"organ_c2_quiet",48,12,53);
  addZone(8,"organ_c3_quiet",60,54,65);
  addZone(8,"organ_c4_quiet",72,66,88);
  addZone(9,"piano_c2_sus",36,24,45);
  addZone(9,"piano_f2_sus",41,46,53);
  addZone(9,"piano_a2_sus",57,54,58);
  addZone(9,"piano_c3_sus",60,59,62);
  addZone(9,"piano_e3_sus",64,63,65);
  addZone(9,"piano_g3_sus",67,66,69);
  addZone(9,"piano_b3_sus",71,70,72);
  addZone(9,"piano_d4_sus",74,73,75);
  addZone(9,"piano_f4_sus",77,76,79);
	  addZone(9,"piano_a4_sus",81,80,82);
	  addZone(9,"piano_c5_sus",84,83,86);
	  addZone(9,"piano_g5_sus",91,87,96);
	  addZone(10,"meta_nylon_e2",40,24,43);
	  addZone(10,"meta_nylon_a2",45,44,47);
	  addZone(10,"meta_nylon_d3",50,48,53);
	  addZone(10,"meta_nylon_g3",55,54,57);
	  addZone(10,"meta_nylon_b3",59,58,61);
	  addZone(10,"meta_nylon_e4",64,62,66);
	  addZone(10,"meta_nylon_a4",69,67,70);
	  addZone(10,"meta_nylon_c5",72,71,88);
  engine.setSoundBank(std::move(samples),std::move(mapped));
  auto remapSample=[&](int id){
   if(id==1)return 1;
   const int oldIndex=id-2;
   if(oldIndex<0||oldIndex>=previousSampleNames.size())return 0;
   const int newIndex=sampleDisplayNames.indexOf(previousSampleNames[oldIndex]);
   return newIndex>=0?newIndex+2:0;
  };
  for(int i=0;i<Engine::maxTracks;++i){
   const int selected=kinds[i].getSelectedId()==3?remapSample(previousSampleSelections[i]):previousSampleSelections[i];
   auto& history=soundHistory[i];
   const int oldHistoryIndex=soundHistoryIndex[i];
   std::vector<SoundChoice> remapped;
   int newHistoryIndex=0;
   for(int j=0;j<static_cast<int>(history.size());++j){
    auto choice=history[static_cast<size_t>(j)];
    if(choice.kind==3)choice.sound=remapSample(choice.sound);
    if(!validSoundChoice(choice))continue;
    if(j<=oldHistoryIndex)newHistoryIndex=static_cast<int>(remapped.size());
    remapped.push_back(choice);
   }
   history=std::move(remapped);
   soundHistoryIndex[i]=std::clamp(newHistoryIndex,0,std::max(0,static_cast<int>(history.size())-1));
   refreshSoundMenu(i,false,false);
   sounds[i].setSelectedId(selected>0?selected:1,juce::dontSendNotification);
   applySoundChoice(i);
   if(i<activeTrackCount)rememberSoundChoice(i);
  }
  if(engine.sampleCount()>0)status="Loaded "+juce::String(static_cast<int>(engine.sampleCount()))+" library sounds";
 }
 juce::DynamicObject::Ptr makeStateObject(){
  juce::DynamicObject::Ptr root=new juce::DynamicObject();
  root->setProperty("version",7);
  root->setProperty("transportStartStep",engine.transportStartStep.load());
  root->setProperty("trackCount",activeTrackCount);
  root->setProperty("selectedTrack",selectedTrack);
  root->setProperty("soloTrack",engine.soloTrack.load());
  root->setProperty("focusTrack",engine.focusTrack.load());
  root->setProperty("focusFade",focusFade.getValue());
  root->setProperty("randomState",static_cast<juce::int64>(engine.randomState()));
  root->setProperty("tempo",tempo.getValue());root->setProperty("density",density.getValue());
  root->setProperty("key",key.getSelectedId());root->setProperty("scale",scale.getSelectedId());root->setProperty("shape",shape.getSelectedId());root->setProperty("mode",mode.getSelectedId());root->setProperty("every",static_cast<int>(changeEvery.getValue()));
  root->setProperty("room",room.getValue());root->setProperty("wet",wet.getValue());root->setProperty("drive",drive.getValue());root->setProperty("filter",filter.getValue());
  root->setProperty("effect",pluginEffect.hasPlugin()?pluginEffect.saveState():pendingEffectState);
  juce::Array<juce::var> lanes;
  for(int i=0;i<activeTrackCount;++i){
   juce::DynamicObject::Ptr lane=new juce::DynamicObject();
   if(kinds[i].getSelectedId()==6){
    lane->setProperty("backingFile",backingFiles[i].getFullPathName());lane->setProperty("backingName",backingFiles[i].getFileName());
    lane->setProperty("backingLoop",backingLoopFlags[i].load());lane->setProperty("backingTiming",timingObject(backingTimings[i]));
   }
   lane->setProperty("name",trackNames[i]);lane->setProperty("subtitle",trackSubtitles[i]);
   juce::Array<juce::var> effects;
   for(const auto& slot:trackEffectSlots[i])effects.add(slot.effect->hasPlugin()?slot.effect->saveState():slot.pending);
   lane->setProperty("effects",juce::var(effects));
   const auto& instrument=trackInstruments[i];
   lane->setProperty("instrumentPlugin",!instrument.pending.isVoid()?instrument.pending:instrument.effect->saveState());
   lane->setProperty("kind",kinds[i].getSelectedId());lane->setProperty("sound",sounds[i].getSelectedId());
   if(kinds[i].getSelectedId()==3&&sounds[i].getSelectedId()>=2){const int index=sounds[i].getSelectedId()-2;if(index<sampleDisplayNames.size())lane->setProperty("soundName",sampleDisplayNames[index]);}
	   lane->setProperty("source",sources[i].getSelectedId());lane->setProperty("material",materials[i].getSelectedId());lane->setProperty("playMode",playModes[i].getSelectedId());lane->setProperty("sampleStart",sampleStarts[i].getValue());lane->setProperty("sampleSize",sampleSizes[i].getValue());lane->setProperty("sampleFade",sampleFades[i].getValue());lane->setProperty("sampleReverse",sampleReverses[i].getToggleState());lane->setProperty("role",roles[i].getSelectedId());lane->setProperty("relationship",relationships[i].getValue());lane->setProperty("dynamics",dynamics[i].getValue());lane->setProperty("articulation",articulations[i].getValue());lane->setProperty("tail",tails[i].getValue());lane->setProperty("length",lengths[i].getSelectedId());lane->setProperty("loopSteps",static_cast<int>(loopLengths[i].getValue()));lane->setProperty("rhythm",engine.rhythmMode[i].load());lane->setProperty("hits",engine.euclideanHits[i].load());lane->setProperty("rotation",engine.euclideanRotate[i].load());lane->setProperty("librarySound",librarySounds[i].getSelectedId());lane->setProperty("instrument",instruments[i].getSelectedId());
   lane->setProperty("level",std::round(engine.level[i].load()*100.0f));lane->setProperty("fadeTarget",fadeTargets[i]);lane->setProperty("fadeSeconds",fadeDurations[i]);lane->setProperty("send",sends[i].getValue());lane->setProperty("color",colors[i].getValue());lane->setProperty("move",moves[i].getValue());lane->setProperty("weather",weathers[i].getValue());
   lane->setProperty("hold",holds[i].getToggleState());lane->setProperty("mute",mutes[i].getToggleState());
   lane->setProperty("connect",connects[i].getToggleState());
   juce::Array<juce::var> lanePhrase;for(int note:engine.getTrackPhrase(i))lanePhrase.add(note);
   lane->setProperty("phrase",juce::var(lanePhrase));
   juce::Array<juce::var> longPhrase;for(int note:engine.getTrackPattern(i))longPhrase.add(note);
   lane->setProperty("longPhrase",juce::var(longPhrase));
   juce::Array<juce::var> history;
   for(const auto& candidate:patternHistory[i]){
    juce::DynamicObject::Ptr entry=new juce::DynamicObject();
    juce::Array<juce::var> phrase;for(int j=0;j<Engine::steps;++j)phrase.add(candidate.phrase[j]);
    entry->setProperty("phrase",juce::var(phrase));
    juce::Array<juce::var> longPhrase;for(int note:candidate.phrase)longPhrase.add(note);
    entry->setProperty("longPhrase",juce::var(longPhrase));
    entry->setProperty("role",candidate.role);
    entry->setProperty("relationship",candidate.relationship);
    entry->setProperty("dynamics",candidate.dynamics);
    entry->setProperty("articulation",candidate.articulation);
    entry->setProperty("motion",candidate.motion);
    entry->setProperty("texture",candidate.texture);
    entry->setProperty("length",candidate.length);
    entry->setProperty("loopSteps",candidate.loopSteps);
    entry->setProperty("rhythm",candidate.rhythm);entry->setProperty("hits",candidate.hits);entry->setProperty("rotation",candidate.rotation);
    history.add(juce::var(entry.get()));
   }
   lane->setProperty("patternHistory",juce::var(history));
   lane->setProperty("patternHistoryIndex",patternHistoryIndex[i]);
   juce::Array<juce::var> triedSounds;
   for(const auto& choice:soundHistory[i]){
    juce::DynamicObject::Ptr entry=new juce::DynamicObject();
    entry->setProperty("kind",choice.kind);entry->setProperty("sound",choice.sound);
    if(choice.kind==3&&choice.sound>=2){const int index=choice.sound-2;if(index<sampleDisplayNames.size())entry->setProperty("soundName",sampleDisplayNames[index]);}
    triedSounds.add(juce::var(entry.get()));
   }
   lane->setProperty("soundHistory",juce::var(triedSounds));
   lane->setProperty("soundHistoryIndex",soundHistoryIndex[i]);
   lanes.add(juce::var(lane.get()));
  }
  root->setProperty("lanes",juce::var(lanes));
  if(activeTrackCount>0){juce::Array<juce::var> phrase;for(int note:engine.getPhrase())phrase.add(note);root->setProperty("phrase",juce::var(phrase));juce::Array<juce::var> longPhrase;for(int note:engine.getTrackPattern(0))longPhrase.add(note);root->setProperty("longPhrase",juce::var(longPhrase));}
  return root;
 }
 bool applyStateObject(juce::DynamicObject* root){
  if(root==nullptr)return false;
  for(int i=0;i<Engine::maxTracks;++i){backingFiles[i]=juce::File();backingClips[i].reset();backingTimings[i]=TakeTiming{};backingLoopFlags[i]=false;backingLoops[i].setToggleState(false,juce::dontSendNotification);}
  const double startStep=root->hasProperty("transportStartStep")?static_cast<double>(root->getProperty("transportStartStep")):0.0;
  engine.transportStartStep=std::isfinite(startStep)?std::clamp(startStep,0.0,1.0e12):0.0;
  pluginPickerWindow.reset();pickerTarget=nullptr;
  for(auto& chain:trackEffectSlots)for(auto& slot:chain){
   ++slot.revision;slot.editor.reset();slot.effect->clear();slot.pending=juce::var();slot.pendingName.clear();
  }
  for(auto& slot:trackInstruments){
   ++slot.revision;slot.editor.reset();slot.effect->clear();slot.pending=juce::var();slot.pendingName.clear();
  }
  restorePluginEffect(root->getProperty("effect"));
  for(int i=0;i<Engine::maxTracks;++i){engine.cancelTrackFade(i);fadeWasActive[i]=false;}
  setSoloTrack(-1);
  setFocusTrack(-1);
  tempo.setValue(root->getProperty("tempo"));density.setValue(root->getProperty("density"));
  key.setSelectedId(static_cast<int>(root->getProperty("key")));scale.setSelectedId(static_cast<int>(root->getProperty("scale")));if(root->hasProperty("shape"))shape.setSelectedId(static_cast<int>(root->getProperty("shape")));mode.setSelectedId(static_cast<int>(root->getProperty("mode")));
  changeEvery.setValue(root->hasProperty("every")?static_cast<int>(root->getProperty("every")):4);
  changeEvery.setEnabled(mode.getSelectedId()!=1);
  room.setValue(root->getProperty("room"));wet.setValue(root->getProperty("wet"));drive.setValue(root->getProperty("drive"));filter.setValue(root->getProperty("filter"));
  focusFade.setValue(root->hasProperty("focusFade")?static_cast<double>(root->getProperty("focusFade")):1.5);
  engine.focusFadeSeconds=static_cast<float>(focusFade.getValue());
  const auto lanes=root->getProperty("lanes");
  if(root->hasProperty("trackCount"))setTrackCount(static_cast<int>(root->getProperty("trackCount")));
  else if(auto* laneArray=lanes.getArray())setTrackCount(std::min(Engine::maxTracks,laneArray->size()));
  if(auto* laneArray=lanes.getArray())for(int i=0;i<std::min(Engine::maxTracks,laneArray->size());++i)if(auto* lane=laneArray->getReference(i).getDynamicObject()){
   if(i>=activeTrackCount)break;
   if(lane->hasProperty("name"))trackNames[i]=lane->getProperty("name").toString();
   if(lane->hasProperty("subtitle"))trackSubtitles[i]=lane->getProperty("subtitle").toString();
   if(lane->hasProperty("kind")&&lane->hasProperty("sound")){
    const int kind=static_cast<int>(lane->getProperty("kind"));
    int sound=static_cast<int>(lane->getProperty("sound"));
    if(kind==3&&lane->hasProperty("soundName")){const int index=sampleDisplayNames.indexOf(lane->getProperty("soundName").toString());sound=index>=0?index+2:1;}
    setKindAndSound(i,kind,sound);
   }
   else migrateOldSourceToKindSound(i,static_cast<int>(lane->getProperty("source")),lane->hasProperty("instrument")?static_cast<int>(lane->getProperty("instrument"))-1:0,lane->hasProperty("librarySound")?static_cast<int>(lane->getProperty("librarySound")):1);
   soundHistory[i].clear();
   const auto triedSounds=lane->getProperty("soundHistory");
   if(auto* entries=triedSounds.getArray())for(const auto& entryVar:*entries)if(auto* entry=entryVar.getDynamicObject()){
    SoundChoice choice{static_cast<int>(entry->getProperty("kind")),static_cast<int>(entry->getProperty("sound"))};
    if(choice.kind==3&&entry->hasProperty("soundName")){const int index=sampleDisplayNames.indexOf(entry->getProperty("soundName").toString());choice.sound=index>=0?index+2:0;}
    if(validSoundChoice(choice)&&soundHistory[i].size()<24)soundHistory[i].push_back(choice);
   }
   if(soundHistory[i].empty())soundHistory[i].push_back({kinds[i].getSelectedId(),sounds[i].getSelectedId()});
   soundHistoryIndex[i]=lane->hasProperty("soundHistoryIndex")?std::clamp(static_cast<int>(lane->getProperty("soundHistoryIndex")),0,static_cast<int>(soundHistory[i].size())-1):static_cast<int>(soundHistory[i].size())-1;
   if(!(soundHistory[i][static_cast<size_t>(soundHistoryIndex[i])]==SoundChoice{kinds[i].getSelectedId(),sounds[i].getSelectedId()}))rememberSoundChoice(i);
   if(kinds[i].getSelectedId()==6){
    const auto local=backingDirectory().getChildFile(juce::File::createLegalFileName(lane->getProperty("backingName").toString()));
    const auto path=lane->getProperty("backingFile").toString();
    const auto file=local.existsAsFile()?local:juce::File::isAbsolutePath(path)?juce::File(path):juce::File();
    backingFiles[i]=file;backingTimings[i]=parseTiming(lane->getProperty("backingTiming"));
    backingClips[i]=readBackingClip(file);
    backingLoopFlags[i]=static_cast<bool>(lane->getProperty("backingLoop"));backingLoops[i].setToggleState(backingLoopFlags[i].load(),juce::dontSendNotification);
   }
   if(lane->hasProperty("source"))sources[i].setSelectedId(static_cast<int>(lane->getProperty("source")),juce::dontSendNotification);
	   if(lane->hasProperty("material")){materials[i].setSelectedId(static_cast<int>(lane->getProperty("material")),juce::dontSendNotification);engine.material[i]=materials[i].getSelectedId()-1;}
	   if(lane->hasProperty("playMode")){playModes[i].setSelectedId(static_cast<int>(lane->getProperty("playMode")),juce::dontSendNotification);engine.samplePlayMode[i]=playModes[i].getSelectedId()-1;}
	   if(lane->hasProperty("sampleStart"))sampleStarts[i].setValue(lane->getProperty("sampleStart"));
	   if(lane->hasProperty("sampleSize"))sampleSizes[i].setValue(lane->getProperty("sampleSize"));
	   if(lane->hasProperty("sampleFade"))sampleFades[i].setValue(lane->getProperty("sampleFade"));
	   if(lane->hasProperty("sampleReverse"))sampleReverses[i].setToggleState(static_cast<bool>(lane->getProperty("sampleReverse")),juce::sendNotification);
	   if(lane->hasProperty("role"))roles[i].setSelectedId(static_cast<int>(lane->getProperty("role")));
	   if(lane->hasProperty("relationship"))relationships[i].setValue(lane->getProperty("relationship"));
	   if(lane->hasProperty("dynamics"))dynamics[i].setValue(lane->getProperty("dynamics"));
	   if(lane->hasProperty("articulation"))articulations[i].setValue(lane->getProperty("articulation"));
	   tails[i].setValue(lane->hasProperty("tail")?static_cast<double>(lane->getProperty("tail")):0.0);
	   if(lane->hasProperty("length"))lengths[i].setSelectedId(static_cast<int>(lane->getProperty("length")));
   const int savedLoopSteps=lane->hasProperty("loopSteps")?std::clamp(static_cast<int>(lane->getProperty("loopSteps")),1,Engine::maxLoopSteps):Engine::steps;
   loopLengths[i].setValue(savedLoopSteps,juce::dontSendNotification);engine.setLoopSteps(i,savedLoopSteps);
   setRhythmSettings(i,lane->hasProperty("rhythm")?std::clamp(static_cast<int>(lane->getProperty("rhythm")),0,1):0,lane->hasProperty("hits")?static_cast<int>(lane->getProperty("hits")):5,lane->hasProperty("rotation")?static_cast<int>(lane->getProperty("rotation")):0);
   if(lane->hasProperty("librarySound"))librarySounds[i].setSelectedId(static_cast<int>(lane->getProperty("librarySound")),juce::dontSendNotification);
   if(lane->hasProperty("instrument"))instruments[i].setSelectedId(static_cast<int>(lane->getProperty("instrument")),juce::dontSendNotification);
   levels[i].setValue(lane->getProperty("level"));setManualTrackLevel(i,levels[i].getValue());
   fadeTargets[i]=lane->hasProperty("fadeTarget")?std::clamp(static_cast<double>(lane->getProperty("fadeTarget")),0.0,100.0):0.0;
   fadeDurations[i]=lane->hasProperty("fadeSeconds")?std::clamp(static_cast<double>(lane->getProperty("fadeSeconds")),0.5,120.0):12.0;
   sends[i].setValue(lane->getProperty("send"));colors[i].setValue(lane->getProperty("color"));moves[i].setValue(lane->getProperty("move"));weathers[i].setValue(lane->getProperty("weather"));
   holds[i].setToggleState(static_cast<bool>(lane->getProperty("hold")),juce::sendNotification);mutes[i].setToggleState(static_cast<bool>(lane->getProperty("mute")),juce::sendNotification);
   connects[i].setToggleState(lane->hasProperty("connect")?static_cast<bool>(lane->getProperty("connect")):false,juce::sendNotification);
   const auto lanePhraseVar=lane->hasProperty("longPhrase")?lane->getProperty("longPhrase"):lane->getProperty("phrase");
   if(auto* phraseArray=lanePhraseVar.getArray())if(phraseArray->size()==Engine::steps||phraseArray->size()==Engine::maxLoopSteps){std::array<int,Engine::maxLoopSteps> phrase{};phrase.fill(-1);for(int j=0;j<phraseArray->size();++j)phrase[j]=static_cast<int>(phraseArray->getReference(j));engine.setTrackPattern(i,phrase);}
   patternHistory[i].clear();
   const auto historyValue=lane->getProperty("patternHistory");
   if(auto* historyArray=historyValue.getArray()){
    for(const auto& candidateVar:*historyArray){
     const auto* entry=candidateVar.getDynamicObject();
     const auto phraseVar=entry!=nullptr?(entry->hasProperty("longPhrase")?entry->getProperty("longPhrase"):entry->getProperty("phrase")):candidateVar;
     if(auto* phrase=phraseVar.getArray())if(phrase->size()==Engine::steps||phrase->size()==Engine::maxLoopSteps){
      PatternCandidate candidate;
      candidate.phrase.fill(-1);
      for(int j=0;j<phrase->size();++j)candidate.phrase[j]=static_cast<int>(phrase->getReference(j));
      candidate.role=entry!=nullptr&&entry->hasProperty("role")?std::clamp(static_cast<int>(entry->getProperty("role")),1,4):std::clamp(roles[i].getSelectedId(),1,4);
      candidate.relationship=entry!=nullptr&&entry->hasProperty("relationship")?std::clamp(static_cast<int>(entry->getProperty("relationship")),0,100):static_cast<int>(relationships[i].getValue());
      candidate.dynamics=entry!=nullptr&&entry->hasProperty("dynamics")?std::clamp(static_cast<int>(entry->getProperty("dynamics")),0,100):static_cast<int>(dynamics[i].getValue());
      candidate.articulation=entry!=nullptr&&entry->hasProperty("articulation")?std::clamp(static_cast<int>(entry->getProperty("articulation")),0,100):static_cast<int>(articulations[i].getValue());
      candidate.motion=entry!=nullptr&&entry->hasProperty("motion")?std::clamp(static_cast<int>(entry->getProperty("motion")),0,100):static_cast<int>(moves[i].getValue());
      candidate.texture=entry!=nullptr&&entry->hasProperty("texture")?std::clamp(static_cast<int>(entry->getProperty("texture")),0,100):static_cast<int>(weathers[i].getValue());
      candidate.length=entry!=nullptr&&entry->hasProperty("length")?std::clamp(static_cast<int>(entry->getProperty("length")),1,4):std::clamp(lengths[i].getSelectedId(),1,4);
      candidate.loopSteps=entry!=nullptr&&entry->hasProperty("loopSteps")?std::clamp(static_cast<int>(entry->getProperty("loopSteps")),1,Engine::maxLoopSteps):savedLoopSteps;
      candidate.rhythm=entry!=nullptr&&entry->hasProperty("rhythm")?std::clamp(static_cast<int>(entry->getProperty("rhythm")),0,1):0;
      candidate.hits=entry!=nullptr&&entry->hasProperty("hits")?std::clamp(static_cast<int>(entry->getProperty("hits")),0,candidate.loopSteps):std::min(5,candidate.loopSteps);
      candidate.rotation=entry!=nullptr&&entry->hasProperty("rotation")?std::clamp(static_cast<int>(entry->getProperty("rotation")),0,candidate.loopSteps-1):0;
      if(patternHistory[i].size()<24)patternHistory[i].push_back(candidate);
     }
    }
   }
   if(patternHistory[i].empty())rememberPattern(i,false);
   patternHistoryIndex[i]=lane->hasProperty("patternHistoryIndex")?std::clamp(static_cast<int>(lane->getProperty("patternHistoryIndex")),0,static_cast<int>(patternHistory[i].size())-1):static_cast<int>(patternHistory[i].size())-1;
   const auto savedEffects=lane->getProperty("effects");
   if(auto* effects=savedEffects.getArray()){
    for(int effectIndex=0;effectIndex<trackEffectCount;++effectIndex)
     restoreTrackPluginEffect(i,effectIndex,effectIndex<effects->size()?effects->getReference(effectIndex):juce::var());
   }else{
    restoreTrackPluginEffect(i,0,lane->getProperty("effect"));
    restoreTrackPluginEffect(i,1,{});
   }
   restoreInstrument(i,lane->getProperty("instrumentPlugin"));
  }
  const auto phraseVar=root->hasProperty("longPhrase")?root->getProperty("longPhrase"):root->getProperty("phrase");
  if(activeTrackCount>0)if(auto* phraseArray=phraseVar.getArray())if(phraseArray->size()==Engine::steps||phraseArray->size()==Engine::maxLoopSteps){std::array<int,Engine::maxLoopSteps> phrase{};phrase.fill(-1);for(int i=0;i<phraseArray->size();++i)phrase[i]=static_cast<int>(phraseArray->getReference(i));engine.setTrackPattern(0,phrase);}
  if(root->hasProperty("selectedTrack"))selectTrack(static_cast<int>(root->getProperty("selectedTrack")));
  else selectTrack(selectedTrack);
  if(root->hasProperty("soloTrack"))setSoloTrack(static_cast<int>(root->getProperty("soloTrack")));
  if(root->hasProperty("focusTrack"))setFocusTrack(static_cast<int>(root->getProperty("focusTrack")));
  if(root->hasProperty("randomState"))engine.seed(static_cast<uint32_t>(static_cast<juce::int64>(root->getProperty("randomState"))));
  midiMappings.resetPickup();
  updateHostedInstrumentMask();engine.restartFromBeginning();
  resized();repaint();
  return true;
 }
 void saveScene(){
  auto root=makeStateObject();
  root->setProperty("pieceName",currentPieceName());
  root->setProperty("pieceId",pieceId);
  currentPieceDirectory().createDirectory();
  const auto file=fileForSceneName(sceneName.getText());file.getParentDirectory().createDirectory();
  const bool ok=file.replaceWithText(juce::JSON::toString(juce::var(root.get()),true));
  status=ok?"Piece saved: "+currentPieceName():"Save failed";
  repaint();
 }
 void loadScene(const juce::File& file){
  if(!file.existsAsFile()){status="That saved piece is no longer available";repaint();return;}
  juce::var data=juce::JSON::parse(file);auto* root=data.getDynamicObject();if(root==nullptr){status="Could not read piece";repaint();return;}
  const auto savedId=root->getProperty("pieceId").toString().trim();
  const auto storedName=root->getProperty("pieceName").toString().trim();
  const auto name=storedName.isNotEmpty()?storedName:file.getFileNameWithoutExtension();
  pieceId=savedId.isNotEmpty()?savedId:legacyPieceId(name);
  sceneName.setText(name,juce::dontSendNotification);
  loadSampleLibrary();
  refreshTakeList(false);
  if(!applyStateObject(root)){status="Could not read piece";repaint();return;}
  status="Piece loaded: "+name;
  repaint();
 }
 juce::File keepsDirectory() const {return appDataDirectory().getChildFile("Keeps");}
 void saveKeep(){
  if(activeTrackCount==0){status="Add a track before keeping a piece";repaint();return;}
  if(recordingSound.load()){status="Finish capturing the sound before keeping this piece";repaint();return;}
  const auto stamp=juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S");
  const auto name=juce::File::createLegalFileName(currentPieceName()+" "+stamp);
  const auto folder=uniqueSiblingFile(keepsDirectory().getChildFile(name));
  if(!folder.createDirectory().wasOk()){status="Could not create Keep";repaint();return;}
  const auto source=userSampleDirectory();
  if(source.isDirectory()&&!source.copyDirectoryTo(folder.getChildFile("Sources"))){status="Could not copy captured sounds into Keep";repaint();return;}
  for(int lane=0;lane<activeTrackCount;++lane)if(kinds[lane].getSelectedId()==6){
   const auto source=backingFiles[lane];const auto destination=folder.getChildFile("Backing Tracks").getChildFile(source.getFileName());
   if(!source.existsAsFile()||!destination.getParentDirectory().createDirectory().wasOk()||!source.copyFileTo(destination)){status="Could not copy backing audio into Keep";repaint();return;}
   if(!writeTakeTiming(destination,backingTimings[lane])){status="Could not copy backing timing into Keep";repaint();return;}
  }
  const auto olderSources=legacySampleDirectory();
  if(olderSources.isDirectory()&&!olderSources.copyDirectoryTo(folder.getChildFile("Older Sources"))){status="Could not copy older sounds into Keep";repaint();return;}
  auto root=makeStateObject();
  root->setProperty("pieceName",currentPieceName());
  root->setProperty("pieceId",pieceId);
  root->setProperty("keptAt",stamp);
  const bool ok=folder.getChildFile("Keep.json").replaceWithText(juce::JSON::toString(juce::var(root.get()),true));
  status=ok?"Kept: "+folder.getFileName():"Could not write Keep";
  repaint();
 }
 void showKeepsMenu(){
  juce::Array<juce::File> folders;
  keepsDirectory().findChildFiles(folders,juce::File::findDirectories,false);
  std::vector<juce::File> saved;
  for(const auto& folder:folders)if(folder.getChildFile("Keep.json").existsAsFile())saved.push_back(folder);
  std::sort(saved.begin(),saved.end(),[](const auto& a,const auto& b){return a.getLastModificationTime()>b.getLastModificationTime();});
  if(saved.empty()){status="No Keeps yet";repaint();return;}
  juce::PopupMenu menu;
  menu.addSectionHeader("This Piece");
  int pieceKeepCount=0;
  for(int i=0;i<static_cast<int>(saved.size());++i){
   const auto data=juce::JSON::parse(saved[static_cast<size_t>(i)].getChildFile("Keep.json"));
   if(auto* root=data.getDynamicObject()){
    const auto savedId=root->getProperty("pieceId").toString().trim();
    const auto savedName=root->getProperty("pieceName").toString().trim();
    const bool matches=savedId.isNotEmpty()?savedId==pieceId
     :legacyPieceId(savedName)==pieceId||savedName.equalsIgnoreCase(currentPieceName());
    if(matches){menu.addItem(i+1,saved[static_cast<size_t>(i)].getFileName());++pieceKeepCount;}
   }
  }
  if(pieceKeepCount==0)menu.addItem(static_cast<int>(saved.size())*3+1,"No Keeps for this Piece",false);
  juce::PopupMenu allMenu;
  for(int i=0;i<static_cast<int>(saved.size());++i)allMenu.addItem(static_cast<int>(saved.size())+i+1,saved[static_cast<size_t>(i)].getFileName());
  menu.addSeparator();
  menu.addSubMenu("All Keeps",allMenu);
  juce::PopupMenu removeMenu;
  for(int i=0;i<static_cast<int>(saved.size());++i)removeMenu.addItem(2*static_cast<int>(saved.size())+i+1,saved[static_cast<size_t>(i)].getFileName());
  menu.addSubMenu("Move Keep to Trash",removeMenu);
  juce::Component::SafePointer<MainView> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(keeps),[safe,saved](int result){
   const int count=static_cast<int>(saved.size());
   if(result>0&&result<=2*count&&safe!=nullptr)safe->confirmRecallKeep(saved[static_cast<size_t>((result-1)%count)]);
   if(result>2*count&&result<=3*count&&safe!=nullptr)safe->confirmRemoveKeep(saved[static_cast<size_t>(result-2*count-1)]);
  });
 }
 void confirmRemoveKeep(const juce::File& folder){
  if(!folder.getChildFile("Keep.json").existsAsFile()){status="That Keep is no longer available";repaint();return;}
  juce::Component::SafePointer<MainView> safe(this);
  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
   .withTitle("Remove Keep?")
   .withMessage("Move '"+folder.getFileName()+"' and its copied sounds to Trash? The current piece keeps playing.")
   .withButton("Cancel").withButton("Move to Trash").withAssociatedComponent(this);
  juce::NativeMessageBox::showAsync(options,[safe,folder](int result){if(result==1&&safe!=nullptr)safe->removeKeepFolder(folder);});
 }
 void removeKeepFolder(const juce::File& folder){
  if(!folder.getChildFile("Keep.json").existsAsFile()){status="That Keep is no longer available";repaint();return;}
  status=folder.moveToTrash()?"Keep moved to Trash: "+folder.getFileName():"Could not move Keep to Trash";
  repaint();
 }
 void confirmRecallKeep(const juce::File& folder){
  juce::Component::SafePointer<MainView> safe(this);
  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
   .withTitle("Recall Keep?").withMessage("Open "+folder.getFileName()+" in a new piece? Unsaved changes in the current piece will be replaced.")
   .withButton("Cancel").withButton("Recall in Still").withAssociatedComponent(this);
  juce::NativeMessageBox::showAsync(options,[safe,folder](int result){if(result==1&&safe!=nullptr)safe->recallKeep(folder);});
 }
 void recallKeep(const juce::File& folder){
  const auto data=juce::JSON::parse(folder.getChildFile("Keep.json"));
  auto* root=data.getDynamicObject();
  if(root==nullptr){status="Could not read Keep";repaint();return;}
  const auto branchName=uniqueSiblingFile(appDataDirectory().getChildFile("Pieces").getChildFile(folder.getFileName()+" Recall")).getFileName();
  const auto branch=appDataDirectory().getChildFile("Pieces").getChildFile(branchName);
  if(!branch.createDirectory().wasOk()){status="Could not create recalled piece";repaint();return;}
  const auto captured=branch.getChildFile("Captured Sounds");
  for(const auto& source:{folder.getChildFile("Older Sources"),folder.getChildFile("Sources")})
   if(source.isDirectory()&&!source.copyDirectoryTo(captured)){status="Could not restore captured sounds";repaint();return;}
  if(folder.getChildFile("Backing Tracks").isDirectory()&&!folder.getChildFile("Backing Tracks").copyDirectoryTo(branch.getChildFile("Backing Tracks"))){status="Could not restore backing tracks";repaint();return;}
  engine.playing=false;takePlaying=false;
  pieceId=juce::Uuid().toString();
  sceneName.setText(branchName,juce::dontSendNotification);
  loadSampleLibrary();
  refreshTakeList(false);
  if(!applyStateObject(root)){status="Could not restore Keep";repaint();return;}
  mode.setSelectedId(1);
  engine.playing=true;
  play.setButtonText("Stop");
  status="Recalled in Still: "+folder.getFileName();
  repaint();
 }
 void setupHelp(){
  help.setTooltip("Open a plain-language guide to the app.");
  keep.setTooltip("Capture this exact track setup as a new, non-overwriting Keep, including its captured sounds.");
  keeps.setTooltip("Find this Piece's Keeps, browse all Keeps, or move one to Trash.");
  play.setTooltip("With no tracks, create a Main pattern and start playback. Otherwise start or stop playback.");
  generate.setTooltip("Make a fresh phrase using the current key, scale, shape, density, and held tracks.");
  randomize.setTooltip("Change the entire piece: tempo, musical behavior, patterns, sounds, levels, and effects.");
  addTrack.setTooltip("Add a new independent track with a random name and sound.");
  cloneTrack.setTooltip("Copy the selected track's pattern, sound, controls, and plugin settings into a new track.");
  recordToggle.setTooltip("Show or hide sound capture, take management, backing-track import, and final export.");
  addTakeBacking.setTooltip("Add the complete selected take as a stereo backing track, with optional tempo alignment. Existing tracks are muted.");
  recordDone.setTooltip("Return to the instrument.");
  patternBack.setTooltip("Restore the previous generated pattern for the selected track.");
  trackGenerate.setTooltip("Generate a new pattern for only the selected track. Euclidean keeps Hits and Rotate while choosing new pitches; followers use Role and Relationship.");
  trackActions.setTooltip("Rename or remove the selected track.");
  patternForward.setTooltip("Restore the next generated pattern for the selected track.");
  soundPrevious.setTooltip("Try the previous sound in this track's sound family. The notes keep playing.");
  soundNext.setTooltip("Try the next sound in this track's sound family. The notes keep playing.");
  soundRandom.setTooltip("Try a different random sound from this track's sound family.");
  soundBack.setTooltip("Go back to a sound you tried on this track.");
  soundForward.setTooltip("Go forward through this track's tried sounds.");
  tempo.setTooltip("How fast the pattern moves, from 10 to 240 BPM.");
  density.setTooltip("How many steps tend to be active when Free phrases are generated. Euclidean tracks use their own exact Hits count.");
  key.setTooltip("Root note for generated phrases.");
  scale.setTooltip("Note collection used by generated phrases.");
  shape.setTooltip("The contour of new generated notes: wander, rise, fall, arch, still, or leap.");
  mode.setTooltip("Still repeats, Drift makes small changes, Renew makes a new phrase every few cycles.");
  changeEvery.setTooltip("Number of complete 16-step cycles between automatic Drift or Renew changes. Still ignores this setting.");
  sceneName.setTooltip("Name for the current piece.");
  pieceActions.setTooltip("Open a saved Piece, make a new name, or move a saved Piece to Trash.");
  save.setTooltip("Save the current full setup as a piece.");
  recordSound.setTooltip("Capture the microphone as a new reusable sound source.");
  recordMix.setTooltip("Record the full mix, including backing tracks. Starts playback if stopped and saves BPM and starting position.");
  showRecordings.setTooltip("Open this piece's takes folder.");
  useLastSound.setTooltip("Put the last microphone recording onto the selected track.");
  discardSound.setTooltip("Delete the last microphone recording.");
  devices.setTooltip("Choose audio output and input. For audio return, choose a two-channel loopback input and a separate speaker/headphone output.");
  monitorReturn.setTooltip("Monitor the selected stereo input and include it in takes. Use a loopback device; monitoring a microphone can feed back.");
  returnLevel.setTooltip("Level of the external stereo audio mixed into Phrase Mix and recorded takes.");
  midi.setTooltip("Send notes to an external instrument, or configure MIDI input and controller mappings.");
	  takeList.setTooltip("Saved audio takes recorded from the app output.");
	  playTake.setTooltip("Play or stop the selected audio take.");
	  revealTake.setTooltip("Show the selected take in Finder.");
	  takeName.setTooltip("Rename the selected take.");
	  renameTake.setTooltip("Apply the take name.");
	  deleteTake.setTooltip("Delete the selected take audio file after confirmation.");
	  useTakeSound.setTooltip("Copy the selected take into this piece's captured sounds and play it from the selected track.");
	  finalStart.setTooltip("Trim start time for the selected take.");
  finalEnd.setTooltip("Trim end time for the selected take.");
  exportFinal.setTooltip("Export a trimmed, normalized final WAV into this piece's Finals folder.");
  room.setTooltip("Global room size for the space effect.");
  wet.setTooltip("How much of the shared space comes back into the mix.");
  drive.setTooltip("Global warmth and saturation.");
  filter.setTooltip("Overall brightness of the final sound.");
  focusFade.setTooltip("How long the other tracks take to fade down or return when Focus changes.");
  fadeTarget.setTooltip("Level to reach on the selected track, from silence to full level.");
  fadeSeconds.setTooltip("Seconds for the selected track's timed fade. The fade pauses when playback stops.");
  startFade.setTooltip("Fade the selected track to the target level while its pattern keeps playing.");
  cancelFade.setTooltip("Stop the selected track's fade at its current level.");
  for(int i=0;i<Engine::maxTracks;++i){
   trackSelect[i].setTooltip("Select track "+juce::String(i+1)+".");
   holds[i].setTooltip("Keep this track's notes when generating or renewing.");
   mutes[i].setTooltip("Silence this track without deleting it.");
   solos[i].setTooltip("Hear only this track, even when muted. Click again to restore the mix.");
   focuses[i].setTooltip("Fade the other tracks down while their patterns keep evolving. Click again to restore the mix.");
   connects[i].setTooltip("Join consecutive notes without a new attack. Short samples may end before the next note.");
	   kinds[i].setTooltip("Choose a sound family, Plugin for a hosted instrument, or Backing for a complete audio recording.");
	   sounds[i].setTooltip("Choose a sound within the selected family.");
	   playModes[i].setTooltip("Pitch changes speed with notes. Retrigger restarts at original speed. Bed keeps running underneath.");
	   roles[i].setTooltip("Follower behavior: Shadow stays closest to Main, Contrast offsets it, Sparse leaves space, Free wanders farther.");
	   relationships[i].setTooltip("Close to Distant: how strongly this follower is tied to the Main phrase.");
	   dynamics[i].setTooltip("How much note intensity varies without changing the phrase.");
	   articulations[i].setTooltip("Short to Long: how long notes are performed without regenerating the pattern.");
	   tails[i].setTooltip("Lengthens this track's note release so sounds can linger between steps. The pattern stays the same; short samples still end with their recording.");
	   sampleStarts[i].setTooltip("Where the sample begins.");
	   sampleSizes[i].setTooltip("How much of the sample window plays.");
	   sampleFades[i].setTooltip("Soft edge for the sample window.");
	   sampleReverses[i].setTooltip("Play the sample window backward.");
	   lengths[i].setTooltip("Choose how long notes ring.");
	   loopLengths[i].setTooltip("Number of steps this track repeats, from 1 to 64. Use Page to edit steps beyond 16; shortening keeps later notes for when you expand again.");
   levels[i].setTooltip("Track volume.");
   rowLevels[i].setTooltip("Track "+juce::String(i+1)+" level. Moving it cancels this track's timed fade without changing the selected track.");
   sends[i].setTooltip("How much of this track goes into the shared space effect. The whole-piece Space return controls how much comes back.");
   colors[i].setTooltip("Changes this track's brightness and harmonic balance without changing its notes.");
   moves[i].setTooltip("From steady to restless: adds motion to this track's sound without changing its notes.");
   weathers[i].setTooltip("From clean to textured: adds subtle variation and stereo movement without changing the note pattern.");
  }
 }
 void showHelp(){
  if(helpWindow==nullptr)helpWindow=std::make_unique<HelpWindow>();
  else{helpWindow->setVisible(true);helpWindow->toFront(true);}
 }
 void setupSlider(juce::Slider& s,double lo,double hi,double value,std::function<void()> callback){addAndMakeVisible(s);s.setRange(lo,hi,1);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,48,24);s.setValue(value);s.onValueChange=std::move(callback);}
 void label(juce::Graphics& g,const juce::String& text,int x,int y){g.setColour(juce::Colour(0xffa5b2ad));g.setFont(12.f);g.drawText(text,x,y,150,18,juce::Justification::centredLeft);}
};
class App final : public juce::JUCEApplication {
public:
 const juce::String getApplicationName() override{return "Phrase Mix";}
 const juce::String getApplicationVersion() override{return "0.1.0";}
 void initialise(const juce::String&) override {window=std::make_unique<Window>();}
 void shutdown() override {window.reset();}
 void systemRequestedQuit() override {quit();}
private:
 class ContentViewport final : public juce::Viewport {
 public:
  ContentViewport(){setScrollBarsShown(true,false);setScrollOnDragMode(ScrollOnDragMode::never);setScrollBarThickness(12);}
  void resized() override {
   juce::Viewport::resized();
   if(auto* content=getViewedComponent())content->setSize(getMaximumVisibleWidth(),content->getHeight());
  }
 };
 class Window final : public juce::DocumentWindow {
 public:
  Window():DocumentWindow("Phrase Mix",juce::Colour(0xff171b1b),allButtons){
   setUsingNativeTitleBar(true);
   const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
   const int availableHeight=display!=nullptr?display->userArea.getHeight()-80:760;
   auto viewport=std::make_unique<ContentViewport>();
   viewport->setSize(1120,std::clamp(availableHeight,500,760));
   viewport->setViewedComponent(new MainView(),true);
   setContentOwned(viewport.release(),true);
   setResizable(true,false);
   setResizeLimits(1120,500,1900,1200);
   centreWithSize(getWidth(),getHeight());
   setVisible(true);
  }
  void closeButtonPressed() override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
 };
 std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(App)
