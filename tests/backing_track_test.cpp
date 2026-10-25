#include "../Source/BackingTrack.h"
#include "../Source/Engine.h"
#include <cassert>
#include <iostream>

int main(){
 BackingTracks player;player.prepare(512);
 auto clip=std::make_shared<BackingTracks::Clip>();clip->rate=8000;
 // More than eight seconds, with deliberately different stereo channels.
 clip->left.resize(96000,0.1f);clip->right.resize(96000,0.3f);clip->left[80000]=0.4f;
 BackingTracks::Clips clips{};clips[0]=clip;player.publish(clips);
 std::array<BackingTracks::Controls,8> controls{};controls[0]={true,false,true,true,1.0f};
 float left[512]{},right[512]{};
 player.render(left,right,1,10,8000,true,true,controls,1.5f);
 assert(std::abs(left[0]-0.4f)<0.00001f&&std::abs(right[0]-0.3f)<0.00001f);
 // Resampling follows file time, independently of BPM; last frame is not lost.
 left[0]=right[0]=0;player.render(left,right,1,static_cast<double>(95999)/8000,48000,true,true,controls,1.5f);
 assert(std::abs(left[0]-0.1f)<0.00001f&&std::abs(right[0]-0.3f)<0.00001f);
 left[0]=right[0]=0;player.render(left,right,1,12.0,8000,true,true,controls,1.5f);assert(left[0]==0&&right[0]==0);
 controls[0].loop=true;player.render(left,right,1,12.0,8000,true,true,controls,1.5f);assert(std::abs(left[0]-0.1f)<0.00001f);
 left[0]=right[0]=0;player.render(left,right,1,0,8000,false,true,controls,1.5f);assert(left[0]==0&&right[0]==0);
 controls[0].audible=false;player.render(left,right,1,0,8000,true,true,controls,1.5f);assert(left[0]==0&&right[0]==0);
 controls[0].audible=true;controls[0].focused=false;player.render(left,right,1,0,8000,true,true,controls,1.5f);assert(left[0]==0&&right[0]==0);
 controls[0].focused=true;controls[0].level=0.5f;player.render(left,right,1,0,8000,true,true,controls,1.5f);
 assert(std::abs(left[0]-0.05f)<0.00001f&&std::abs(right[0]-0.15f)<0.00001f);
 // A replaced/missing clip is silent; publishing doesn't keep the old audio playing.
 player.publish({});left[0]=right[0]=0;player.render(left,right,1,0,8000,true,true,controls,1.5f);assert(left[0]==0&&right[0]==0);
 // Capture at an arbitrary step retains fractional phase at audio-block boundaries.
 Engine engine;engine.prepare(8000);engine.activeTracks=2;engine.bpm=120;engine.transportStartStep=3.5;
 engine.backingTrackMask=1;engine.midiOutputTrack=-1;
 std::array<int,Engine::maxLoopSteps> phrase;phrase.fill(60);engine.setTrackPattern(0,phrase);engine.setTrackPattern(1,phrase);
 engine.hold[0]=true;engine.hold[1]=true;engine.playing=true;
 Engine::MidiEvents events;TakeTiming timing;int64_t outputFrames=0;bool atStep4=false;
 for(int block=0;block<30;++block){
  engine.render(left,right,512,&events);
  timing.observe(engine.blockTempo,engine.blockStartStep,engine.blockPlaying,512,8000);
  for(int i=0;i<events.count;++i){const auto& event=events.items[i];assert(event.track==1);if(event.on&&outputFrames+event.sample==500)atStep4=true;}
  outputFrames+=512;
 }
 assert(atStep4&&timing.canAlign()&&std::abs(timing.startStep-3.5)<0.000001);
 assert(std::abs(engine.blockStartSeconds-29.0*512/8000)<0.000001);
 engine.bpm=100;engine.render(left,right,512,&events);timing.observe(engine.blockTempo,engine.blockStartStep,engine.blockPlaying,512,8000);assert(!timing.canAlign());
 TakeTiming stopped;stopped.observe(96,0,false,512,8000);assert(!stopped.canAlign());
 TakeTiming restarted;restarted.observe(96,4,true,512,8000);restarted.observe(96,0,true,512,8000);assert(!restarted.canAlign());
 engine.playing=false;engine.render(left,right,512,&events);engine.playing=true;engine.render(left,right,512,&events);
 assert(engine.blockRestarted&&engine.blockStartSeconds==0&&std::abs(engine.blockStartStep-3.5)<0.000001);
 std::cout<<"Backing track checks passed: full stereo, original speed, file end, loop, stop/restart, mute/focus/level, missing audio, MIDI exclusion, fractional phase, steady and changing tempo\n";
}
