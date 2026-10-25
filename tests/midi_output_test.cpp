#include "../Source/Engine.h"
#include <algorithm>
#include <cassert>
#include <iostream>

int main(){
 Engine engine;
 engine.prepare(8000);
 engine.activeTracks=2;
 engine.bpm=120;
 engine.noteLength[0]=0;
 engine.articulation[0]=0;
 std::array<float,512> left{},right{};
 Engine::MidiEvents events;
 engine.render(left.data(),right.data(),512,&events);
 std::array<int,Engine::steps> first{},second{};
 first.fill(-1);second.fill(-1);
 first[0]=60;first[1]=62;second[0]=67;
 engine.setTrackPhrase(0,first);
 engine.setTrackPhrase(1,second);
 engine.midiOutputTrack=0;
 engine.playing=true;
 engine.render(left.data(),right.data(),512,&events);
 assert(events.count==2);
 assert(events.items[0].sample==0&&events.items[0].track==0&&events.items[0].note==60&&events.items[0].on);
 assert(events.items[1].sample==380&&events.items[1].track==0&&events.items[1].note==60&&!events.items[1].on);

 engine.render(left.data(),right.data(),512,&events);
 assert(events.count==1&&events.items[0].track==0&&events.items[0].note==62&&events.items[0].on);
 engine.mute[0]=true;
 engine.render(left.data(),right.data(),512,&events);
 assert(events.count==1&&events.items[0].sample==0&&!events.items[0].on);

 engine.midiOutputTrack=1;
 engine.restartFromBeginning();
 engine.render(left.data(),right.data(),512,&events);
 assert(events.count==1&&events.items[0].track==1&&events.items[0].note==67&&events.items[0].on);
 engine.playing=false;
 engine.render(left.data(),right.data(),512,&events);
 assert(events.count==1&&events.items[0].track==1&&events.items[0].note==67&&!events.items[0].on);

 Engine all;
 all.prepare(8000);all.activeTracks=2;all.bpm=120;
 all.render(left.data(),right.data(),512,&events);
 all.setTrackPhrase(0,first);all.setTrackPhrase(1,second);
 all.midiOutputTrack=-1;all.playing=true;
 all.render(left.data(),right.data(),512,&events);
 assert(events.count==2&&events.items[0].track==0&&events.items[1].track==1);
 all.soloTrack=0;
 all.render(left.data(),right.data(),512,&events);
 assert(events.count==3&&events.items[0].track==1&&!events.items[0].on);
 all.midiOutputTrack=-2;
 all.render(left.data(),right.data(),512,&events);
 assert(events.count==1&&events.items[0].track==0&&!events.items[0].on);

 Engine mixedRoutes;
 mixedRoutes.prepare(8000);mixedRoutes.activeTracks=2;mixedRoutes.bpm=120;
 mixedRoutes.setTrackPhrase(0,first);mixedRoutes.setTrackPhrase(1,second);
 mixedRoutes.hostedInstrumentMask=1; mixedRoutes.midiOutputTrack=1; mixedRoutes.playing=true;
 mixedRoutes.render(left.data(),right.data(),512,&events);
 assert(events.count==2&&events.items[0].track==0&&events.items[1].track==1);
 mixedRoutes.midiOutputTrack=-2;
 mixedRoutes.restartFromBeginning();
 mixedRoutes.render(left.data(),right.data(),512,&events);
 assert(std::any_of(events.items.begin(),events.items.begin()+events.count,[](const Engine::MidiEvent& event){return event.track==0&&event.on;}));
 assert(std::none_of(events.items.begin(),events.items.begin()+events.count,[](const Engine::MidiEvent& event){return event.track==1&&event.on;}));

 Engine connected;
 connected.prepare(8000);connected.activeTracks=1;connected.bpm=120;
 connected.noteLength[0]=0;connected.articulation[0]=0;connected.connect[0]=true;
 connected.render(left.data(),right.data(),512,&events);
 first[1]=60;connected.setTrackPhrase(0,first);
 connected.midiOutputTrack=0;connected.playing=true;
 connected.render(left.data(),right.data(),512,&events);
 assert(events.count==1&&events.items[0].on);
 connected.render(left.data(),right.data(),512,&events);
 assert(events.count==0); // Same-pitch connected step stays held.
 connected.connect[0]=false;
 connected.render(left.data(),right.data(),512,&events);
 assert(events.count==1&&!events.items[0].on);
 std::cout<<"MIDI output timing and note release checks passed\n";
}
