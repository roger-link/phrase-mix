#include "../Source/Engine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

static bool hasNotes(const Engine& e,int track){
 for(int j=0;j<Engine::steps;++j)if(e.display[track*Engine::steps+j].load()>=0)return true;
 return false;
}

static int matchingActiveSteps(const Engine& e,int a,int b){
 int matches=0;
 for(int j=0;j<Engine::steps;++j)if(e.display[a*Engine::steps+j].load()>=0&&e.display[b*Engine::steps+j].load()>=0)++matches;
 return matches;
}

int main(){
 std::array<float,512> l{},r{},l2{},r2{};
 Engine empty;empty.seed(42);empty.prepare(48000);empty.render(l.data(),r.data(),512);
 for(int j=0;j<Engine::maxTracks*Engine::steps;++j)assert(empty.display[j].load()==-1);

 Engine staged;staged.seed(43);staged.prepare(48000);staged.activateTracks(1);
 staged.generateTrack(0);staged.generateTrack(1);
 assert(!hasNotes(staged,1));
 staged.activateTracks(2);
 assert(hasNotes(staged,1));

 Engine a,b;a.seed(42);b.seed(42);a.prepare(48000);b.prepare(48000);a.activeTracks=3;b.activeTracks=3;
 a.render(l.data(),r.data(),512);b.render(l2.data(),r2.data(),512);
 for(int j=0;j<Engine::maxTracks*Engine::steps;++j)assert(a.display[j].load()==b.display[j].load());
 assert(hasNotes(a,0));assert(hasNotes(a,1));assert(hasNotes(a,2));

 std::array<int,Engine::steps> held{};for(int j=0;j<Engine::steps;++j)held[j]=a.display[Engine::steps+j].load();
 a.hold[1]=true;a.request.fetch_add(1);a.render(l.data(),r.data(),512);
 for(int j=0;j<Engine::steps;++j)assert(a.display[Engine::steps+j].load()==held[j]);

 Engine related;related.seed(1234);related.prepare(48000);related.activeTracks=3;related.density=0.82f;
 related.relationship[1]=0.0f;related.role[1]=0;
 related.relationship[2]=1.0f;related.role[2]=3;
 related.request.fetch_add(1);related.render(l.data(),r.data(),512);
 assert(hasNotes(related,0));assert(hasNotes(related,1));assert(hasNotes(related,2));
 assert(matchingActiveSteps(related,0,1)>=matchingActiveSteps(related,0,2));
 const auto mainBeforeFollowerGenerate=related.getTrackPhrase(0);
 const auto otherBeforeFollowerGenerate=related.getTrackPhrase(2);
 related.generateTrack(1);
 assert(related.getTrackPhrase(0)==mainBeforeFollowerGenerate);
 assert(related.getTrackPhrase(2)==otherBeforeFollowerGenerate);

 Engine linkedDrift;linkedDrift.seed(2026);linkedDrift.prepare(48000);linkedDrift.activeTracks=3;linkedDrift.density=0.82f;
 linkedDrift.relationship[1]=0.0f;linkedDrift.role[1]=0;
 linkedDrift.relationship[2]=1.0f;linkedDrift.role[2]=3;
 linkedDrift.render(l.data(),r.data(),512);
 std::array<std::array<int,Engine::steps>,3> beforeDrift{};
 for(int c=0;c<3;++c)beforeDrift[c]=linkedDrift.getTrackPhrase(c);
 linkedDrift.hold[0]=true;linkedDrift.mode=1;linkedDrift.every=1;linkedDrift.playing=true;
 int wraps=0,previousHead=-1;
 for(int k=0;k<800&&wraps<2;++k){
  linkedDrift.render(l.data(),r.data(),512);
  const int head=linkedDrift.playhead.load();
  if(head==0&&previousHead!=0)++wraps;
  previousHead=head;
 }
 assert(wraps==2);
 int driftedSteps=0;
 for(int c=0;c<3;++c){
  int changed=0;
  const auto after=linkedDrift.getTrackPhrase(c);
  for(int j=0;j<Engine::steps;++j)changed+=after[j]!=beforeDrift[c][j];
  if(c==0)assert(changed==0);
  else {assert(changed<=2);driftedSteps+=changed;}
 }
 assert(driftedSteps>0);

 Engine timedRenew;timedRenew.seed(9026);timedRenew.prepare(8000);timedRenew.activeTracks=1;timedRenew.bpm=240;timedRenew.mode=2;timedRenew.every=3;timedRenew.playing=true;
 timedRenew.render(l.data(),r.data(),512);
 const auto initialRenewPhrase=timedRenew.getTrackPhrase(0);
 int renewCycle=1,renewHead=timedRenew.playhead.load();
 for(int k=0;k<200&&renewCycle<4;++k){
  timedRenew.render(l.data(),r.data(),512);
  const int head=timedRenew.playhead.load();
  if(head<renewHead)++renewCycle;
  if(renewCycle<4)assert(timedRenew.getTrackPhrase(0)==initialRenewPhrase);
  renewHead=head;
 }
 assert(renewCycle==4);
 assert(timedRenew.getTrackPhrase(0)!=initialRenewPhrase);

 std::array<int,Engine::steps> beforeSoundChange{};for(int j=0;j<Engine::steps;++j)beforeSoundChange[j]=a.display[j].load();
 for(int i=0;i<Engine::maxTracks;++i){a.sourceType[i]=8;a.material[i]=6;a.noteLength[i]=3;a.sampleChoice[i]=0;}
 a.render(l.data(),r.data(),512);
 for(int j=0;j<Engine::steps;++j)assert(a.display[j].load()==beforeSoundChange[j]);

 Engine clicked;clicked.seed(91);clicked.prepare(48000);clicked.activeTracks=1;std::array<int,Engine::steps> phrase{};phrase.fill(-1);phrase[4]=60;phrase[5]=64;clicked.setPhrase(phrase);clicked.render(l.data(),r.data(),512);
 clicked.toggleStep(0,5);assert(clicked.display[5].load()==-1);
 clicked.toggleStep(0,5);assert(clicked.display[5].load()>=0);
 clicked.activeTracks=6;clicked.generateTrack(4);clicked.generateTrack(5);clicked.render(l.data(),r.data(),512);
 assert(hasNotes(clicked,4)||hasNotes(clicked,5));

 Engine recalled;recalled.seed(314);recalled.prepare(48000);recalled.activeTracks=1;
 recalled.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> keptPhrase{};keptPhrase.fill(-1);keptPhrase[0]=60;keptPhrase[8]=67;
 recalled.setPhrase(keptPhrase);
 const auto keptRandomState=recalled.randomState();
 recalled.seed(123);recalled.seed(keptRandomState);
 assert(recalled.randomState()==keptRandomState);
 recalled.mode=0;recalled.restartFromBeginning();recalled.playing=true;
 recalled.render(l.data(),r.data(),512);
 assert(recalled.getPhrase()==keptPhrase);
 assert(recalled.playhead.load()==0);
 for(int k=0;k<100;++k)recalled.render(l.data(),r.data(),512);
 recalled.restartFromBeginning();recalled.render(l.data(),r.data(),512);
 assert(recalled.getPhrase()==keptPhrase);
 assert(recalled.playhead.load()==0);

 for(int shape=0;shape<6;++shape){
  Engine shaped;shaped.seed(500+shape);shaped.prepare(48000);shaped.activeTracks=1;shaped.shape=shape;shaped.density=1.0f;shaped.request.fetch_add(1);shaped.render(l.data(),r.data(),512);
  bool hasNote=false;for(int j=0;j<Engine::steps;++j){const int n=shaped.display[j].load();assert(n<0||(n>=24&&n<=88));hasNote=hasNote||n>=0;}
  assert(hasNote);
 }

 Engine evolving;evolving.seed(77);evolving.prepare(48000);evolving.activeTracks=1;evolving.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> beforeEvolve{};for(int j=0;j<Engine::steps;++j)beforeEvolve[j]=evolving.display[j].load();
 evolving.mode=1;evolving.every=1;evolving.playing=true;
 for(int k=0;k<700;++k)evolving.render(l.data(),r.data(),512);
 bool evolved=false;for(int j=0;j<Engine::steps;++j)evolved=evolved||beforeEvolve[j]!=evolving.display[j].load();
 assert(evolved);

 a.playing=true;bool audible=false;
 for(int k=0;k<500;++k){a.render(l.data(),r.data(),512);for(float v:l){assert(std::isfinite(v));assert(std::abs(v)<=1);if(std::abs(v)>0.001)audible=true;}}
 assert(audible);
 for(auto& muted:a.mute)muted=true;
 a.render(l.data(),r.data(),512);for(float v:l)assert(v==0);
 for(auto& muted:a.mute)muted=false;
 a.playing=false;a.render(l.data(),r.data(),512);for(float v:l)assert(v==0);

 Engine audition;audition.seed(19);audition.prepare(48000);audition.activeTracks=2;
 audition.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> repeating{};repeating.fill(60);
 audition.setTrackPhrase(0,repeating);audition.setTrackPhrase(1,repeating);
 audition.send[0]=0;audition.send[1]=0;audition.wet=0;
 audition.level[0]=0;audition.level[1]=1;
 audition.soloTrack=0;audition.playing=true;
 for(int k=0;k<30;++k){audition.render(l.data(),r.data(),512);for(float v:l)assert(v==0);}
 audition.mute[1]=true;audition.soloTrack=1;
 bool mutedTrackHeard=false;
 for(int k=0;k<30;++k){audition.render(l.data(),r.data(),512);for(float v:l)if(std::abs(v)>0.001f)mutedTrackHeard=true;}
 assert(mutedTrackHeard&&audition.mute[1].load());
 audition.soloTrack=-1;audition.level[0]=1;
 bool mixRestored=false;
 for(int k=0;k<30;++k){audition.render(l.data(),r.data(),512);for(float v:l)if(std::abs(v)>0.001f)mixRestored=true;}
 assert(mixRestored&&audition.mute[1].load());

 Engine focused;focused.seed(71);focused.prepare(48000);focused.activeTracks=2;
 focused.render(l.data(),r.data(),512);
 focused.setTrackPhrase(0,repeating);focused.setTrackPhrase(1,repeating);
 focused.sourceType[0]=4;focused.sourceType[1]=4;
 focused.level[0]=0;focused.level[1]=1;
 focused.send[0]=0;focused.send[1]=0;focused.wet=0;focused.focusFadeSeconds=0.1f;focused.playing=true;
 bool focusBeforeHeard=false;
 for(int k=0;k<30;++k){focused.render(l.data(),r.data(),512);for(float v:l)focusBeforeHeard=focusBeforeHeard||std::abs(v)>0.001f;}
 assert(focusBeforeHeard);
 focused.focusTrack=0;
 for(int k=0;k<40;++k)focused.render(l.data(),r.data(),512);
 for(float v:l)assert(std::abs(v)<0.001f);
 assert(focused.level[1].load()==1.0f);
 focused.focusTrack=-1;
 bool focusAfterHeard=false;
 for(int k=0;k<40;++k){focused.render(l.data(),r.data(),512);for(float v:l)focusAfterHeard=focusAfterHeard||std::abs(v)>0.001f;}
 assert(focusAfterHeard&&focused.level[1].load()==1.0f);

 Engine fades;fades.seed(81);fades.prepare(48000);fades.activeTracks=2;
 fades.render(l.data(),r.data(),512);fades.setTrackLevel(0,0.8f);fades.setTrackLevel(1,0.3f);
 fades.beginTrackFade(0,0.0f,1.0f);fades.playing=true;
 for(int k=0;k<47;++k)fades.render(l.data(),r.data(),512);
 assert(fades.isTrackFadeActive(0));
 assert(fades.level[0].load()>0.38f&&fades.level[0].load()<0.42f);
 assert(fades.level[1].load()==0.3f);
 fades.playing=false;for(int k=0;k<20;++k)fades.render(l.data(),r.data(),512);
 assert(fades.level[0].load()>0.38f&&fades.level[0].load()<0.42f);
 fades.playing=true;for(int k=0;k<47;++k)fades.render(l.data(),r.data(),512);
 assert(!fades.isTrackFadeActive(0)&&fades.level[0].load()==0.0f);
 fades.beginTrackFade(0,1.0f,1.0f);
 for(int k=0;k<10;++k)fades.render(l.data(),r.data(),512);
 fades.setTrackLevel(0,0.65f);
 for(int k=0;k<100;++k)fades.render(l.data(),r.data(),512);
 assert(!fades.isTrackFadeActive(0)&&fades.level[0].load()==0.65f);
 fades.mute[0]=true;fades.mute[1]=true;
 for(int k=0;k<20;++k){fades.render(l.data(),r.data(),512);for(float v:l)assert(v==0.0f);}
 assert(fades.playhead.load()>=0);
 fades.beginTrackFade(0,0.0f,1.0f);fades.beginTrackFade(1,1.0f,2.0f);
 for(int k=0;k<47;++k)fades.render(l.data(),r.data(),512);
 assert(fades.level[0].load()>0.30f&&fades.level[0].load()<0.34f);
 assert(fades.level[1].load()>0.46f&&fades.level[1].load()<0.49f);

 Engine fadeRace;fadeRace.seed(82);fadeRace.prepare(48000);fadeRace.activeTracks=1;fadeRace.playing=true;
 std::thread changeFade([&]{
  for(int k=0;k<300;++k){
   fadeRace.beginTrackFade(0,static_cast<float>(k%101)/100.0f,0.5f);
   if(k%3==0)fadeRace.setTrackLevel(0,static_cast<float>(k%101)/100.0f);
   if(k%7==0)fadeRace.cancelTrackFade(0);
  }
 });
 for(int k=0;k<300;++k){fadeRace.render(l.data(),r.data(),512);assert(std::isfinite(fadeRace.level[0].load()));assert(fadeRace.level[0].load()>=0.0f&&fadeRace.level[0].load()<=1.0f);}
 changeFade.join();

 Engine swapping;swapping.seed(72);swapping.prepare(48000);swapping.activeTracks=1;swapping.sourceType[0]=8;swapping.playing=true;
 Engine::Sample initialSample;initialSample.rate=48000;initialSample.mono.resize(4096,0.2f);
 swapping.setSoundBank({initialSample},{});
 std::thread publishSamples([&]{
  for(int k=0;k<120;++k){
   Engine::Sample next;next.rate=48000;next.mono.resize(4096,0.1f+static_cast<float>(k%5)*0.1f);
   swapping.setSoundBank({std::move(next)},{});
   swapping.reclaimSoundBanks();
   std::this_thread::yield();
  }
 });
 for(int k=0;k<300;++k){swapping.render(l.data(),r.data(),512);for(float v:l)assert(std::isfinite(v));}
 publishSamples.join();
 swapping.render(l.data(),r.data(),512);swapping.reclaimSoundBanks();
 assert(swapping.sampleCount()==1);

 Engine editing;editing.seed(73);editing.prepare(48000);editing.activeTracks=2;editing.mode=1;editing.every=1;editing.playing=true;
 std::thread editPatterns([&]{
  for(int k=0;k<400;++k){
   auto phrase=editing.getTrackPhrase(1);
   phrase[static_cast<size_t>(k%Engine::steps)]=60+k%12;
   editing.setTrackPhrase(1,phrase);
   editing.generateTrack(0);
   editing.toggleStep(1,(k+1)%Engine::steps);
  }
 });
 for(int k=0;k<400;++k){editing.render(l.data(),r.data(),512);for(float v:l)assert(std::isfinite(v));}
 editPatterns.join();
 assert(editing.getTrackPhrase(0).size()==Engine::steps);

 Engine shortTail,longTail;shortTail.seed(501);longTail.seed(501);
 shortTail.prepare(48000);longTail.prepare(48000);
 shortTail.activeTracks=1;longTail.activeTracks=1;
 shortTail.render(l.data(),r.data(),512);longTail.render(l2.data(),r2.data(),512);
 std::array<int,Engine::steps> slowPhrase{};slowPhrase.fill(-1);slowPhrase[0]=60;
 shortTail.setTrackPhrase(0,slowPhrase);longTail.setTrackPhrase(0,slowPhrase);
 for(Engine* e:{&shortTail,&longTail}){
  e->bpm=40;e->sourceType[0]=4;e->noteLength[0]=0;e->level[0]=1;
  e->send[0]=0;e->wet=0;e->drive=0;e->weather[0]=0;e->playing=true;
 }
 longTail.tail[0]=1.0f;
 double shortEnergy=0,longEnergy=0;
 for(int k=0;k<110;++k){
  shortTail.render(l.data(),r.data(),512);longTail.render(l2.data(),r2.data(),512);
  if(k>=80)for(int s=0;s<512;++s){shortEnergy+=l[s]*l[s];longEnergy+=l2[s]*l2[s];}
 }
 assert(longEnergy>shortEnergy*10.0);
 assert(shortTail.getTrackPhrase(0)==longTail.getTrackPhrase(0));

 Engine browsing;browsing.seed(505);browsing.prepare(48000);browsing.activeTracks=1;
 browsing.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> browsePhrase{};browsePhrase.fill(60);
 browsing.setTrackPhrase(0,browsePhrase);
 browsing.bpm=120;browsing.sourceType[0]=4;browsing.level[0]=1;
 browsing.send[0]=0;browsing.wet=0;browsing.drive=0;browsing.playing=true;
 for(int k=0;k<24;++k)browsing.render(l.data(),r.data(),512);
 const int beforeBrowse=browsing.playhead.load();
 browsing.browseGain[0]=0;
 for(int k=0;k<24;++k)browsing.render(l.data(),r.data(),512);
 assert(browsing.playhead.load()!=beforeBrowse);
 for(float sample:l)assert(std::abs(sample)<0.001f);
 browsing.sourceType[0]=5;browsing.soundRevision[0].fetch_add(1);
 browsing.browseGain[0]=1;
 bool heardAfterBrowse=false;
 for(int k=0;k<30;++k){browsing.render(l.data(),r.data(),512);for(float sample:l)if(std::abs(sample)>0.001f)heardAfterBrowse=true;}
 assert(heardAfterBrowse);
 assert(browsing.getTrackPhrase(0)==browsePhrase);

 Engine verySlow;verySlow.seed(502);verySlow.prepare(48000);verySlow.activeTracks=1;
 verySlow.render(l.data(),r.data(),512);verySlow.bpm=10;verySlow.playing=true;
 for(int k=0;k<140;++k)verySlow.render(l.data(),r.data(),512);
 assert(verySlow.playhead.load()==0);
 verySlow.render(l.data(),r.data(),512);
 assert(verySlow.playhead.load()==1);

 Engine detached,joined;detached.seed(503);joined.seed(503);
 detached.prepare(48000);joined.prepare(48000);
 detached.activeTracks=1;joined.activeTracks=1;
 detached.render(l.data(),r.data(),512);joined.render(l2.data(),r2.data(),512);
 std::array<int,Engine::steps> legatoPhrase{};legatoPhrase.fill(-1);legatoPhrase[0]=60;legatoPhrase[1]=64;
 detached.setTrackPhrase(0,legatoPhrase);joined.setTrackPhrase(0,legatoPhrase);
 for(Engine* e:{&detached,&joined}){
  e->bpm=10;e->sourceType[0]=4;e->material[0]=2;e->noteLength[0]=0;
  e->articulation[0]=0;e->move[0]=0;e->weather[0]=0;
  e->level[0]=1;e->send[0]=0;e->wet=0;e->drive=0;e->playing=true;
 }
 joined.connect[0]=true;
 double detachedGapEnergy=0,joinedGapEnergy=0,joinedReleaseEnergy=0;
 for(int k=0;k<310;++k){
  detached.render(l.data(),r.data(),512);joined.render(l2.data(),r2.data(),512);
  for(float sample:l2){assert(std::isfinite(sample));assert(std::abs(sample)<=1);}
  if(k>=120&&k<140)for(int s=0;s<512;++s){detachedGapEnergy+=l[s]*l[s];joinedGapEnergy+=l2[s]*l2[s];}
  if(k>=290)for(float sample:l2)joinedReleaseEnergy+=sample*sample;
 }
 assert(joinedGapEnergy>detachedGapEnergy*20.0);
 assert(joinedReleaseEnergy<joinedGapEnergy*0.01);
 assert(detached.getTrackPhrase(0)==joined.getTrackPhrase(0));

 Engine restartedSample,continuedSample;restartedSample.seed(504);continuedSample.seed(504);
 restartedSample.prepare(48000);continuedSample.prepare(48000);
 restartedSample.activeTracks=1;continuedSample.activeTracks=1;
 restartedSample.render(l.data(),r.data(),512);continuedSample.render(l2.data(),r2.data(),512);
 restartedSample.setTrackPhrase(0,legatoPhrase);continuedSample.setTrackPhrase(0,legatoPhrase);
 Engine::Sample longSample;longSample.rate=48000;longSample.mono.resize(48000,0.8f);
 std::fill(longSample.mono.begin(),longSample.mono.begin()+6000,0.0f);
 restartedSample.setSampleLibrary({longSample});continuedSample.setSampleLibrary({longSample});
 for(Engine* e:{&restartedSample,&continuedSample}){
  e->bpm=60;e->sourceType[0]=8;e->sampleChoice[0]=0;e->sampleFade[0]=0;
  e->noteLength[0]=3;e->level[0]=1;e->send[0]=0;e->wet=0;e->drive=0;e->playing=true;
 }
 continuedSample.connect[0]=true;
 double restartedAfterNote=0,continuedAfterNote=0;
 for(int k=0;k<28;++k){
  restartedSample.render(l.data(),r.data(),512);continuedSample.render(l2.data(),r2.data(),512);
  if(k>=24)for(int s=0;s<512;++s){restartedAfterNote+=l[s]*l[s];continuedAfterNote+=l2[s]*l2[s];}
 }
 assert(continuedAfterNote>restartedAfterNote+0.01);

 Engine expressive;expressive.seed(231);expressive.prepare(48000);expressive.activeTracks=1;expressive.dynamics[0]=1.0f;expressive.articulation[0]=1.0f;expressive.playing=true;
 bool expressiveHeard=false;
 for(int k=0;k<300;++k){expressive.render(l.data(),r.data(),512);for(float v:l){assert(std::isfinite(v));assert(std::abs(v)<=1);if(std::abs(v)>0.001f)expressiveHeard=true;}}
 assert(expressiveHeard);

 for(int src=0;src<14;++src){
  if(src==8||src==13)continue;
  Engine e;e.seed(100+src);e.prepare(48000);e.activeTracks=1;for(auto& s:e.sourceType)s=src;e.playing=true;
  bool heard=false;
  for(int k=0;k<300;++k){e.render(l.data(),r.data(),512);for(float v:l){assert(std::isfinite(v));assert(std::abs(v)<=1);if(std::abs(v)>0.001f)heard=true;}}
  assert(heard);
 }

 Engine sampled;sampled.seed(300);sampled.prepare(48000);sampled.activeTracks=1;
 Engine::Sample sample;sample.rate=48000;sample.mono.resize(24000);
 for(size_t i=0;i<sample.mono.size();++i)sample.mono[i]=static_cast<float>(std::sin(static_cast<double>(i)*0.04));
 std::vector<Engine::Sample> samples;samples.push_back(std::move(sample));sampled.setSampleLibrary(std::move(samples));
 for(auto& s:sampled.sourceType)s=8;for(auto& n:sampled.noteLength)n=3;sampled.playing=true;
 bool sampleHeard=false;
	 for(int k=0;k<300;++k){sampled.render(l.data(),r.data(),512);for(float v:l){assert(std::isfinite(v));assert(std::abs(v)<=1);if(std::abs(v)>0.001f)sampleHeard=true;}}
	 assert(sampleHeard);

	 Engine bed;bed.seed(305);bed.prepare(48000);bed.activeTracks=1;bed.render(l.data(),r.data(),512);
	 Engine::Sample bedSample;bedSample.rate=48000;bedSample.mono.resize(2048);
	 for(size_t i=0;i<bedSample.mono.size();++i)bedSample.mono[i]=static_cast<float>(std::sin(static_cast<double>(i)*0.08));
	 std::vector<Engine::Sample> bedSamples;bedSamples.push_back(std::move(bedSample));bed.setSampleLibrary(std::move(bedSamples));
	 std::array<int,Engine::steps> bedPhrase{};bedPhrase.fill(-1);bedPhrase[0]=60;bed.setTrackPhrase(0,bedPhrase);
	 bed.sourceType[0]=8;bed.sampleChoice[0]=0;bed.samplePlayMode[0]=2;bed.noteLength[0]=3;bed.playing=true;
	 bool bedStillHeard=false;
	 for(int k=0;k<80;++k){bed.render(l.data(),r.data(),512);if(k>20)for(float v:l){assert(std::isfinite(v));assert(std::abs(v)<=1);if(std::abs(v)>0.001f)bedStillHeard=true;}}
	 assert(bedStillHeard);

	 Engine shapedSample;shapedSample.seed(306);shapedSample.prepare(48000);shapedSample.activeTracks=1;
	 Engine::Sample shapedSource;shapedSource.rate=48000;shapedSource.mono.resize(48000);
	 for(size_t i=0;i<shapedSource.mono.size();++i)shapedSource.mono[i]=static_cast<float>(std::sin(static_cast<double>(i)*0.03));
	 std::vector<Engine::Sample> shapedSources;shapedSources.push_back(std::move(shapedSource));shapedSample.setSampleLibrary(std::move(shapedSources));
	 shapedSample.sourceType[0]=8;shapedSample.sampleChoice[0]=0;shapedSample.samplePlayMode[0]=1;shapedSample.sampleStart[0]=0.25f;shapedSample.sampleSize[0]=0.25f;shapedSample.sampleFade[0]=0.1f;shapedSample.sampleReverse[0]=true;shapedSample.noteLength[0]=3;shapedSample.playing=true;
	 bool shapedSampleHeard=false;
	 for(int k=0;k<80;++k){shapedSample.render(l.data(),r.data(),512);for(float v:l){assert(std::isfinite(v));assert(std::abs(v)<=1);if(std::abs(v)>0.001f)shapedSampleHeard=true;}}
	 assert(shapedSampleHeard);

 Engine mapped;mapped.seed(301);mapped.prepare(48000);mapped.activeTracks=1;
 Engine::Sample low;low.rate=48000;low.mono.resize(24000);
 Engine::Sample high;high.rate=48000;high.mono.resize(24000);
 for(size_t i=0;i<low.mono.size();++i){low.mono[i]=static_cast<float>(std::sin(static_cast<double>(i)*0.025));high.mono[i]=static_cast<float>(std::sin(static_cast<double>(i)*0.08));}
 std::vector<Engine::Sample> mappedSamples;mappedSamples.push_back(std::move(low));mappedSamples.push_back(std::move(high));mapped.setSampleLibrary(std::move(mappedSamples));
 Engine::MappedInstrument instrument;instrument.zones.push_back({0,48,24,55});instrument.zones.push_back({1,67,56,96});
 std::vector<Engine::MappedInstrument> instruments;instruments.push_back(std::move(instrument));mapped.setMappedInstruments(std::move(instruments));
 for(auto& s:mapped.sourceType)s=13;for(auto& n:mapped.noteLength)n=2;mapped.playing=true;
 bool mappedHeard=false;
 for(int k=0;k<300;++k){mapped.render(l.data(),r.data(),512);for(float v:l){assert(std::isfinite(v));assert(std::abs(v)<=1);if(std::abs(v)>0.001f)mappedHeard=true;}}
 assert(mappedHeard);

 Engine quietMapped,boostedMapped;
 std::array<int,Engine::steps> sustained{};sustained.fill(60);
 for(auto* e:{&quietMapped,&boostedMapped}){
  e->seed(302);e->prepare(48000);e->activeTracks=1;
  e->render(l.data(),r.data(),512);
  e->setTrackPhrase(0,sustained);
  Engine::Sample source;source.rate=48000;source.mono.resize(48000);
  for(size_t i=0;i<source.mono.size();++i)source.mono[i]=0.01f*static_cast<float>(std::sin(static_cast<double>(i)*0.03));
  Engine::MappedInstrument voice;voice.zones.push_back({0,60,24,96});voice.gain=e==&boostedMapped?8.0f:1.0f;
  e->setSoundBank({std::move(source)},{std::move(voice)});
  e->sourceType[0]=13;e->instrumentChoice[0]=0;e->noteLength[0]=3;e->level[0]=0.5f;e->send[0]=0;e->wet=0;e->playing=true;
 }
 double quietEnergy=0,boostedEnergy=0;
 for(int k=0;k<80;++k){
  quietMapped.render(l.data(),r.data(),512);
  for(float v:l)quietEnergy+=static_cast<double>(v)*v;
  boostedMapped.render(l.data(),r.data(),512);
  for(float v:l)boostedEnergy+=static_cast<double>(v)*v;
 }
 assert(quietEnergy>0);
 assert(boostedEnergy>quietEnergy*30.0);

 Engine dryMix,passMix,trackMuted;
 std::array<int,Engine::steps> firstPhrase{},secondPhrase{};
 firstPhrase.fill(-1);secondPhrase.fill(-1);firstPhrase[0]=60;secondPhrase[0]=67;
 for(auto* e:{&dryMix,&passMix,&trackMuted}){
  e->seed(733);e->prepare(48000);e->activeTracks=2;
  e->setTrackPhrase(0,firstPhrase);e->setTrackPhrase(1,secondPhrase);e->playing=true;
 }
 std::array<float,512> passLeft{},passRight{},mutedLeft{},mutedRight{};
 int processedTracks[2]{};
 bool changedOneTrack=false;
 for(int k=0;k<30;++k){
  dryMix.render(l.data(),r.data(),512);
  passMix.render(passLeft.data(),passRight.data(),512,nullptr,
   [](void* context,int track,float*,float*,int,const Engine::MidiEvents&){++static_cast<int*>(context)[track];},processedTracks);
  trackMuted.render(mutedLeft.data(),mutedRight.data(),512,nullptr,
   [](void*,int track,float* left,float* right,int count,const Engine::MidiEvents&){
    if(track==0)for(int i=0;i<count;++i){left[i]=0;right[i]=0;}
   });
  for(int i=0;i<512;++i){
   assert(std::abs(l[i]-passLeft[i])<0.00001f);
   assert(std::abs(r[i]-passRight[i])<0.00001f);
   if(std::abs(l[i]-mutedLeft[i])>0.0001f)changedOneTrack=true;
  }
 }
 assert(processedTracks[0]==30&&processedTracks[1]==30&&changedOneTrack);

 Engine clonedPattern;
 clonedPattern.activeTracks=2;
 clonedPattern.setTrackPhrase(0,firstPhrase);
 clonedPattern.copyTrackData(0,1);
 assert(clonedPattern.getTrackPhrase(1)==firstPhrase);
 clonedPattern.toggleStep(1,1);
 assert(clonedPattern.getTrackPhrase(0)==firstPhrase);
 assert(clonedPattern.getTrackPhrase(1)!=firstPhrase);

 Engine mutedBase,mutedWithEffect;
 for(auto* e:{&mutedBase,&mutedWithEffect}){
  e->seed(734);e->prepare(48000);e->activeTracks=2;
  e->setTrackPhrase(0,firstPhrase);e->setTrackPhrase(1,secondPhrase);
  e->mute[1]=true;e->playing=true;
 }
 for(int k=0;k<10;++k){
  mutedBase.render(l.data(),r.data(),512);
  mutedWithEffect.render(passLeft.data(),passRight.data(),512,nullptr,
   [](void*,int track,float* left,float* right,int count,const Engine::MidiEvents&){
    if(track==1)for(int i=0;i<count;++i){left[i]=0.4f;right[i]=0.4f;}
   });
  for(int i=0;i<512;++i){
   assert(std::abs(l[i]-passLeft[i])<0.00001f);
   assert(std::abs(r[i]-passRight[i])<0.00001f);
  }
 }

 Engine hosted;
 hosted.prepare(48000);hosted.activeTracks=1;hosted.hostedInstrumentMask=1;hosted.playing=true;
 hosted.setTrackPhrase(0,firstPhrase);
 Engine::MidiEvents hostedEvents;
 struct HostedProbe {bool sawNote=false,sawDryAudio=false;} hostedProbe;
 hosted.render(l.data(),r.data(),512,&hostedEvents,
  [](void* context,int track,float* left,float* right,int count,const Engine::MidiEvents& events){
   auto& probe=*static_cast<HostedProbe*>(context);
   assert(track==0);
   for(int i=0;i<count;++i)probe.sawDryAudio=probe.sawDryAudio||left[i]!=0.0f||right[i]!=0.0f;
   for(int i=0;i<events.count;++i)if(events.items[static_cast<size_t>(i)].track==track&&events.items[static_cast<size_t>(i)].on)probe.sawNote=true;
   std::fill_n(left,count,0.2f);std::fill_n(right,count,0.2f);
  },&hostedProbe);
 assert(hostedProbe.sawNote&&!hostedProbe.sawDryAudio);
 assert(hostedEvents.count>0&&hostedEvents.items[0].on);
 assert(std::any_of(l.begin(),l.end(),[](float sample){return sample>0.01f;}));
 hosted.playing=false;
 hosted.render(l.data(),r.data(),512,&hostedEvents);
 assert(std::any_of(hostedEvents.items.begin(),hostedEvents.items.begin()+hostedEvents.count,[](const Engine::MidiEvent& event){return !event.on;}));

 Engine drone;drone.seed(2027);drone.prepare(8000);drone.activeTracks=1;drone.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> dronePhrase;dronePhrase.fill(48);drone.setTrackPhrase(0,dronePhrase);
 drone.sourceType[0]=4;drone.connect[0]=true;drone.noteLength[0]=3;
 drone.midiOutputTrack=0;drone.bpm=240;drone.playing=true;
 int droneNoteOns=0,droneNoteOffs=0;double lateDroneEnergy=0;
 Engine::MidiEvents droneEvents;
 for(int block=0;block<80;++block){
  drone.render(l.data(),r.data(),512,&droneEvents);
  for(int event=0;event<droneEvents.count;++event)
   if(droneEvents.items[static_cast<size_t>(event)].on)++droneNoteOns;else ++droneNoteOffs;
  if(block>=60)for(float sample:l)lateDroneEnergy+=sample*sample;
 }
 assert(droneNoteOns==1&&droneNoteOffs==0&&lateDroneEnergy>0.01);

 Engine mixedLoops;mixedLoops.seed(2029);mixedLoops.prepare(8000);mixedLoops.activeTracks=2;
 mixedLoops.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> shortPhrase{},longPhrase{};
 shortPhrase.fill(-1);longPhrase.fill(-1);
 shortPhrase[0]=60;shortPhrase[4]=64;shortPhrase[12]=79;
 longPhrase[0]=72;longPhrase[5]=74;longPhrase[15]=76;
 mixedLoops.setLoopSteps(0,5);mixedLoops.setTrackPhrase(0,shortPhrase);
 mixedLoops.setTrackPhrase(1,longPhrase);
 mixedLoops.toggleStep(0,12);
 assert(mixedLoops.getTrackPhrase(0)[12]==79);
 mixedLoops.midiOutputTrack=-1;mixedLoops.bpm=240;mixedLoops.playing=true;
 std::vector<std::pair<int,int>> shortNotes,longNotes;
 Engine::MidiEvents loopEvents;
 for(int sample=0;sample<8500;++sample){
  mixedLoops.render(l.data(),r.data(),1,&loopEvents);
  for(int event=0;event<loopEvents.count;++event){
   const auto& item=loopEvents.items[static_cast<size_t>(event)];
   if(item.on)(item.track==0?shortNotes:longNotes).emplace_back(mixedLoops.playhead.load(),item.note);
  }
 }
 assert((shortNotes==std::vector<std::pair<int,int>>{{0,60},{4,64},{5,60},{9,64},{10,60},{14,64},{15,60}}));
 assert((longNotes==std::vector<std::pair<int,int>>{{0,72},{5,74},{15,76},{0,72}}));
 assert(mixedLoops.playhead.load()==0&&mixedLoops.lanePlayheads[0].load()==1);
 mixedLoops.setLoopSteps(0,16);
 assert(mixedLoops.getTrackPhrase(0)[12]==79);
 mixedLoops.setLoopSteps(0,5);
 mixedLoops.copyTrackData(0,1);
 assert(mixedLoops.loopSteps[1].load()==5);
 mixedLoops.clearTrack(0);
 assert(mixedLoops.loopSteps[0].load()==16);

 Engine longLoop;longLoop.seed(2032);longLoop.prepare(8000);longLoop.activeTracks=1;
 longLoop.render(l.data(),r.data(),512);
 std::array<int,Engine::maxLoopSteps> fourPages{};fourPages.fill(-1);
 fourPages[0]=60;fourPages[16]=62;fourPages[32]=64;fourPages[63]=67;
 longLoop.setLoopSteps(0,64);longLoop.setTrackPattern(0,fourPages);
 assert(longLoop.getTrackPhrase(0)[0]==60&&longLoop.getTrackPattern(0)[63]==67);
 assert(longLoop.pageDisplay[3*Engine::steps+15].load()==67);
 longLoop.midiOutputTrack=0;longLoop.bpm=240;longLoop.playing=true;
 std::vector<std::pair<int,int>> pageNotes;
 for(int sample=0;sample<32500;++sample){
  longLoop.render(l.data(),r.data(),1,&loopEvents);
  for(int event=0;event<loopEvents.count;++event){
   const auto& item=loopEvents.items[static_cast<size_t>(event)];
   if(item.on)pageNotes.emplace_back(longLoop.lanePlayheads[0].load(),item.note);
  }
 }
 assert((pageNotes==std::vector<std::pair<int,int>>{{0,60},{16,62},{32,64},{63,67},{0,60}}));
 longLoop.setLoopSteps(0,12);assert(longLoop.getTrackPattern(0)[63]==67);
 longLoop.setLoopSteps(0,64);longLoop.copyTrackData(0,1);
 assert(longLoop.loopSteps[1].load()==64&&longLoop.getTrackPattern(1)==fourPages);
 longLoop.generateTrack(0);
 const auto generatedLongLoop=longLoop.getTrackPattern(0);
 assert(std::any_of(generatedLongLoop.begin()+16,generatedLongLoop.end(),[](int n){return n>=0;}));

 Engine oneStep;oneStep.seed(2031);oneStep.activeTracks=1;oneStep.setLoopSteps(0,1);
 for(int trial=0;trial<20;++trial){oneStep.generateTrack(0);assert(oneStep.getTrackPhrase(0)[0]>=0);}

 Engine shortDrone;shortDrone.seed(2030);shortDrone.prepare(8000);shortDrone.activeTracks=1;
 shortDrone.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> shortDronePhrase{};shortDronePhrase.fill(-1);
 std::fill(shortDronePhrase.begin(),shortDronePhrase.begin()+5,48);
 shortDrone.setLoopSteps(0,5);shortDrone.setTrackPhrase(0,shortDronePhrase);
 shortDrone.connect[0]=true;shortDrone.midiOutputTrack=0;shortDrone.bpm=240;shortDrone.playing=true;
 int shortDroneOns=0,shortDroneOffs=0;
 for(int block=0;block<20;++block){
  shortDrone.render(l.data(),r.data(),512,&loopEvents);
  for(int event=0;event<loopEvents.count;++event)
   if(loopEvents.items[static_cast<size_t>(event)].on)++shortDroneOns;else ++shortDroneOffs;
 }
 assert(shortDroneOns==1&&shortDroneOffs==0);

 Engine wanderingDrone;wanderingDrone.seed(2028);wanderingDrone.prepare(8000);wanderingDrone.activeTracks=3;
 wanderingDrone.render(l.data(),r.data(),512);
 std::array<int,Engine::steps> anchor,upper,resting;
 anchor.fill(48);upper.fill(55);resting.fill(-1);resting[0]=60;resting[8]=67;
 wanderingDrone.setTrackPhrase(0,anchor);wanderingDrone.setTrackPhrase(1,upper);wanderingDrone.setTrackPhrase(2,resting);
 wanderingDrone.connect[0]=true;wanderingDrone.connect[1]=true;
 wanderingDrone.mute[2]=true;wanderingDrone.hold[2]=true;
 wanderingDrone.mode=1;wanderingDrone.every=1;wanderingDrone.bpm=240;wanderingDrone.playing=true;
 bool upperMoved=false;
 double wanderingEnergy=0;
 for(int block=0;block<100;++block){
  wanderingDrone.render(l.data(),r.data(),512);
  for(float sample:l){assert(std::isfinite(sample)&&std::abs(sample)<=1.0f);wanderingEnergy+=sample*sample;}
  const auto current=wanderingDrone.getTrackPhrase(1);
  upperMoved=upperMoved||current[0]!=55;
  assert(std::all_of(current.begin(),current.end(),[note=current[0]](int value){return value==note;}));
 }
 assert(upperMoved&&wanderingEnergy>0.01);
 assert(wanderingDrone.getTrackPhrase(0)==anchor);
 assert(wanderingDrone.getTrackPhrase(2)==resting);

	 std::cout<<"Engine checks passed: empty start, independent tracks, follower regeneration, linked drift, click toggles, shaped notes, sound browsing during playback, hold, evolve, solo audition, Focus restore, timed fades, silent pattern clock, live sample swap, concurrent pattern edits, slow tail, 10 BPM timing, connected notes, independent track loops, continuous and wandering drone, sample bed, sample shape, track effect stems, hosted MIDI source, audio bounds, stop\n";
}
