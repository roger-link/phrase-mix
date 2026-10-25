#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>

class PluginEffect {
public:
 explicit PluginEffect(bool instrument=false):instrumentSlot(instrument){formats.addDefaultFormats();}

 void prepare(double rate,int blockSize){
  const juce::ScopedLock guard(lock);
  sampleRate=rate;block=blockSize;
  if(instance!=nullptr){
   if(prepared)instance->releaseResources();
   extraInputs.setSize(static_cast<int>(channelPointers.size())-2,juce::jmax(1,blockSize));
   instance->setRateAndBufferSizeDetails(rate,blockSize);
   instance->prepareToPlay(rate,blockSize);
   prepared=true;
  }
 }

 void release(){
  const juce::ScopedLock guard(lock);
  if(instance!=nullptr&&prepared)instance->releaseResources();
  prepared=false;
 }

 void process(float* left,float* right,int samples,const juce::MidiBuffer* notes=nullptr){
  // Audio must never wait for plugin setup/editor/state calls. Audio Unit setup
  // can itself wait for CoreAudio to finish this callback.
  const juce::ScopedTryLock guard(lock);
  if(!guard.isLocked()||instance==nullptr||!prepared||bypassed.load()){
   if(instrumentSlot)notesOffPending.store(true);
   if(instrumentSlot){std::fill_n(left,samples,0.0f);std::fill_n(right,samples,0.0f);}
   return;
  }
  for(int offset=0;offset<samples;){
   const int count=juce::jmin(samples-offset,juce::jmax(1,block));
   if(instrumentSlot){std::fill_n(left+offset,count,0.0f);std::fill_n(right+offset,count,0.0f);}
   channelPointers[0]=left+offset;
   channelPointers[1]=right+offset;
   // Optional input buses, such as sidechains, receive silence.
   extraInputs.clear();
   for(size_t channel=2;channel<channelPointers.size();++channel)
    channelPointers[channel]=extraInputs.getWritePointer(static_cast<int>(channel)-2);
   juce::AudioBuffer<float> audio(channelPointers.data(),static_cast<int>(channelPointers.size()),count);
   juce::MidiBuffer midi;
   if(instrumentSlot&&notesOffPending.exchange(false))midi.addEvent(juce::MidiMessage::allNotesOff(1),0);
   if(notes!=nullptr)midi.addEvents(*notes,offset,count,-offset);
   instance->processBlock(audio,midi);
   offset+=count;
  }
 }

 juce::String replace(std::unique_ptr<juce::AudioPluginInstance> next,const juce::PluginDescription& description,const juce::MemoryBlock& state,bool bypass){
  if(next==nullptr||description.isInstrument!=instrumentSlot)return instrumentSlot?"Choose an instrument plugin":"Choose an audio effect, not an instrument";
  auto layout=next->getBusesLayout();
  if((!instrumentSlot&&layout.inputBuses.isEmpty())||layout.outputBuses.isEmpty())return instrumentSlot?"Instrument needs stereo audio output":"Effect needs stereo audio input and output";
  if((!instrumentSlot&&layout.inputBuses[0]!=juce::AudioChannelSet::stereo())
     ||layout.outputBuses[0]!=juce::AudioChannelSet::stereo()
     ||next->getTotalNumOutputChannels()!=2){
   if(!instrumentSlot)layout.inputBuses.set(0,juce::AudioChannelSet::stereo());
   layout.outputBuses.set(0,juce::AudioChannelSet::stereo());
   for(int i=1;i<layout.outputBuses.size();++i)layout.outputBuses.set(i,juce::AudioChannelSet::disabled());
   if(!next->setBusesLayout(layout))return instrumentSlot?"This instrument does not support stereo output":"This effect does not support a stereo mix";
  }
  if((!instrumentSlot&&next->getBusesLayout().inputBuses[0]!=juce::AudioChannelSet::stereo())
     ||next->getBusesLayout().outputBuses[0]!=juce::AudioChannelSet::stereo()
     ||next->getTotalNumOutputChannels()!=2)
   return instrumentSlot?"This instrument does not support stereo output":"This effect does not support a stereo mix";
  const int inputChannels=next->getTotalNumInputChannels();
  if(!state.isEmpty())next->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
  std::unique_ptr<juce::AudioPluginInstance> old;
  bool oldPrepared=false;
  {
   const juce::ScopedLock guard(lock);
   channelPointers.resize(static_cast<size_t>(juce::jmax(2,inputChannels)));
   extraInputs.setSize(juce::jmax(0,inputChannels-2),juce::jmax(1,block));
   next->setRateAndBufferSizeDetails(sampleRate,block);
   next->prepareToPlay(sampleRate,block);
   old=std::move(instance);
   oldPrepared=prepared;
   instance=std::move(next);
   available.store(true);
   pluginDescription=description;
   bypassed=bypass;
   prepared=true;
  }
  if(old!=nullptr&&oldPrepared)old->releaseResources();
  return {};
 }

 void clear(){
  std::unique_ptr<juce::AudioPluginInstance> old;
  bool oldPrepared=false;
  {
   const juce::ScopedLock guard(lock);
   old=std::move(instance);
   available.store(false);
   oldPrepared=prepared;
   pluginDescription={};
   bypassed=false;
   prepared=false;
  }
  if(old!=nullptr&&oldPrepared)old->releaseResources();
 }

