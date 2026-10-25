// Native JUCE regression: plugin lifecycle calls must not block the audio callback.
#include "../Source/PluginEffect.h"
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>

struct Gate {
 std::promise<void> entered, resume;
 std::shared_future<void> resumed=resume.get_future().share();
 void wait(){entered.set_value();resumed.wait();}
};
class TestPlugin final : public juce::AudioPluginInstance {
public:
 TestPlugin(bool synth,Gate* setup=nullptr):AudioPluginInstance(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),instrument(synth),setupGate(setup){}
 const juce::String getName() const override{return "Test";}
 void fillInPluginDescription(juce::PluginDescription& d) const override{d.name="Test";d.isInstrument=instrument;}
 void prepareToPlay(double,int) override{if(setupGate)setupGate->wait();}
 void releaseResources() override{}
 void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi) override{
  for(const auto event:midi)if(event.getMessage().isAllNotesOff())sawNotesOff=true;
  for(int c=0;c<b.getNumChannels();++c)juce::FloatVectorOperations::fill(b.getWritePointer(c),0.25f,b.getNumSamples());
 }
 bool acceptsMidi() const override{return instrument;}
 bool producesMidi() const override{return false;}
 double getTailLengthSeconds() const override{return 0;}
 bool hasEditor() const override{return false;}
 juce::AudioProcessorEditor* createEditor() override{return nullptr;}
 int getNumPrograms() override{return 1;}
 int getCurrentProgram() override{return 0;}
 void setCurrentProgram(int) override{}
 const juce::String getProgramName(int) override{return {};}
 void changeProgramName(int,const juce::String&) override{}
 void getStateInformation(juce::MemoryBlock&) override{if(stateGate)stateGate->wait();}
 void setStateInformation(const void*,int) override{}
 bool instrument,sawNotesOff=false;
 Gate* setupGate=nullptr;
 Gate* stateGate=nullptr;
};
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void checkCallbackWhileBlocked(PluginEffect& slot,Gate& gate,bool instrument){
 gate.entered.get_future().wait();
 float left[64],right[64];std::fill_n(left,64,0.75f);std::fill_n(right,64,0.75f);
 auto callback=std::async(std::launch::async,[&]{
  slot.hasPlugin();slot.isBypassed();slot.process(left,right,64);
 });
 const bool ready=callback.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
 gate.resume.set_value();callback.get();
 require(ready,"Audio callback waited for plugin lifecycle lock");
 require(left[0]==(instrument?0.0f:0.75f)&&right[63]==left[0],"Busy plugin fallback changed audio incorrectly");
}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 try{
  PluginEffect instrument(true);instrument.prepare(48000,64);
  Gate setup;
  auto plugin=std::make_unique<TestPlugin>(true,&setup);auto* raw=plugin.get();
  juce::PluginDescription description;raw->fillInPluginDescription(description);
  auto loading=std::async(std::launch::async,[&]{return instrument.replace(std::move(plugin),description,{},false);});
  checkCallbackWhileBlocked(instrument,setup,true);
  require(loading.get().isEmpty()&&instrument.hasPlugin(),"Instrument installation failed");
  float left[64]{},right[64]{};instrument.process(left,right,64);
  require(left[0]==0.25f&&raw->sawNotesOff,"Instrument did not resume with notes off");
  Gate saving;raw->stateGate=&saving;
  auto state=std::async(std::launch::async,[&]{return instrument.saveState();});
  checkCallbackWhileBlocked(instrument,saving,true);(void)state.get();raw->stateGate=nullptr;
  instrument.setBypassed(true);require(instrument.isBypassed(),"Bypass flag failed");
  instrument.clear();require(!instrument.hasPlugin()&&!instrument.isBypassed(),"Clear flags failed");
  // A busy effect slot must preserve the backing audio.
  PluginEffect effect;
  class Effect final : public juce::AudioPluginInstance {
  public:
   explicit Effect(Gate& g):AudioPluginInstance(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true)),gate(g){}
   const juce::String getName()const override{return "Effect";}
   void fillInPluginDescription(juce::PluginDescription& d)const override{d.name="Effect";}
   void prepareToPlay(double,int)override{} void releaseResources()override{}
   void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override{}
   bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
   double getTailLengthSeconds()const override{return 0;}bool hasEditor()const override{return false;}
   juce::AudioProcessorEditor* createEditor()override{return nullptr;}
   int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}
   void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return {};}
   void changeProgramName(int,const juce::String&)override{}
   void getStateInformation(juce::MemoryBlock&)override{gate.wait();}
   void setStateInformation(const void*,int)override{}
   Gate& gate;
  };
  Gate savingEffect;auto fx=std::make_unique<Effect>(savingEffect);juce::PluginDescription fd;fx->fillInPluginDescription(fd);
  require(effect.replace(std::move(fx),fd,{},false).isEmpty(),"Effect installation failed");
  auto effectState=std::async(std::launch::async,[&]{return effect.saveState();});
  checkCallbackWhileBlocked(effect,savingEffect,false);(void)effectState.get();
  std::cout<<"PASS: audio stays responsive during plugin setup/state save; instrument silence, effect passthrough, and notes-off recovery\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
