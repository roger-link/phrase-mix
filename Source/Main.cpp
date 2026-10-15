#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "HelpView.h"

class MainView final : public juce::AudioAppComponent, private juce::Timer {
public:
 MainView(){
  setSize(1120,760);
  audioFormats.registerBasicFormats();
  addAndMakeVisible(play);play.setButtonText("Discover");play.onClick=[this]{if(activeTrackCount==0)addNewTrack();const bool p=!engine.playing.load();engine.playing=p;play.setButtonText(p?"Stop":"Play");};
 addAndMakeVisible(generate);generate.setButtonText("Generate all patterns");generate.onClick=[this]{generateAllTrackCandidates();};
  addAndMakeVisible(randomize);randomize.setButtonText("Shuffle whole piece");randomize.onClick=[this]{randomizeAll();};
  addAndMakeVisible(addTrack);addTrack.setButtonText("+ Track");addTrack.onClick=[this]{addNewTrack();};
  addAndMakeVisible(recordToggle);recordToggle.setButtonText("Record / Finish");recordToggle.setClickingTogglesState(true);recordToggle.onClick=[this]{setRecordPanelOpen(recordToggle.getToggleState());};
  addAndMakeVisible(recordDone);recordDone.setButtonText("Done");recordDone.onClick=[this]{setRecordPanelOpen(false);};
  addAndMakeVisible(patternBack);patternBack.setButtonText("< Pattern");patternBack.onClick=[this]{restorePatternStep(-1);};
  addAndMakeVisible(trackGenerate);trackGenerate.setButtonText("Generate track");trackGenerate.onClick=[this]{generateSelectedTrackCandidate();};
  addAndMakeVisible(removeTrack);removeTrack.setButtonText("Remove track");removeTrack.onClick=[this]{confirmRemoveTrack(selectedTrack);};
  removeTrack.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff493334));
  removeTrack.setColour(juce::TextButton::textColourOffId,juce::Colour(0xfff0d9d6));
  addAndMakeVisible(patternForward);patternForward.setButtonText("Pattern >");patternForward.onClick=[this]{restorePatternStep(1);};
  addAndMakeVisible(soundPrevious);soundPrevious.setButtonText("<");soundPrevious.onClick=[this]{browseSound(-1);};
  addAndMakeVisible(soundNext);soundNext.setButtonText(">");soundNext.onClick=[this]{browseSound(1);};
  addAndMakeVisible(soundRandom);soundRandom.setButtonText("Random");soundRandom.onClick=[this]{randomSound();};
  addAndMakeVisible(soundBack);soundBack.setButtonText("< Tried");soundBack.onClick=[this]{stepSoundHistory(-1);};
  addAndMakeVisible(soundForward);soundForward.setButtonText("Tried >");soundForward.onClick=[this]{stepSoundHistory(1);};
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
  addAndMakeVisible(useTakeSound);useTakeSound.setButtonText("Turn into sound");useTakeSound.onClick=[this]{useSelectedTakeAsSound();};
  addAndMakeVisible(exportFinal);exportFinal.setButtonText("Export final");exportFinal.onClick=[this]{exportFinalTake();};
  addAndMakeVisible(useLastSound);useLastSound.setButtonText("Use last");useLastSound.onClick=[this]{useLastRecording();};
  addAndMakeVisible(discardSound);discardSound.setButtonText("Discard");discardSound.onClick=[this]{discardLastRecording();};
  addAndMakeVisible(sceneName);sceneName.setText(makeSceneName(),juce::dontSendNotification);sceneName.setSelectAllWhenFocused(true);
  addAndMakeVisible(sceneList);sceneList.onChange=[this]{removeSavedPiece.setEnabled(sceneList.getText().isNotEmpty()&&fileForSceneName(sceneList.getText()).existsAsFile());};
  addAndMakeVisible(newName);newName.setButtonText("New name");newName.onClick=[this]{pieceId=juce::Uuid().toString();sceneName.setText(makeSceneName(),juce::dontSendNotification);};
  addAndMakeVisible(save);save.setButtonText("Save piece");save.onClick=[this]{saveScene();};
  addAndMakeVisible(load);load.setButtonText("Load piece");load.onClick=[this]{loadScene();};
  addAndMakeVisible(removeSavedPiece);removeSavedPiece.setButtonText("Remove");removeSavedPiece.setEnabled(false);removeSavedPiece.onClick=[this]{confirmRemoveSavedPiece();};
  addAndMakeVisible(devices);devices.setButtonText("Audio device");devices.onClick=[this]{
   auto content=std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,0,1,1,2,false,false,true,false);content->setSize(460,350);
   juce::DialogWindow::LaunchOptions options;options.dialogTitle="Audio settings";options.dialogBackgroundColour=juce::Colour(0xff222727);options.content.setOwned(content.release());options.useNativeTitleBar=true;options.resizable=false;options.launchAsync();
  };
  setupSlider(tempo,10,240,96,[this]{engine.bpm=static_cast<float>(tempo.getValue());});
  tempo.setSkewFactorFromMidPoint(70);
  setupSlider(density,20,100,65,[this]{engine.density=static_cast<float>(density.getValue()/100);});
  addAndMakeVisible(key);const char* keys[]={"C","C♯","D","E♭","E","F","F♯","G","A♭","A","B♭","B"};for(int i=0;i<12;++i)key.addItem(keys[i],i+1);key.setSelectedId(3);key.onChange=[this]{engine.key=key.getSelectedId()-1;};
  addAndMakeVisible(scale);scale.addItem("Minor pentatonic",1);scale.addItem("Natural minor",2);scale.addItem("Major",3);scale.setSelectedId(1);scale.onChange=[this]{engine.scale=scale.getSelectedId()-1;};
  addAndMakeVisible(shape);shape.addItem("Wander",1);shape.addItem("Rise",2);shape.addItem("Fall",3);shape.addItem("Arch",4);shape.addItem("Still",5);shape.addItem("Leap",6);shape.setSelectedId(1);shape.onChange=[this]{engine.shape=shape.getSelectedId()-1;};
  addAndMakeVisible(mode);mode.addItem("Still",1);mode.addItem("Drift / 4 cycles",2);mode.addItem("Renew / 4 cycles",3);mode.setSelectedId(1);mode.onChange=[this]{engine.mode=mode.getSelectedId()-1;};
  for(int i=0;i<Engine::maxTracks;++i){
   addAndMakeVisible(trackSelect[i]);trackSelect[i].setClickingTogglesState(false);trackSelect[i].setButtonText(juce::String(i+1));trackSelect[i].onClick=[this,i]{selectTrack(i);};
  }
  for(int i=0;i<Engine::maxTracks;++i){
   addAndMakeVisible(kinds[i]);
   kinds[i].addItem("Made",1);kinds[i].addItem("Instruments",2);kinds[i].addItem("Samples",3);kinds[i].addItem("Strange",4);
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
   auto setupRowToggle=[](juce::TextButton& button,const juce::String& title,juce::Colour active){
    button.setButtonText(title);button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff27302f));
    button.setColour(juce::TextButton::buttonOnColourId,active);
    button.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffc1cbc5));
    button.setColour(juce::TextButton::textColourOnId,juce::Colour(0xfff5f8f5));
   };
   addAndMakeVisible(holds[i]);setupRowToggle(holds[i],"Hold",juce::Colour(0xff3e6851));holds[i].onClick=[this,i]{engine.hold[i]=holds[i].getToggleState();engine.refreshFollowers=true;repaint();};
   addAndMakeVisible(mutes[i]);setupRowToggle(mutes[i],"Mute",juce::Colour(0xff765047));mutes[i].onClick=[this,i]{engine.mute[i]=mutes[i].getToggleState();repaint();};
   addAndMakeVisible(solos[i]);setupRowToggle(solos[i],"Solo",juce::Colour(0xff3f6575));solos[i].onClick=[this,i]{setSoloTrack(solos[i].getToggleState()?i:-1);};
   addAndMakeVisible(connects[i]);connects[i].setButtonText("Connect");connects[i].onClick=[this,i]{engine.connect[i]=connects[i].getToggleState();};
   setupSlider(levels[i],0,100,65,[this,i]{engine.level[i]=static_cast<float>(levels[i].getValue()/100);rowLevels[i].setValue(levels[i].getValue(),juce::dontSendNotification);});
   setupSlider(rowLevels[i],0,100,65,[this,i]{levels[i].setValue(rowLevels[i].getValue());});
   rowLevels[i].setTextBoxStyle(juce::Slider::TextBoxRight,false,38,22);
   setupSlider(sends[i],0,100,i==3?65:25,[this,i]{engine.send[i]=static_cast<float>(sends[i].getValue()/100);});
   setupSlider(colors[i],0,100,i==2?78:55,[this,i]{engine.color[i]=static_cast<float>(colors[i].getValue()/100);});
   setupSlider(moves[i],0,100,i==3?72:35,[this,i]{engine.move[i]=static_cast<float>(moves[i].getValue()/100);});
   setupSlider(weathers[i],0,100,i==3?64:32,[this,i]{engine.weather[i]=static_cast<float>(weathers[i].getValue()/100);});
   setupSlider(relationships[i],0,100,i==0?0:35+(i%3)*12,[this,i]{engine.relationship[i]=static_cast<float>(relationships[i].getValue()/100);});
   setupSlider(dynamics[i],0,100,i==0?18:28+(i%4)*8,[this,i]{engine.dynamics[i]=static_cast<float>(dynamics[i].getValue()/100);});
   setupSlider(articulations[i],0,100,i==0?42:(i==1?68:52),[this,i]{engine.articulation[i]=static_cast<float>(articulations[i].getValue()/100);});
   setupSlider(tails[i],0,100,0,[this,i]{engine.tail[i]=static_cast<float>(tails[i].getValue()/100);});
   refreshSoundMenu(i,false);
  }
  setupSlider(room,0,100,60,[this]{engine.room=static_cast<float>(room.getValue()/100);});
  setupSlider(wet,0,100,25,[this]{engine.wet=static_cast<float>(wet.getValue()/100);});
  setupSlider(drive,0,100,18,[this]{engine.drive=static_cast<float>(drive.getValue()/100);});
  setupSlider(filter,0,100,72,[this]{engine.filter=static_cast<float>(filter.getValue()/100);});
  setupSlider(finalStart,0,1,0,[this]{clampFinalTrim(true);});
  setupSlider(finalEnd,0,1,1,[this]{clampFinalTrim(false);});
  setupHelp();
  migrateSingleSceneIfNeeded();
  refreshSceneList();
  loadSampleLibrary();
  refreshTakeList(false);
  resized();
  setAudioChannels(1,2);
  deviceManager.addAudioCallback(&captureTap);
  startTimerHz(30);
 }
 ~MainView() override {stopTimer();deviceManager.removeAudioCallback(&captureTap);shutdownAudio();}
 void prepareToPlay(int,double rate) override {engine.prepare(rate);captureSampleRate.store(rate);}
 void releaseResources() override {}
 void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
  if(!info.buffer||info.buffer->getNumChannels()==0)return;
 engine.render(info.buffer->getWritePointer(0,info.startSample),info.buffer->getNumChannels()>1?info.buffer->getWritePointer(1,info.startSample):nullptr,info.numSamples);
  renderTakePlayback(info);
  for(int c=2;c<info.buffer->getNumChannels();++c)info.buffer->clear(c,info.startSample,info.numSamples);
  if(recordingMix.load()){
   const juce::ScopedLock lock(mixRecordLock);
   const float* left=info.buffer->getReadPointer(0,info.startSample);
   const float* right=info.buffer->getNumChannels()>1?info.buffer->getReadPointer(1,info.startSample):left;
   mixRecordBuffer.reserve(mixRecordBuffer.size()+static_cast<size_t>(info.numSamples*2));
   for(int i=0;i<info.numSamples;++i){mixRecordBuffer.push_back(left[i]);mixRecordBuffer.push_back(right[i]);}
  }
 }
 void paint(juce::Graphics& g) override {
  g.fillAll(juce::Colour(0xff171b1b));g.setColour(juce::Colour(0xffe9eeeb));g.setFont(20.f);g.drawText("PHRASE / MIX",24,14,300,30,juce::Justification::centredLeft);
  label(g,"BPM",145,62);label(g,"Density",390,62);label(g,"Key",24,112);label(g,"Scale",110,112);label(g,"Shape",310,112);label(g,"Behavior",475,112);label(g,"Piece name",660,112);label(g,"Saved pieces",660,171);
  if(status.isNotEmpty()){g.setColour((recordingSound.load()||recordingMix.load())?juce::Colour(0xffff8f7a):juce::Colour(0xffb6dfc5));g.setFont(13.f);g.drawText(status,290,195,350,24,juce::Justification::centredLeft);}
  if(recordPanelOpen){
   const int captureY=recordPanelY();
   g.setColour(juce::Colour(0xff222727));g.fillRoundedRectangle(24.f,static_cast<float>(captureY),static_cast<float>(getWidth()-48),162.f,6.f);
   label(g,"Sound source",40,captureY+2);label(g,"Takes",40,captureY+43);label(g,"Manage",40,captureY+83);label(g,"Finish",40,captureY+123);
   label(g,"Start",110,captureY+123);label(g,"End",300,captureY+123);
   g.setColour(juce::Colour(0xff121616));g.fillRoundedRectangle(475.f,static_cast<float>(captureY+16),90.f,8.f,2.f);
   g.setColour(recordingSound.load()?juce::Colour(0xffff8f7a):juce::Colour(0xff94bba4));g.fillRoundedRectangle(475.f,static_cast<float>(captureY+16),90.f*std::clamp(captureMeterLevel,0.0f,1.0f),8.f,2.f);
  }
  const int noteWidth=std::max(10,(getWidth()-600)/16);
  if(activeTrackCount==0){g.setColour(juce::Colour(0xffa5b2ad));g.setFont(16.f);g.drawText("No tracks yet",24,sequenceTop,240,28,juce::Justification::centredLeft);}
  for(int c=0;c<activeTrackCount;++c){int y=sequenceTop+c*sequenceRowHeight;g.setColour(juce::Colour(engine.mute[c].load()&&engine.soloTrack.load()!=c?0xff1b2121:(c==engine.soloTrack.load()?0xff293d45:(c==selectedTrack?0xff26352f:0xff1e2423))));g.fillRoundedRectangle(16.f,static_cast<float>(y-1),static_cast<float>(getWidth()-32),32.f,6.f);
   if(c==selectedTrack){g.setColour(juce::Colour(0xff8fbda3));g.fillRect(16,y+3,3,24);}
   g.setColour(juce::Colour(0xff43514a));g.fillRect(getWidth()-238,y+3,1,26);
   for(int j=0;j<Engine::steps;++j){int x=190+j*noteWidth;g.setColour(juce::Colour(j%4==0?0xff303b35:0xff222727));g.fillRoundedRectangle(static_cast<float>(x),static_cast<float>(y),static_cast<float>(noteWidth-3),29.f,3.f);
    const int n=engine.display[c*Engine::steps+j].load();if(n>=0){g.setColour(juce::Colour(engine.mute[c].load()&&engine.soloTrack.load()!=c?0xff54645a:0xff94bba4));g.fillRoundedRectangle(static_cast<float>(x+2),static_cast<float>(y+23-std::clamp(n-24,0,60)*0.28),static_cast<float>(noteWidth-7),6.f,2.f);}
    if(j==engine.playhead.load()){g.setColour(juce::Colour(0xffb6dfc5));g.drawRoundedRectangle(static_cast<float>(x),static_cast<float>(y),static_cast<float>(noteWidth-3),29.f,3.f,1.5f);}
   }
  }
  if(activeTrackCount==0)return;
  const int panelY=soundPanelY();
	  g.setColour(juce::Colour(0xff222727));g.fillRoundedRectangle(24.f,static_cast<float>(panelY),static_cast<float>(getWidth()-48),230.f,8.f);
  g.setColour(juce::Colour(0xffe9eeeb));g.setFont(16.f);g.drawText(juce::String(selectedTrack+1)+". "+trackNames[selectedTrack],48,panelY+11,180,24,juce::Justification::centredLeft);
	  label(g,"Kind",48,panelY+43);label(g,"Sound",228,panelY+43);label(g,"Space send",508,panelY+43);label(g,"Tone",648,panelY+43);label(g,"Motion",788,panelY+43);label(g,"Texture",928,panelY+43);
	  g.setColour(juce::Colour(0xff3b4943));g.fillRect(40,panelY+98,getWidth()-80,1);
	  if(selectedTrack>0){label(g,"Role",48,panelY+104);label(g,"Relationship",208,panelY+104);}
	  label(g,"Dynamics",368,panelY+104);label(g,"Articulation",508,panelY+104);label(g,"Length",648,panelY+104);label(g,"Note tail",788,panelY+104);if(kinds[selectedTrack].getSelectedId()==3)label(g,"Play",928,panelY+104);
	  g.setColour(juce::Colour(0xff3b4943));g.fillRect(40,panelY+158,getWidth()-80,1);
	  g.setColour(juce::Colour(0xff94bba4));g.setFont(11.f);g.drawText("WHOLE PIECE",48,panelY+160,160,14,juce::Justification::centredLeft);
	  label(g,"Space return",48,panelY+177);label(g,"Warmth",208,panelY+177);label(g,"Brightness",368,panelY+177);label(g,"Room size",528,panelY+177);
	  if(kinds[selectedTrack].getSelectedId()==3){g.setColour(juce::Colour(0xff94bba4));g.setFont(11.f);g.drawText("SAMPLE SHAPE",668,panelY+160,160,14,juce::Justification::centredLeft);label(g,"Start",668,panelY+177);label(g,"Size",788,panelY+177);label(g,"Fade",908,panelY+177);}
  }
 void resized() override {
  keep.setBounds(getWidth()-500,18,100,24);keeps.setBounds(getWidth()-390,18,90,24);help.setBounds(getWidth()-280,18,70,24);devices.setBounds(getWidth()-145,18,121,24);play.setBounds(24,76,100,32);tempo.setBounds(145,76,220,32);density.setBounds(390,76,240,32);randomize.setBounds(getWidth()-390,76,176,32);generate.setBounds(getWidth()-200,76,176,32);
  key.setBounds(24,135,70,32);scale.setBounds(110,135,185,32);shape.setBounds(310,135,150,32);mode.setBounds(475,135,170,32);
  sceneName.setBounds(660,135,185,32);newName.setBounds(855,135,100,32);save.setBounds(965,135,130,32);
  sceneList.setBounds(660,194,185,32);load.setBounds(855,194,140,32);removeSavedPiece.setBounds(1005,194,90,32);
  addTrack.setBounds(24,188,76,30);recordToggle.setBounds(110,188,170,30);
  const int captureY=recordPanelY();
  recordSound.setBounds(110,captureY+10,124,24);useLastSound.setBounds(244,captureY+10,88,24);discardSound.setBounds(342,captureY+10,82,24);
  recordMix.setBounds(110,captureY+50,104,24);showRecordings.setBounds(224,captureY+50,104,24);takeList.setBounds(338,captureY+50,220,24);playTake.setBounds(568,captureY+50,90,24);revealTake.setBounds(668,captureY+50,80,24);
  takeName.setBounds(110,captureY+90,260,24);renameTake.setBounds(380,captureY+90,80,24);deleteTake.setBounds(470,captureY+90,74,24);useTakeSound.setBounds(554,captureY+90,132,24);
  finalStart.setBounds(150,captureY+130,130,24);finalEnd.setBounds(330,captureY+130,130,24);exportFinal.setBounds(480,captureY+130,120,24);
  recordDone.setBounds(getWidth()-145,captureY+10,121,24);
  std::array<juce::Component*,16> recordControls{&recordSound,&useLastSound,&discardSound,&recordMix,&showRecordings,&takeList,&playTake,&revealTake,&takeName,&renameTake,&deleteTake,&useTakeSound,&finalStart,&finalEnd,&exportFinal,&recordDone};
  for(auto* control:recordControls)control->setVisible(recordPanelOpen);
  for(int c=0;c<Engine::maxTracks;++c){
   const bool active=c<activeTrackCount;
   trackSelect[c].setVisible(active);trackSelect[c].setBounds(24,sequenceTop+1+c*sequenceRowHeight,150,27);
   updateTrackSelectLabel(c);
   rowLevels[c].setVisible(active);rowLevels[c].setBounds(getWidth()-380,sequenceTop+1+c*sequenceRowHeight,140,28);
   holds[c].setVisible(active);mutes[c].setVisible(active);solos[c].setVisible(active);holds[c].setBounds(getWidth()-220,sequenceTop+2+c*sequenceRowHeight,60,26);mutes[c].setBounds(getWidth()-150,sequenceTop+2+c*sequenceRowHeight,60,26);solos[c].setBounds(getWidth()-80,sequenceTop+2+c*sequenceRowHeight,60,26);
   trackSelect[c].toFront(false);rowLevels[c].toFront(false);holds[c].toFront(false);mutes[c].toFront(false);solos[c].toFront(false);
  }
	  for(int c=0;c<Engine::maxTracks;++c){const bool selected=c==selectedTrack;const bool followerSelected=selected&&c>0;const bool sampleSelected=selected&&kinds[c].getSelectedId()==3;kinds[c].setVisible(selected);sounds[c].setVisible(selected);sources[c].setVisible(false);materials[c].setVisible(false);playModes[c].setVisible(sampleSelected);sampleStarts[c].setVisible(sampleSelected);sampleSizes[c].setVisible(sampleSelected);sampleFades[c].setVisible(sampleSelected);sampleReverses[c].setVisible(sampleSelected);librarySounds[c].setVisible(false);instruments[c].setVisible(false);roles[c].setVisible(followerSelected);lengths[c].setVisible(selected);levels[c].setVisible(false);sends[c].setVisible(selected);colors[c].setVisible(selected);moves[c].setVisible(selected);weathers[c].setVisible(selected);relationships[c].setVisible(followerSelected);dynamics[c].setVisible(selected);articulations[c].setVisible(selected);tails[c].setVisible(selected);connects[c].setVisible(selected);}
  if(activeTrackCount==0||selectedTrack<0||selectedTrack>=activeTrackCount){patternBack.setVisible(false);trackGenerate.setVisible(false);patternForward.setVisible(false);removeTrack.setVisible(false);soundPrevious.setVisible(false);soundNext.setVisible(false);soundRandom.setVisible(false);soundBack.setVisible(false);soundForward.setVisible(false);room.setVisible(false);wet.setVisible(false);drive.setVisible(false);filter.setVisible(false);return;}
	  const int panelY=soundPanelY();
	  patternBack.setVisible(true);trackGenerate.setVisible(true);patternForward.setVisible(true);removeTrack.setVisible(true);patternBack.setBounds(240,panelY+12,92,24);trackGenerate.setBounds(342,panelY+12,126,24);patternForward.setBounds(478,panelY+12,92,24);removeTrack.setBounds(780,panelY+12,142,24);connects[selectedTrack].setBounds(getWidth()-168,panelY+12,126,24);
	  soundPrevious.setVisible(true);soundNext.setVisible(true);soundRandom.setVisible(true);soundBack.setVisible(true);soundForward.setVisible(true);
	  soundBack.setBounds(590,panelY+12,80,24);soundForward.setBounds(680,panelY+12,80,24);
	  kinds[selectedTrack].setBounds(48,panelY+62,140,26);soundPrevious.setBounds(197,panelY+62,28,26);sounds[selectedTrack].setBounds(230,panelY+62,155,26);soundNext.setBounds(390,panelY+62,28,26);soundRandom.setBounds(425,panelY+62,75,26);
	  sends[selectedTrack].setBounds(508,panelY+60,120,28);colors[selectedTrack].setBounds(648,panelY+60,120,28);moves[selectedTrack].setBounds(788,panelY+60,120,28);weathers[selectedTrack].setBounds(928,panelY+60,140,28);
	  if(selectedTrack>0){roles[selectedTrack].setBounds(48,panelY+123,140,26);relationships[selectedTrack].setBounds(208,panelY+121,140,28);}
	  dynamics[selectedTrack].setBounds(368,panelY+121,120,28);articulations[selectedTrack].setBounds(508,panelY+121,120,28);lengths[selectedTrack].setBounds(648,panelY+123,120,26);tails[selectedTrack].setBounds(788,panelY+121,120,28);playModes[selectedTrack].setBounds(928,panelY+123,120,26);
	  room.setVisible(true);wet.setVisible(true);drive.setVisible(true);filter.setVisible(true);wet.setBounds(48,panelY+195,120,28);drive.setBounds(208,panelY+195,120,28);filter.setBounds(368,panelY+195,120,28);room.setBounds(528,panelY+195,120,28);
	  sampleStarts[selectedTrack].setBounds(668,panelY+195,108,28);sampleSizes[selectedTrack].setBounds(788,panelY+195,108,28);sampleFades[selectedTrack].setBounds(908,panelY+195,108,28);sampleReverses[selectedTrack].setBounds(1026,panelY+198,70,22);
 }
 void mouseDown(const juce::MouseEvent& e) override {
   const int track=trackAt(e.position);
  const int step=stepAt(e.position);
  if(track>=0){
   selectTrack(track);
   if(step>=0){
    engine.toggleStep(track,step);
    rememberPattern(track,true);
   }
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
   return std::min(writeIndex.load(),maxSamples);
  }
  bool isActive() const {return active.load();}
  int recordedSamples() const {return std::min(writeIndex.load(),maxSamples);}
  float peak() {return inputPeak.exchange(0.0f);}
  void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,int numInputChannels,float* const* outputChannelData,int numOutputChannels,int numSamples,const juce::AudioIODeviceCallbackContext&) override {
   for(int channel=0;channel<numOutputChannels;++channel)if(outputChannelData[channel]!=nullptr)juce::FloatVectorOperations::clear(outputChannelData[channel],numSamples);
   if(!active.load()||buffer==nullptr||numInputChannels<=0||inputChannelData[0]==nullptr)return;
   int index=writeIndex.load();
   if(index>=maxSamples){active.store(false);return;}
   const int writable=std::min(numSamples,maxSamples-index);
   auto& target=*buffer;
   float blockPeak=0.0f;
   for(int i=0;i<writable;++i){const float value=inputChannelData[0][i];target[static_cast<size_t>(index+i)]=value;blockPeak=std::max(blockPeak,std::abs(value));}
   inputPeak.store(std::max(inputPeak.load(),blockPeak));
   writeIndex.store(index+writable);
   if(index+writable>=maxSamples)active.store(false);
  }
  void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
  void audioDeviceStopped() override {active.store(false);}
 private:
  std::atomic<bool> active{false};
  std::atomic<int> writeIndex{0};
  std::atomic<float> inputPeak{0.0f};
  std::vector<float>* buffer=nullptr;
  int maxSamples=0;
 };
 juce::TextButton play,generate,randomize,addTrack,recordToggle,recordDone,patternBack,trackGenerate,patternForward,removeTrack,soundPrevious,soundNext,soundRandom,soundBack,soundForward,help,keep,keeps,recordSound,recordMix,showRecordings,playTake,revealTake,renameTake,deleteTake,useTakeSound,exportFinal,useLastSound,discardSound,newName,save,load,removeSavedPiece,devices;
 std::array<juce::TextButton,Engine::maxTracks> trackSelect;
 juce::TextEditor sceneName,takeName;
 juce::String pieceId=juce::Uuid().toString();
 juce::Slider tempo,density,room,wet,drive,filter,finalStart,finalEnd;
 std::array<juce::Slider,Engine::maxTracks> rowLevels;
 juce::ComboBox key,scale,shape,mode,sceneList,takeList;
 int selectedTrack=-1;
 int activeTrackCount=0;
 bool recordPanelOpen=false;
 std::array<juce::String,Engine::maxTracks> trackNames{};
 std::array<juce::String,Engine::maxTracks> trackSubtitles{};
 std::array<std::vector<std::array<int,Engine::steps>>,Engine::maxTracks> patternHistory{};
 std::array<int,Engine::maxTracks> patternHistoryIndex{};
 struct SoundChoice {int kind=1,sound=1;bool operator==(const SoundChoice& other) const {return kind==other.kind&&sound==other.sound;}};
 std::array<std::vector<SoundChoice>,Engine::maxTracks> soundHistory{};
 std::array<int,Engine::maxTracks> soundHistoryIndex{};
 struct PendingSoundChange {bool active=false;juce::uint32 due=0;};
 std::array<PendingSoundChange,Engine::maxTracks> pendingSoundChanges{};
 static constexpr int sequenceTop=230;
 static constexpr int sequenceRowHeight=34;
 int soundPanelY() const {return sequenceTop+Engine::maxTracks*sequenceRowHeight+14;}
 int recordPanelY() const {return soundPanelY()+242;}
 int contentHeight() const {return recordPanelOpen?recordPanelY()+180:std::max(760,soundPanelY()+242);}
 void setRecordPanelOpen(bool open){
  recordPanelOpen=open;
  recordToggle.setToggleState(open,juce::dontSendNotification);
  recordToggle.setButtonText(open?"Hide Record / Finish":"Record / Finish");
  setSize(getWidth(),contentHeight());
  resized();
  if(auto* viewport=findParentComponentOfClass<juce::Viewport>())viewport->setViewPosition(0,open?std::max(0,recordPanelY()+180-viewport->getViewHeight()):0);
  repaint();
 }
 void updateTrackSelectLabel(int lane){
  if(lane<0||lane>=Engine::maxTracks)return;
  if(lane>=activeTrackCount){trackSelect[lane].setButtonText(juce::String());return;}
  const auto sound=sounds[lane].getText();
  trackSelect[lane].setButtonText(juce::String(lane+1)+". "+sound);
  trackSelect[lane].setTooltip("Select track "+juce::String(lane+1)+": "+trackNames[lane]+". Sound: "+sound+".");
 }
 void selectTrack(int index){
  selectedTrack=activeTrackCount>0?std::clamp(index,0,activeTrackCount-1):-1;
  for(int i=0;i<Engine::maxTracks;++i){
   const bool selected=i==selectedTrack;
   trackSelect[i].setToggleState(selected,juce::dontSendNotification);
   trackSelect[i].setColour(juce::TextButton::buttonColourId,juce::Colour(selected?0xff40584e:0xff222727));
   trackSelect[i].setColour(juce::TextButton::textColourOffId,juce::Colour(selected?0xffe9eeeb:0xffa5b2ad));
  }
  resized();repaint();
 }
 void setSoloTrack(int index){
  const int solo=index>=0&&index<activeTrackCount?index:-1;
  for(int i=0;i<Engine::maxTracks;++i)solos[i].setToggleState(i==solo,juce::dontSendNotification);
  engine.soloTrack=solo;
  repaint();
 }
 void addNewTrack(){
  if(activeTrackCount>=Engine::maxTracks)return;
  const int lane=activeTrackCount;
  setTrackCount(activeTrackCount+1);
  selectTrack(lane);
  if(auto* viewport=findParentComponentOfClass<juce::Viewport>()){
   const int panelBottom=soundPanelY()+242;
   if(panelBottom>viewport->getViewPositionY()+viewport->getViewHeight())
    viewport->setViewPosition(0,panelBottom-viewport->getViewHeight());
  }
 }
 void setTrackCount(int count){
  const int previousCount=activeTrackCount;
  activeTrackCount=std::clamp(count,0,Engine::maxTracks);
  if(engine.soloTrack.load()>=activeTrackCount)setSoloTrack(-1);
  setSize(getWidth(),contentHeight());
  engine.activeTracks=activeTrackCount;
  if(activeTrackCount==0)engine.playing=false;
  play.setButtonText(activeTrackCount==0?"Discover":engine.playing.load()?"Stop":"Play");
  if(activeTrackCount==0)selectedTrack=-1;
  else if(selectedTrack<0||selectedTrack>=activeTrackCount)selectedTrack=activeTrackCount-1;
  for(int i=previousCount;i<activeTrackCount;++i){
   if(trackNames[i].trim().isEmpty())trackNames[i]=makeTrackName();
   if(trackSubtitles[i].trim().isEmpty())trackSubtitles[i]=i==0?"Main phrase":roleSubtitle(roles[i].getSelectedId()-1);
   applyNewTrackDefaults(i);
   soundHistory[i]={{kinds[i].getSelectedId(),sounds[i].getSelectedId()}};
   soundHistoryIndex[i]=0;
   engine.generateTrack(i);
   rememberPattern(i,true);
  }
  for(int i=activeTrackCount;i<Engine::maxTracks;++i){
   pendingSoundChanges[i].active=false;
   engine.browseGain[i]=1.0f;
   soundHistory[i].clear();
   soundHistoryIndex[i]=0;
   engine.clearTrack(i);
   holds[i].setToggleState(false,juce::sendNotification);
   mutes[i].setToggleState(false,juce::sendNotification);
   solos[i].setToggleState(false,juce::dontSendNotification);
  }
  if(activeTrackCount==0)selectTrack(-1);
  else if(selectedTrack<0||selectedTrack>=activeTrackCount)selectTrack(activeTrackCount-1);
  resized();repaint();
 }
 void copyTrackSettings(int from,int to){
	  setLaneSoundWithoutRole(to,kinds[from].getSelectedId(),sounds[from].getSelectedId());
	  materials[to].setSelectedId(materials[from].getSelectedId(),juce::dontSendNotification);engine.material[to]=materials[to].getSelectedId()-1;
	  playModes[to].setSelectedId(playModes[from].getSelectedId(),juce::dontSendNotification);engine.samplePlayMode[to]=playModes[to].getSelectedId()-1;
	  sampleStarts[to].setValue(sampleStarts[from].getValue());sampleSizes[to].setValue(sampleSizes[from].getValue());sampleFades[to].setValue(sampleFades[from].getValue());
	  sampleReverses[to].setToggleState(sampleReverses[from].getToggleState(),juce::sendNotification);
	  roles[to].setSelectedId(roles[from].getSelectedId());relationships[to].setValue(relationships[from].getValue());dynamics[to].setValue(dynamics[from].getValue());articulations[to].setValue(articulations[from].getValue());tails[to].setValue(tails[from].getValue());
	  lengths[to].setSelectedId(lengths[from].getSelectedId());
  connects[to].setToggleState(connects[from].getToggleState(),juce::sendNotification);
  levels[to].setValue(levels[from].getValue());sends[to].setValue(sends[from].getValue());colors[to].setValue(colors[from].getValue());moves[to].setValue(moves[from].getValue());weathers[to].setValue(weathers[from].getValue());
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
 void removeTrackAt(int lane){
  if(lane<0||lane>=activeTrackCount)return;
  for(int i=0;i<activeTrackCount;++i){pendingSoundChanges[i].active=false;engine.browseGain[i]=1.0f;}
  const int solo=engine.soloTrack.load();
  if(solo>=0)setSoloTrack(solo==lane?-1:(solo>lane?solo-1:solo));
  for(int i=lane;i<activeTrackCount-1;++i){
   copyTrackSettings(i+1,i);
   engine.copyTrackData(i+1,i);
   patternHistory[i]=patternHistory[i+1];
   patternHistoryIndex[i]=patternHistoryIndex[i+1];
   soundHistory[i]=soundHistory[i+1];
   soundHistoryIndex[i]=soundHistoryIndex[i+1];
   trackNames[i]=trackNames[i+1];
   trackSubtitles[i]=trackSubtitles[i+1];
  }
  const int last=activeTrackCount-1;
  trackNames[last].clear();
  trackSubtitles[last].clear();
  engine.clearTrack(last);
  patternHistory[last].clear();
  patternHistoryIndex[last]=0;
  soundHistory[last].clear();
  soundHistoryIndex[last]=0;
  activeTrackCount=std::max(0,activeTrackCount-1);
  engine.activeTracks=activeTrackCount;
  if(activeTrackCount==0)engine.playing=false;
  play.setButtonText(activeTrackCount==0?"Discover":engine.playing.load()?"Stop":"Play");
  if(activeTrackCount==0)selectTrack(-1);
  else if(selectedTrack>=activeTrackCount)selectTrack(activeTrackCount-1);
  else selectTrack(std::min(lane,activeTrackCount-1));
  resized();repaint();
 }
 void rememberPattern(int lane,bool discardForward){
  if(lane<0||lane>=Engine::maxTracks)return;
  auto phrase=engine.getTrackPhrase(lane);
  auto& history=patternHistory[lane];
  int& index=patternHistoryIndex[lane];
  if(discardForward&&index+1<static_cast<int>(history.size()))history.erase(history.begin()+index+1,history.end());
  if(!history.empty()&&history.back()==phrase){index=static_cast<int>(history.size())-1;return;}
  history.push_back(phrase);
  constexpr int maxHistory=24;
  if(static_cast<int>(history.size())>maxHistory)history.erase(history.begin());
  index=static_cast<int>(history.size())-1;
 }
 void restorePatternStep(int direction){
  if(selectedTrack<0||selectedTrack>=activeTrackCount)return;
  auto& history=patternHistory[selectedTrack];
  if(history.empty()){rememberPattern(selectedTrack,false);return;}
  int& index=patternHistoryIndex[selectedTrack];
  index=std::clamp(index+direction,0,static_cast<int>(history.size())-1);
  engine.setTrackPhrase(selectedTrack,history[static_cast<size_t>(index)]);
  status="Restored pattern "+juce::String(index+1)+" / "+juce::String(static_cast<int>(history.size()))+" on track "+juce::String(selectedTrack+1);
  repaint();
 }
 void generateSelectedTrackCandidate(){
  if(selectedTrack<0||selectedTrack>=activeTrackCount)return;
  rememberPattern(selectedTrack,true);
  engine.generateTrack(selectedTrack);
  rememberPattern(selectedTrack,true);
  status="Generated pattern "+juce::String(patternHistoryIndex[selectedTrack]+1)+" / "+juce::String(static_cast<int>(patternHistory[selectedTrack].size()))+" on track "+juce::String(selectedTrack+1);
  repaint();
 }
 void generateAllTrackCandidates(){
  for(int i=0;i<activeTrackCount;++i){
   if(holds[i].getToggleState())continue;
   rememberPattern(i,true);
   engine.generateTrack(i);
   rememberPattern(i,true);
  }
  status="Generated patterns for unheld tracks";
  repaint();
 }
 int trackAt(juce::Point<float> p) const {const int t=static_cast<int>((p.y-sequenceTop)/sequenceRowHeight);return p.y>=sequenceTop&&t>=0&&t<activeTrackCount&&p.y<sequenceTop+t*sequenceRowHeight+29?t:-1;}
 int stepAt(juce::Point<float> p) const {const int noteWidth=std::max(10,(getWidth()-600)/Engine::steps);const int s=static_cast<int>((p.x-190)/noteWidth);return p.x>=190&&s>=0&&s<Engine::steps&&p.x<190+s*noteWidth+noteWidth-3?s:-1;}
 std::array<juce::TextButton,Engine::maxTracks> holds,mutes,solos;
 std::array<juce::ToggleButton,Engine::maxTracks> connects,sampleReverses;
 std::array<juce::ComboBox,Engine::maxTracks> kinds,sounds,sources,materials,playModes,roles,lengths,librarySounds,instruments;
 std::array<juce::Slider,Engine::maxTracks> levels,sends,colors,moves,weathers,relationships,dynamics,articulations,tails,sampleStarts,sampleSizes,sampleFades;
 juce::String status;
 juce::AudioFormatManager audioFormats;
 juce::StringArray sampleDisplayNames,sampleGroups;
 CaptureTap captureTap;
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
 juce::CriticalSection mixRecordLock;
 std::vector<float> mixRecordBuffer;
 std::atomic<bool> recordingMix{false};
 double mixRecordSampleRate=48000.0;
 int lastRecordingSoundId=0;
 float captureMeterLevel=0.0f;
 std::atomic<bool> recordingSound{false};
 std::atomic<double> captureSampleRate{48000.0};
 void timerCallback() override {
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
   size_t samples=0;
   {const juce::ScopedLock lock(mixRecordLock);samples=mixRecordBuffer.size();}
   const double seconds=static_cast<double>(samples/2)/std::max(1.0,mixRecordSampleRate);
   status="Recording take "+timeText(seconds);
  }
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
 void applyNewTrackDefaults(int lane){
  holds[lane].setToggleState(false,juce::sendNotification);
  mutes[lane].setToggleState(false,juce::sendNotification);
  solos[lane].setToggleState(false,juce::dontSendNotification);
  connects[lane].setToggleState(false,juce::sendNotification);
  juce::Random r;
  static constexpr int presets[][2]={{2,6},{2,3},{2,1},{4,2},{4,1},{1,4},{1,8},{1,3},{1,6}};
  const auto& preset=presets[r.nextInt(static_cast<int>(sizeof(presets)/sizeof(presets[0])))];
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
   default:
    sounds[lane].addItem("Pluck",1);sounds[lane].addItem("Low",2);sounds[lane].addItem("Ring",3);sounds[lane].addItem("Cloud",4);
    sounds[lane].addItem("Glow",5);sounds[lane].addItem("Reed",6);sounds[lane].addItem("Dust",7);sounds[lane].addItem("Bell",8);
    sounds[lane].addItem("Guitar",10);sounds[lane].addItem("Violin",11);sounds[lane].addItem("Bass",12);sounds[lane].addItem("Keys",13);
    break;
  }
  sounds[lane].setSelectedId(previous>0?previous:1,juce::dontSendNotification);
  if(sounds[lane].getSelectedId()==0)sounds[lane].setSelectedId(1,juce::dontSendNotification);
  if(updateEngine)applySoundChoice(lane);
  resized();repaint();
 }
 void applySoundChoice(int lane){
  const int kind=kinds[lane].getSelectedId();
  const int sound=sounds[lane].getSelectedId();
  if(kind==2){
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
  updateTrackSelectLabel(lane);
 }
 bool validSoundChoice(SoundChoice choice) const {
  if(choice.kind==1)return (choice.sound>=1&&choice.sound<=8)||(choice.sound>=10&&choice.sound<=13);
  if(choice.kind==2)return choice.sound>=1&&choice.sound<=7;
  if(choice.kind==3)return choice.sound>=1&&choice.sound<=sampleDisplayNames.size()+1;
  if(choice.kind==4)return choice.sound>=1&&choice.sound<=4;
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
  for(int i=0;i<activeTrackCount;++i){
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
 void renderTakePlayback(const juce::AudioSourceChannelInfo& info){
  if(!takePlaying.load())return;
  const juce::ScopedLock lock(takePlaybackLock);
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
	  refreshTakeList(false);
	  selectTakeNamed(target.getFileNameWithoutExtension());
	  status=ok?"Renamed take: "+target.getFileNameWithoutExtension():"Could not rename take";
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
  const auto out=finalDirectory().getChildFile(juce::File::createLegalFileName(base)).withFileExtension(".wav");
  out.getParentDirectory().createDirectory();
  juce::WavAudioFormat format;
  std::unique_ptr<juce::FileOutputStream> stream(out.createOutputStream());
  if(stream==nullptr||!stream->openedOk()){status="Could not save final";repaint();return;}
  std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,2,24,{},0));
  if(writer==nullptr){status="Could not write final";repaint();return;}
  stream.release();
  if(!writer->writeFromAudioSampleBuffer(buffer,0,frames)){status="Could not write final";repaint();return;}
  out.revealToUser();
  status="Final exported: "+out.getFileNameWithoutExtension();
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
  if(recordingSound.load()){status="Stop sound recording first";repaint();return;}
  if(recordingMix.load()){
   recordingMix.store(false);
   recordMix.setButtonText("Record take");
   finishMixRecording();
   return;
  }
  const juce::ScopedLock lock(mixRecordLock);
  mixRecordSampleRate=std::max(8000.0,captureSampleRate.load());
  mixRecordBuffer.clear();
  mixRecordBuffer.reserve(static_cast<size_t>(mixRecordSampleRate*2.0*30.0));
  recordingMix.store(true);
  recordMix.setButtonText("Stop take");
  status="Recording take 0:00";
  repaint();
 }
 void finishMixRecording(){
  std::vector<float> data;
  {
   const juce::ScopedLock lock(mixRecordLock);
   data=mixRecordBuffer;
   mixRecordBuffer.clear();
  }
  const int frames=static_cast<int>(data.size()/2);
  if(frames<static_cast<int>(mixRecordSampleRate*0.25)){status="Take too short";repaint();return;}
  juce::AudioBuffer<float> buffer(2,frames);
  for(int i=0;i<frames;++i){buffer.setSample(0,i,data[static_cast<size_t>(i*2)]);buffer.setSample(1,i,data[static_cast<size_t>(i*2+1)]);}
  const auto file=mixRecordingDirectory().getChildFile(juce::File::createLegalFileName(makeMixName())).withFileExtension(".wav");
  file.getParentDirectory().createDirectory();
  juce::WavAudioFormat format;
  std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
  if(stream==nullptr||!stream->openedOk()){status="Could not save take";repaint();return;}
  std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),mixRecordSampleRate,2,24,{},0));
  if(writer==nullptr){status="Could not write take";repaint();return;}
  stream.release();
  if(!writer->writeFromAudioSampleBuffer(buffer,0,frames)){status="Could not write take";repaint();return;}
	  lastMixFile=file;
	  refreshTakeList(false);
	  selectTakeNamed(file.getFileNameWithoutExtension());
		  status="Saved take: "+file.getFileNameWithoutExtension()+" · Turn into sound to reuse it";
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
  return sceneDirectory().getChildFile(safe).withFileExtension(".json");
 }
 void refreshSceneList(){
  const auto current=sceneList.getText();
  sceneList.clear(juce::dontSendNotification);
  juce::Array<juce::File> files;sceneDirectory().findChildFiles(files,juce::File::findFiles,false,"*.json");
  files.sort();
  int id=1;for(auto& f:files)sceneList.addItem(f.getFileNameWithoutExtension(),id++);
  if(current.isNotEmpty())sceneList.setText(current,juce::dontSendNotification);
  removeSavedPiece.setEnabled(current.isNotEmpty()&&fileForSceneName(current).existsAsFile());
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
 void confirmRemoveSavedPiece(){
  const auto selected=sceneList.getText().trim();
  if(selected.isEmpty()){status="Select a saved piece to remove";repaint();return;}
  const auto file=fileForSceneName(selected);
  if(!file.existsAsFile()){status="That saved piece is no longer available";refreshSceneList();repaint();return;}
  juce::Component::SafePointer<MainView> safe(this);
  const auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
   .withTitle("Remove saved Piece?")
   .withMessage("Move the saved setup '"+selected+"' to Trash? Captured sounds, takes, and finals stay in this Piece's folder. The current music keeps playing.")
   .withButton("Cancel").withButton("Move to Trash").withAssociatedComponent(this);
  juce::NativeMessageBox::showAsync(options,[safe,file](int result){if(result==1&&safe!=nullptr)safe->removeSavedPieceFile(file);});
 }
 void removeSavedPieceFile(const juce::File& file){
  if(!file.existsAsFile()){status="That saved piece is no longer available";refreshSceneList();repaint();return;}
  if(!file.moveToTrash()){status="Could not move saved piece to Trash";repaint();return;}
  if(sceneList.getText()==file.getFileNameWithoutExtension())sceneList.setText({},juce::dontSendNotification);
  refreshSceneList();
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
  engine.setSampleLibrary(std::move(samples));
  engine.setMappedInstruments(std::move(mapped));
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
  root->setProperty("version",1);
  root->setProperty("trackCount",activeTrackCount);
  root->setProperty("selectedTrack",selectedTrack);
  root->setProperty("soloTrack",engine.soloTrack.load());
  root->setProperty("randomState",static_cast<juce::int64>(engine.randomState()));
  root->setProperty("tempo",tempo.getValue());root->setProperty("density",density.getValue());
  root->setProperty("key",key.getSelectedId());root->setProperty("scale",scale.getSelectedId());root->setProperty("shape",shape.getSelectedId());root->setProperty("mode",mode.getSelectedId());
  root->setProperty("room",room.getValue());root->setProperty("wet",wet.getValue());root->setProperty("drive",drive.getValue());root->setProperty("filter",filter.getValue());
  juce::Array<juce::var> lanes;
  for(int i=0;i<activeTrackCount;++i){
   juce::DynamicObject::Ptr lane=new juce::DynamicObject();
   lane->setProperty("name",trackNames[i]);lane->setProperty("subtitle",trackSubtitles[i]);
   lane->setProperty("kind",kinds[i].getSelectedId());lane->setProperty("sound",sounds[i].getSelectedId());
   if(kinds[i].getSelectedId()==3&&sounds[i].getSelectedId()>=2){const int index=sounds[i].getSelectedId()-2;if(index<sampleDisplayNames.size())lane->setProperty("soundName",sampleDisplayNames[index]);}
	   lane->setProperty("source",sources[i].getSelectedId());lane->setProperty("material",materials[i].getSelectedId());lane->setProperty("playMode",playModes[i].getSelectedId());lane->setProperty("sampleStart",sampleStarts[i].getValue());lane->setProperty("sampleSize",sampleSizes[i].getValue());lane->setProperty("sampleFade",sampleFades[i].getValue());lane->setProperty("sampleReverse",sampleReverses[i].getToggleState());lane->setProperty("role",roles[i].getSelectedId());lane->setProperty("relationship",relationships[i].getValue());lane->setProperty("dynamics",dynamics[i].getValue());lane->setProperty("articulation",articulations[i].getValue());lane->setProperty("tail",tails[i].getValue());lane->setProperty("length",lengths[i].getSelectedId());lane->setProperty("librarySound",librarySounds[i].getSelectedId());lane->setProperty("instrument",instruments[i].getSelectedId());
   lane->setProperty("level",levels[i].getValue());lane->setProperty("send",sends[i].getValue());lane->setProperty("color",colors[i].getValue());lane->setProperty("move",moves[i].getValue());lane->setProperty("weather",weathers[i].getValue());
   lane->setProperty("hold",holds[i].getToggleState());lane->setProperty("mute",mutes[i].getToggleState());
   lane->setProperty("connect",connects[i].getToggleState());
   juce::Array<juce::var> lanePhrase;for(int note:engine.getTrackPhrase(i))lanePhrase.add(note);
   lane->setProperty("phrase",juce::var(lanePhrase));
   juce::Array<juce::var> history;
   for(const auto& candidate:patternHistory[i]){
    juce::Array<juce::var> phrase;for(int note:candidate)phrase.add(note);
    history.add(juce::var(phrase));
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
  if(activeTrackCount>0){juce::Array<juce::var> phrase;for(int note:engine.getPhrase())phrase.add(note);root->setProperty("phrase",juce::var(phrase));}
  return root;
 }
 bool applyStateObject(juce::DynamicObject* root){
  if(root==nullptr)return false;
  setSoloTrack(-1);
  tempo.setValue(root->getProperty("tempo"));density.setValue(root->getProperty("density"));
  key.setSelectedId(static_cast<int>(root->getProperty("key")));scale.setSelectedId(static_cast<int>(root->getProperty("scale")));if(root->hasProperty("shape"))shape.setSelectedId(static_cast<int>(root->getProperty("shape")));mode.setSelectedId(static_cast<int>(root->getProperty("mode")));
  room.setValue(root->getProperty("room"));wet.setValue(root->getProperty("wet"));drive.setValue(root->getProperty("drive"));filter.setValue(root->getProperty("filter"));
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
   if(lane->hasProperty("librarySound"))librarySounds[i].setSelectedId(static_cast<int>(lane->getProperty("librarySound")),juce::dontSendNotification);
   if(lane->hasProperty("instrument"))instruments[i].setSelectedId(static_cast<int>(lane->getProperty("instrument")),juce::dontSendNotification);
   levels[i].setValue(lane->getProperty("level"));sends[i].setValue(lane->getProperty("send"));colors[i].setValue(lane->getProperty("color"));moves[i].setValue(lane->getProperty("move"));weathers[i].setValue(lane->getProperty("weather"));
   holds[i].setToggleState(static_cast<bool>(lane->getProperty("hold")),juce::sendNotification);mutes[i].setToggleState(static_cast<bool>(lane->getProperty("mute")),juce::sendNotification);
   connects[i].setToggleState(lane->hasProperty("connect")?static_cast<bool>(lane->getProperty("connect")):false,juce::sendNotification);
   const auto lanePhraseVar=lane->getProperty("phrase");
   if(auto* phraseArray=lanePhraseVar.getArray())if(phraseArray->size()==Engine::steps){std::array<int,Engine::steps> phrase{};for(int j=0;j<Engine::steps;++j)phrase[j]=static_cast<int>(phraseArray->getReference(j));engine.setTrackPhrase(i,phrase);}
   patternHistory[i].clear();
   if(auto* historyArray=lane->getProperty("patternHistory").getArray()){
    for(const auto& candidateVar:*historyArray)if(auto* candidate=candidateVar.getArray())if(candidate->size()==Engine::steps){
     std::array<int,Engine::steps> phrase{};for(int j=0;j<Engine::steps;++j)phrase[j]=static_cast<int>(candidate->getReference(j));
     patternHistory[i].push_back(phrase);
    }
   }
   if(patternHistory[i].empty())rememberPattern(i,false);
   patternHistoryIndex[i]=lane->hasProperty("patternHistoryIndex")?std::clamp(static_cast<int>(lane->getProperty("patternHistoryIndex")),0,static_cast<int>(patternHistory[i].size())-1):static_cast<int>(patternHistory[i].size())-1;
  }
  const auto phraseVar=root->getProperty("phrase");
  if(activeTrackCount>0)if(auto* phraseArray=phraseVar.getArray())if(phraseArray->size()==Engine::steps){std::array<int,Engine::steps> phrase{};for(int i=0;i<Engine::steps;++i)phrase[i]=static_cast<int>(phraseArray->getReference(i));engine.setPhrase(phrase);}
  if(root->hasProperty("selectedTrack"))selectTrack(static_cast<int>(root->getProperty("selectedTrack")));
  if(root->hasProperty("soloTrack"))setSoloTrack(static_cast<int>(root->getProperty("soloTrack")));
  if(root->hasProperty("randomState"))engine.seed(static_cast<uint32_t>(static_cast<juce::int64>(root->getProperty("randomState"))));
  engine.restartFromBeginning();
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
  status=ok?"Piece saved: "+file.getFileNameWithoutExtension():"Save failed";
  refreshSceneList();sceneList.setText(file.getFileNameWithoutExtension(),juce::dontSendNotification);
  removeSavedPiece.setEnabled(ok);
  repaint();
 }
 void loadScene(){
  auto file=sceneList.getText().isNotEmpty()?fileForSceneName(sceneList.getText()):fileForSceneName(sceneName.getText());
  if(!file.existsAsFile())file=fileForSceneName(sceneName.getText());
  if(!file.existsAsFile()){status="No saved piece";repaint();return;}
  juce::var data=juce::JSON::parse(file);auto* root=data.getDynamicObject();if(root==nullptr){status="Could not read piece";repaint();return;}
  const auto savedId=root->getProperty("pieceId").toString().trim();
  pieceId=savedId.isNotEmpty()?savedId:legacyPieceId(file.getFileNameWithoutExtension());
  sceneName.setText(file.getFileNameWithoutExtension(),juce::dontSendNotification);
  loadSampleLibrary();
  refreshTakeList(false);
  if(!applyStateObject(root)){status="Could not read piece";repaint();return;}
  sceneList.setText(file.getFileNameWithoutExtension(),juce::dontSendNotification);
  removeSavedPiece.setEnabled(true);
  status="Piece loaded: "+file.getFileNameWithoutExtension();
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
  recordToggle.setTooltip("Show or hide sound capture, take management, and final export.");
  recordDone.setTooltip("Return to the instrument.");
  patternBack.setTooltip("Restore the previous generated pattern for the selected track.");
  trackGenerate.setTooltip("Generate a new pattern for only the selected track, using its Role and Relationship.");
  removeTrack.setTooltip("Remove the selected track after confirmation.");
  patternForward.setTooltip("Restore the next generated pattern for the selected track.");
  soundPrevious.setTooltip("Try the previous sound in this track's sound family. The notes keep playing.");
  soundNext.setTooltip("Try the next sound in this track's sound family. The notes keep playing.");
  soundRandom.setTooltip("Try a different random sound from this track's sound family.");
  soundBack.setTooltip("Go back to a sound you tried on this track.");
  soundForward.setTooltip("Go forward through this track's tried sounds.");
  tempo.setTooltip("How fast the pattern moves, from 10 to 240 BPM.");
  density.setTooltip("How many steps tend to be active when phrases are generated.");
  key.setTooltip("Root note for generated phrases.");
  scale.setTooltip("Note collection used by generated phrases.");
  shape.setTooltip("The contour of new generated notes: wander, rise, fall, arch, still, or leap.");
  mode.setTooltip("Still repeats, Drift makes small changes, Renew makes a new phrase every few cycles.");
  sceneName.setTooltip("Name for the current piece.");
  sceneList.setTooltip("Saved pieces you can load back later.");
  newName.setTooltip("Give the current sound a new Piece name and a separate Keep group.");
  save.setTooltip("Save the current full setup as a piece.");
  load.setTooltip("Load the selected saved piece.");
  removeSavedPiece.setTooltip("Move the selected saved Piece setup to Trash. Its audio files stay on disk.");
  recordSound.setTooltip("Capture the microphone as a new reusable sound source.");
  recordMix.setTooltip("Record the app output as a take.");
  showRecordings.setTooltip("Open this piece's takes folder.");
  useLastSound.setTooltip("Put the last microphone recording onto the selected track.");
  discardSound.setTooltip("Delete the last microphone recording.");
  devices.setTooltip("Choose audio output and microphone input.");
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
  for(int i=0;i<Engine::maxTracks;++i){
   trackSelect[i].setTooltip("Select track "+juce::String(i+1)+".");
   holds[i].setTooltip("Keep this track's notes when generating or renewing.");
   mutes[i].setTooltip("Silence this track without deleting it.");
   solos[i].setTooltip("Hear only this track, even when muted. Click again to restore the mix.");
   connects[i].setTooltip("Join consecutive notes without a new attack. Short samples may end before the next note.");
	   kinds[i].setTooltip("Choose a sound family: made sounds, instruments, samples, or stranger mapped sounds.");
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
   levels[i].setTooltip("Track volume.");
   rowLevels[i].setTooltip("Track "+juce::String(i+1)+" volume. Fade this track without changing the selected track.");
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