 void setBypassed(bool value){bypassed.store(value);}
 void requestAllNotesOff(){if(instrumentSlot)notesOffPending.store(true);}
 bool isBypassed() const {return bypassed.load();}
 bool hasPlugin() const {return available.load();}
 juce::String name() const {const juce::ScopedLock guard(lock);return instance!=nullptr?pluginDescription.name:juce::String(instrumentSlot?"No instrument":"No effect");}
 juce::AudioProcessorEditor* createEditor(){
  const juce::ScopedLock guard(lock);
  if(instance==nullptr)return nullptr;
  if(auto* editor=instance->createEditorIfNeeded())return editor;
  return new juce::GenericAudioProcessorEditor(*instance);
 }

 juce::var saveState() const {
  const juce::ScopedLock guard(lock);
  if(instance==nullptr)return {};
  juce::MemoryBlock data;
  instance->getStateInformation(data);
  juce::DynamicObject::Ptr result=new juce::DynamicObject();
  if(auto descriptionXml=pluginDescription.createXml())result->setProperty("description",descriptionXml->toString());
  result->setProperty("state",data.toBase64Encoding());
  result->setProperty("bypassed",bypassed.load());
  return juce::var(result.get());
 }

 juce::AudioPluginFormatManager formats;
 juce::KnownPluginList known;

private:
 mutable juce::CriticalSection lock;
 std::unique_ptr<juce::AudioPluginInstance> instance;
 juce::PluginDescription pluginDescription;
 std::vector<float*> channelPointers;
 juce::AudioBuffer<float> extraInputs;
 double sampleRate=48000.0;
 int block=512;
 std::atomic<bool> bypassed{false};
 std::atomic<bool> available{false};
 bool prepared=false;
 bool instrumentSlot=false;
 std::atomic<bool> notesOffPending{false};
};

class PluginPicker final : public juce::Component {
public:
 PluginPicker(juce::AudioPluginFormatManager& formats,juce::KnownPluginList& knownList,const juce::File& scanGuard,
              std::function<void(juce::PluginDescription)> selected,bool instrument=false)
  : known(knownList),onSelected(std::move(selected)),list(formats,knownList,scanGuard,nullptr,true),instrumentSlot(instrument){
  setSize(680,500);
  list.setOptionsButtonText("Scan / manage");
  list.getTableListBox().setMultipleSelectionEnabled(false);
  addAndMakeVisible(list);
  addAndMakeVisible(feedback);
  feedback.setColour(juce::Label::textColourId,juce::Colour(0xffeabf8e));
  addAndMakeVisible(load);
  load.setButtonText(instrumentSlot?"Load instrument":"Load selected effect");
  load.onClick=[this]{
   const auto types=known.getTypes();
   const int row=list.getTableListBox().getSelectedRow();
   if(!juce::isPositiveAndBelow(row,types.size())){
    feedback.setText(instrumentSlot?"Select an instrument first.":"Select an effect first.",juce::dontSendNotification);
    return;
   }
   if(types.getReference(row).isInstrument!=instrumentSlot){
    feedback.setText(types.getReference(row).name+(instrumentSlot?" is an effect. Choose an instrument.":" is an instrument. Choose an effect."),juce::dontSendNotification);
    return;
   }
   feedback.setText({},juce::dontSendNotification);
   onSelected(types.getReference(row));
   if(auto* window=findParentComponentOfClass<juce::DocumentWindow>())window->setVisible(false);
  };
 }
 void resized() override {
  list.setBounds(12,12,getWidth()-24,getHeight()-62);
  feedback.setBounds(12,getHeight()-42,getWidth()-212,30);
  load.setBounds(getWidth()-188,getHeight()-42,176,30);
 }
private:
 juce::KnownPluginList& known;
 std::function<void(juce::PluginDescription)> onSelected;
 juce::PluginListComponent list;
 juce::Label feedback;
 juce::TextButton load;
 bool instrumentSlot=false;
};

class PluginPickerWindow final : public juce::DocumentWindow {
public:
 explicit PluginPickerWindow(PluginPicker* picker,bool instrument=false)
  : juce::DocumentWindow(instrument?"Instrument plugins":"Effect plugins",juce::Colour(0xff222727),juce::DocumentWindow::closeButton){
  setUsingNativeTitleBar(true);
  setContentOwned(picker,true);
  setResizable(true,true);
  setResizeLimits(500,360,1000,800);
  centreWithSize(680,500);
  setVisible(true);
 }
 void closeButtonPressed() override {setVisible(false);}
};

class PluginEditorWindow final : public juce::DocumentWindow {
public:
 PluginEditorWindow(const juce::String& title,juce::AudioProcessorEditor* editor)
  : juce::DocumentWindow(title,juce::Colour(0xff222727),juce::DocumentWindow::closeButton){
  setUsingNativeTitleBar(true);
  const int width=std::max(320,editor->getWidth());
  const int height=std::max(240,editor->getHeight());
  setContentOwned(editor,true);
  setResizable(false,false);
  centreWithSize(width,height);
  setVisible(true);
 }
 void closeButtonPressed() override {setVisible(false);}
};
