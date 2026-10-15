#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

// Audio-thread-owned musical state. UI communicates through atomics only.
class Engine {
public:
    static constexpr int maxTracks=8;
    static constexpr int steps=16;
    struct Sample {
        std::vector<float> mono;
        double rate=44100.0;
    };
    struct SampleZone {
        int sampleIndex=0;
        int rootNote=60;
        int lowNote=0;
        int highNote=127;
    };
    struct MappedInstrument {
        std::vector<SampleZone> zones;
    };
    std::atomic<float> bpm{96}, density{0.65f}, room{0.6f}, wet{0.25f}, drive{0.18f}, filter{0.72f};
    std::atomic<int> key{2}, scale{0}, shape{0}, mode{0}, every{4}, octaves{2};
    std::atomic<int> activeTracks{0};
    std::atomic<int> soloTrack{-1};
    std::atomic<bool> playing{false};
    std::array<std::atomic<bool>,maxTracks> hold{}, mute{}, connect{};
    std::array<std::atomic<int>,maxTracks> sourceType, material, noteLength, sampleChoice, instrumentChoice, samplePlayMode, role;
    std::array<std::atomic<float>,maxTracks> level, send, color, move, weather, relationship, dynamics, articulation, tail, sampleStart, sampleSize, sampleFade;
    std::array<std::atomic<float>,maxTracks> browseGain;
    std::array<std::atomic<unsigned>,maxTracks> soundRevision;
    std::array<std::atomic<bool>,maxTracks> sampleReverse;
    std::array<std::atomic<int>,maxTracks*steps> display;
    std::atomic<int> playhead{-1};
    std::atomic<unsigned> request{1};
    std::atomic<bool> refreshFollowers{false};
    Engine() {
        for(int i=0;i<maxTracks;++i){
            sourceType[i].store(i%4);
            level[i].store(0.65f);
            browseGain[i].store(1.0f);
            soundRevision[i].store(0);
            send[i].store(i%4==3?0.65f:0.25f);
            color[i].store(i%4==2?0.78f:0.55f);
            move[i].store(i%4==3?0.72f:0.35f);
            weather[i].store(i%4==3?0.64f:0.32f);
            relationship[i].store(i==0?0.0f:0.35f+0.12f*static_cast<float>(i%3));
            dynamics[i].store(i==0?0.18f:0.28f+0.08f*static_cast<float>(i%4));
            articulation[i].store(i==0?0.42f:(i==1?0.68f:0.52f));
            tail[i].store(0.0f);
            role[i].store(i==0?0:(i%4));
            material[i].store(i%4==2?5:(i%4==3?3:i%4));
            noteLength[i].store(i==0?1:(i==1?0:2));
            sampleChoice[i].store(-1);
            instrumentChoice[i].store(0);
            samplePlayMode[i].store(0);
            sampleStart[i].store(0.0f);
            sampleSize[i].store(1.0f);
            sampleFade[i].store(0.05f);
            sampleReverse[i].store(false);
            connect[i].store(false);
        }
        for(auto& n:display)n.store(-1);
        for(auto& p:parts)p.fill(-1);
    }
    void prepare(double rate) {
        sampleRate=rate; samplesLeft=0; position=-1; cycles=0;
        for(int c=0;c<maxTracks;++c){voices[c]=Voice{};lastSoundRevision[c]=soundRevision[c].load();}
        mixGains.fill(0.0f);lastSoloTrack=-1;
        delay.fill(0); delayIndex=0; running=false;
    }
    void seed(uint32_t value){rng.store(value?value:1);}
    uint32_t randomState() const {return rng.load();}
    void restartFromBeginning(){restartRequested.store(true);}
    void setSampleLibrary(std::vector<Sample> samples){sampleLibrary=std::move(samples);}
    void setMappedInstruments(std::vector<MappedInstrument> instruments){mappedInstruments=std::move(instruments);}
    size_t sampleCount() const {return sampleLibrary.size();}
    std::array<int,steps> getPhrase() const {return parts[0];}
    void setPhrase(const std::array<int,steps>& phrase) {
        parts[0]=phrase;
        refreshDisplay();
        request.fetch_add(1);
    }
    std::array<int,steps> getTrackPhrase(int track) const {
        return parts[static_cast<size_t>(std::clamp(track,0,maxTracks-1))];
    }
    void setTrackPhrase(int track,const std::array<int,steps>& phrase) {
        parts[static_cast<size_t>(std::clamp(track,0,maxTracks-1))]=phrase;
        refreshDisplay();
    }
    void copyTrackData(int from,int to) {
        parts[static_cast<size_t>(std::clamp(to,0,maxTracks-1))]=parts[static_cast<size_t>(std::clamp(from,0,maxTracks-1))];
        refreshDisplay();
    }
    void clearTrack(int track) {
        parts[static_cast<size_t>(std::clamp(track,0,maxTracks-1))].fill(-1);
        refreshDisplay();
    }
    void generateTrack(int track) {
        generateTrackInternal(std::clamp(track,0,maxTracks-1));
        refreshDisplay();
    }
    int getStep(int track,int step) const {
        track=std::clamp(track,0,maxTracks-1);
        step=std::clamp(step,0,steps-1);
        return parts[track][step];
    }
    void toggleStep(int track,int step) {
        track=std::clamp(track,0,maxTracks-1);
        step=std::clamp(step,0,steps-1);
        parts[track][step]=parts[track][step]<0?noteForStep(step,previousTrackNote(track,step)):-1;
        if(std::all_of(parts[track].begin(),parts[track].end(),[](int n){return n<0;}))parts[track][step]=noteForStep(step,-1);
        display[track*steps+step].store(parts[track][step]);
    }
    void render(float* left,float* right,int count) {
        const bool restart=restartRequested.exchange(false);
        if(restart){
            clearAudioState();
            delayIndex=0;position=-1;cycles=0;samplesLeft=0;running=false;
            playhead.store(-1);
            for(int c=0;c<maxTracks;++c)lastSoundRevision[c]=soundRevision[c].load();
            lastRequest=request.load();
        }
        const unsigned r=request.load();
        if(r!=lastRequest){lastRequest=r;generate();}
        if(refreshFollowers.exchange(false))refreshDisplay();
        const bool run=playing.load();
        if(run&&!running){position=-1;samplesLeft=0;cycles=0;}
        running=run;
        if(!run)playhead.store(-1);
        std::array<float,maxTracks> gains{},sends{},colors{},moves{},weathers{};
        std::array<int,maxTracks> sources{},materials{};
        bool anyAudibleLane=false;
        const int trackCount=std::clamp(activeTracks.load(),0,maxTracks);
        const int solo=soloTrack.load();
        if(solo!=lastSoloTrack){
            if(solo>=0){delay.fill(0.0f);filterState=0.0f;}
            lastSoloTrack=solo;
        }
        for(int c=0;c<trackCount;++c){
            gains[c]=(solo>=0?c==solo:!mute[c].load())?level[c].load():0.0f;
            sends[c]=send[c].load();
            colors[c]=std::clamp(color[c].load(),0.f,1.f);
            moves[c]=std::clamp(move[c].load(),0.f,1.f);
            weathers[c]=std::clamp(weather[c].load(),0.f,1.f);
            sources[c]=std::clamp(sourceType[c].load(),0,13);
            materials[c]=std::clamp(material[c].load(),0,6);
            const unsigned revision=soundRevision[c].load();
            if(revision!=lastSoundRevision[c]){
                lastSoundRevision[c]=revision;
                voices[c]=Voice{};
            }
            if(!connect[c].load())voices[c].connectHold=false;
            anyAudibleLane=anyAudibleLane||gains[c]>0.0001f;
            gains[c]*=std::clamp(browseGain[c].load(),0.0f,1.0f);
        }
        if(!run||!anyAudibleLane){
            clearAudioState();
            std::fill(left,left+count,0.0f);
            if(right)std::fill(right,right+count,0.0f);
            return;
        }
        const float tempo=std::clamp(bpm.load(),10.f,240.f);
        const double interval=sampleRate*60.0/tempo/4.0;
        const float feedback=0.15f+std::clamp(room.load(),0.f,1.f)*0.65f;
        const float returnLevel=std::clamp(wet.load(),0.f,1.f);
        const float driveAmount=1.0f+std::clamp(drive.load(),0.f,1.f)*5.0f;
        const float filterAmount=std::clamp(filter.load(),0.f,1.f);
        const float filterCoeff=0.015f+filterAmount*filterAmount*0.42f;
        const float gainSlew=1.0f-static_cast<float>(std::exp(-1.0/(std::max(1.0,sampleRate)*0.012)));
        for(int s=0;s<count;++s){
            if(run&&samplesLeft<=0){advance();samplesLeft+=interval;}
            if(run)samplesLeft-=1;
            float dry=0, bus=0, leftMix=0, rightMix=0;
            if(run)for(int c=0;c<trackCount;++c){auto& v=voices[c];
                float tone=0;
                const int src=sources[c];
                const int mat=materials[c];
                const float bright=materialBrightness(mat);
                const float body=materialBody(mat);
                const float grit=materialGrit(mat);
                const float air=materialAir(mat);
                const float rough=materialRough(mat);
                const float softness=materialSoftness(mat);
                v.wanderPhase+=v.wanderIncrement*(0.4+moves[c]*1.8+weathers[c]*1.2);
                if(v.wanderPhase>6.283185307)v.wanderPhase-=6.283185307;
                const float wander=static_cast<float>(std::sin(v.wanderPhase));
                v.noise=v.noise*1664525u+1013904223u;
                const float materialNoise=static_cast<float>((v.noise>>16)&0xffff)/32768.f-1.f;
                if((src==8||src==13)&&!sampleLibrary.empty()){
                    const auto& sample=sampleLibrary[static_cast<size_t>(std::clamp(v.sampleIndex,0,static_cast<int>(sampleLibrary.size())-1))];
                    if(v.sampleActive&&sample.mono.size()>1){
                        const double lower=static_cast<double>(std::max(0,v.sampleStartFrame));
                        const double upper=static_cast<double>(std::max(v.sampleStartFrame+2,v.sampleEndFrame));
                        const bool inside=v.samplePosition>=lower&&v.samplePosition+1.0<upper;
                        if(!inside){
                            if(src==8&&samplePlayMode[c].load()==2){
                                v.samplePosition=v.sampleReverse?upper-2.0:lower;
                            }else{
                                v.sampleActive=false;
                                v.envelope=0;
                            }
                        }else{
                            const auto i=static_cast<size_t>(v.samplePosition);
                            if(i+1<sample.mono.size()){
                                const float frac=static_cast<float>(v.samplePosition-static_cast<double>(i));
                                const float a=sample.mono[i];
                                const float b=sample.mono[i+1];
                                const double edge=std::min(v.samplePosition-lower,upper-v.samplePosition);
                                const float fadeGain=v.sampleFadeSamples>1.0?static_cast<float>(std::clamp(edge/v.sampleFadeSamples,0.0,1.0)):1.0f;
                                tone=(a+(b-a)*frac)*v.sampleLevel*v.envelope*fadeGain*(0.72f+colors[c]*0.52f);
                                v.samplePosition+=v.sampleReverse?-v.sampleIncrement:v.sampleIncrement;
                            }else{
                                v.sampleActive=false;
                                v.envelope=0;
                            }
                        }
                    }
                }else if(src==2){
                    float ring=0;
                    for(int h=0;h<4;++h){
                        ring+=std::sin(v.resonatorPhase[h])*v.resonatorLevel[h];
                        v.resonatorPhase[h]+=v.resonatorIncrement[h];
                        if(v.resonatorPhase[h]>6.283185307)v.resonatorPhase[h]-=6.283185307;
                        if(!v.connectHold)v.resonatorLevel[h]*=v.resonatorDecay[h];
                    }
                    const float strike=materialNoise*v.envelope;
                    tone=ring*(0.15f+colors[c]*0.38f)*bright+strike*(0.04f+colors[c]*0.12f+grit*0.18f);
                }else if(src==3){
                    const float hush=materialNoise;
                    const float slow=static_cast<float>(std::sin(v.phase)*0.38+std::sin(v.phase*(1.07+moves[c]*0.9)+1.7+wander*0.8)*0.34+std::sin(v.phase*0.497+0.4)*0.18);
                    const float shadow=static_cast<float>(std::sin(v.shadowPhase))*v.shadowLevel;
                    v.shadowPhase+=v.shadowIncrement;if(v.shadowPhase>6.283185307)v.shadowPhase-=6.283185307;v.shadowLevel*=v.shadowDecay;
                    tone=(slow*body+shadow*(0.08f+weathers[c]*0.18f)+hush*(0.03f+colors[c]*0.06f+air*0.24f+grit*0.10f))*v.envelope;
                }else if(src==4){
                    const float organ=static_cast<float>(std::sin(v.phase)*0.52+std::sin(v.phase*(2.0+rough*0.18+0.01*wander))*0.22*bright+std::sin(v.phase*(3.01+grit*0.6))*0.14);
                    tone=(organ+materialNoise*air*0.08f)*v.envelope*(0.55f+colors[c]*0.55f)*body;
                }else if(src==5){
                    const float breath=materialNoise;
                    const float reed=static_cast<float>(std::sin(v.phase+wander*0.04)+0.44*std::sin(v.phase*2.0+0.5)+0.24*std::sin(v.phase*4.0+1.2));
                    tone=(reed*(0.36f+colors[c]*0.34f)*body*bright+breath*(0.08f+colors[c]*0.10f+air*0.26f+grit*0.12f))*v.envelope;
                }else if(src==6){
                    const float dust=materialNoise;
                    const float shimmer=static_cast<float>(std::sin(v.phase*0.5)+std::sin(v.phase*(1.0+moves[c])*1.37+2.1))*0.18f;
                    tone=(dust*(0.10f+colors[c]*0.16f+grit*0.24f+air*0.14f)+shimmer*(0.35f+bright*0.55f))*v.envelope*(1.0f-softness*0.28f);
                }else if(src==7){
                    float bell=0;
                    for(int h=0;h<4;++h){
                        bell+=std::sin(v.resonatorPhase[h])*v.resonatorLevel[h];
                        v.resonatorPhase[h]+=v.resonatorIncrement[h];
                        if(v.resonatorPhase[h]>6.283185307)v.resonatorPhase[h]-=6.283185307;
                        if(!v.connectHold)v.resonatorLevel[h]*=v.resonatorDecay[h];
                    }
                    tone=bell*(0.16f+colors[c]*0.42f)*bright+materialNoise*grit*0.05f*v.envelope;
                }else if(src==9){
                    const float pick=std::clamp(v.strike*18.0f,0.0f,1.0f);
                    const float string=static_cast<float>(std::sin(v.phase)+0.42*std::sin(v.phase*2.01+0.15)+0.18*std::sin(v.phase*3.0+0.4));
                    const float bodyTone=static_cast<float>(std::sin(v.bodyPhase)*v.bodyLevel);
                    tone=(string*(0.44f+colors[c]*0.28f)*body+bodyTone*(0.18f+softness*0.32f)+materialNoise*(0.05f+grit*0.18f)*pick)*v.envelope;
                    v.bodyPhase+=v.bodyIncrement;if(v.bodyPhase>6.283185307)v.bodyPhase-=6.283185307;v.bodyLevel*=v.bodyDecay;
                    v.strike*=0.997f;
                }else if(src==10){
                    const float bow=static_cast<float>(std::tanh(std::sin(v.phase+wander*0.08)*1.7+0.46*std::sin(v.phase*2.0+0.4)+0.22*std::sin(v.phase*3.01+1.2)));
                    const float resin=materialNoise*(0.06f+colors[c]*0.06f+grit*0.16f);
                    const float swell=1.0f-std::clamp(v.strike*4.0f,0.0f,0.72f);
                    tone=(bow*(0.46f+colors[c]*0.35f)*bright+resin)*v.envelope*swell;
                    v.strike*=0.9994f;
                }else if(src==11){
                    const float sub=static_cast<float>(std::sin(v.phase*0.5));
                    const float growl=static_cast<float>(std::tanh(std::sin(v.phase)*1.4+0.35*std::sin(v.phase*2.0+wander*0.2)));
                    tone=(sub*(0.62f+softness*0.35f)+growl*(0.25f+colors[c]*0.22f)+materialNoise*grit*0.08f)*v.envelope*body;
                }else if(src==12){
                    const float hammer=std::clamp(v.strike*22.0f,0.0f,1.0f);
                    const float string=static_cast<float>(std::sin(v.phase)+0.46*std::sin(v.phase*2.01+0.18)+0.22*std::sin(v.phase*3.02+0.7)+0.10*std::sin(v.phase*5.97+1.4));
                    const float soundboard=static_cast<float>(std::sin(v.bodyPhase)*v.bodyLevel);
                    const float felt=materialNoise*(0.025f+grit*0.10f+air*0.04f)*hammer;
                    tone=(string*(0.34f+colors[c]*0.24f)*bright+soundboard*(0.22f+softness*0.24f)+felt)*v.envelope;
                    v.bodyPhase+=v.bodyIncrement;if(v.bodyPhase>6.283185307)v.bodyPhase-=6.283185307;v.bodyLevel*=v.bodyDecay;
                    v.strike*=0.9955f;
                }else{
                    const double wave=std::sin(v.phase+wander*0.015)+(src==0?(0.08+colors[c]*0.22+rough*0.22)*std::sin((2.0+rough*0.35)*v.phase):0.0)+(src==1?(0.10+colors[c]*0.18+grit*0.20)*std::sin((3.0+rough)*v.phase):0.0);
                    tone=(static_cast<float>(wave)*body+materialNoise*(air*0.12f+grit*0.08f))*v.envelope*(1.0f-softness*0.18f);
                }
                mixGains[c]+=(gains[c]-mixGains[c])*gainSlew;
                const float value=tone*mixGains[c]*0.16f;
                v.phase+=v.increment;if(v.phase>6.283185307)v.phase-=6.283185307;
                const float pan=std::clamp(v.pan+wander*weathers[c]*0.28f,-0.85f,0.85f);
                const float lGain=0.78f-pan*0.28f;
                const float rGain=0.78f+pan*0.28f;
                if(!v.connectHold)v.envelope*=v.decay;
                dry+=value;leftMix+=value*lGain;rightMix+=value*rGain;bus+=value*sends[c];
            }
            const float tail=delay[delayIndex];
            delay[delayIndex]=run?bus+tail*feedback:0;
            delayIndex=(delayIndex+1)%delay.size();
            filterState+=filterCoeff*((dry+tail*returnLevel)-filterState);
            const float out=std::tanh(filterState*driveAmount);
            const float monoOut=std::tanh(out+(tail*returnLevel*0.2f));
            left[s]=std::tanh((leftMix+monoOut*0.35f)*driveAmount);
            if(right)right[s]=std::tanh((rightMix+monoOut*0.35f)*driveAmount);
        }
    }
private:
    struct Voice{
        double phase=0,increment=0;
        float envelope=0,decay=0;
        uint32_t noise=0x12345678;
        double bodyPhase=0,bodyIncrement=0;
        float bodyLevel=0,bodyDecay=0,strike=0;
        int sampleIndex=0;
        double samplePosition=0,sampleIncrement=1;
        int sampleStartFrame=0,sampleEndFrame=0;
        double sampleFadeSamples=0;
        float sampleLevel=1;
        bool sampleReverse=false;
        bool sampleActive=false;
        bool connectHold=false;
        int source=-1,sampleRootNote=60;
        double noteHz=0;
        double wanderPhase=0,wanderIncrement=0.00002,shadowPhase=0,shadowIncrement=0;
        float shadowLevel=0,shadowDecay=0,pan=0;
        std::array<double,4> resonatorPhase{},resonatorIncrement{};
        std::array<float,4> resonatorLevel{},resonatorDecay{};
    };
    std::array<Voice,maxTracks> voices{};
    std::array<unsigned,maxTracks> lastSoundRevision{};
    std::array<float,maxTracks> mixGains{};
    int lastSoloTrack=-1;
    std::vector<Sample> sampleLibrary;
    std::vector<MappedInstrument> mappedInstruments;
    std::array<std::array<int,steps>,maxTracks> parts{};
    std::array<float,16384> delay{}; // Modest diffuse-space placeholder, not a finished reverb.
    size_t delayIndex=0;
    std::atomic<uint32_t> rng{0x8abc0123};
    std::atomic<bool> restartRequested{false};
    unsigned lastRequest=0;
    double sampleRate=48000,samplesLeft=0;
    float filterState=0;
    int position=-1,cycles=0;
    bool running=false;
    static float materialBrightness(int m){constexpr float v[]={0.72f,1.22f,1.45f,0.78f,1.05f,1.35f,0.92f};return v[std::clamp(m,0,6)];}
    static float materialBody(int m){constexpr float v[]={1.18f,0.88f,0.74f,0.95f,0.72f,0.82f,1.15f};return v[std::clamp(m,0,6)];}
    static float materialGrit(int m){constexpr float v[]={0.16f,0.24f,0.05f,0.34f,0.08f,0.22f,0.10f};return v[std::clamp(m,0,6)];}
    static float materialAir(int m){constexpr float v[]={0.10f,0.08f,0.16f,0.22f,0.42f,0.05f,0.12f};return v[std::clamp(m,0,6)];}
    static float materialRough(int m){constexpr float v[]={0.05f,0.62f,0.14f,0.38f,0.02f,0.48f,0.10f};return v[std::clamp(m,0,6)];}
    static float materialSoftness(int m){constexpr float v[]={0.30f,0.06f,0.02f,0.42f,0.70f,0.04f,0.18f};return v[std::clamp(m,0,6)];}
    void clearAudioState(){
        for(auto& v:voices)v=Voice{};
        mixGains.fill(0.0f);
        delay.fill(0);
        filterState=0;
    }
    uint32_t random(){
        auto previous=rng.load();
        for(;;){
            auto next=previous;
            next^=next<<13;next^=next>>17;next^=next<<5;
            if(rng.compare_exchange_weak(previous,next))return next;
        }
    }
    float chance(){return static_cast<float>(random()%10000)/10000.f;}
    int note(){return noteForStep(-1,-1);}
    int previousTrackNote(int track,int step) const {
        track=std::clamp(track,0,maxTracks-1);
        for(int offset=1;offset<steps;++offset){
            const int index=(step-offset+steps)%steps;
            if(parts[track][index]>=0)return parts[track][index];
        }
        return -1;
    }
    int nearestMainNote(int step) const {
        for(int offset=0;offset<steps;++offset){
            const int forward=(step+offset)%steps;
            if(parts[0][forward]>=0)return parts[0][forward];
            const int backward=(step-offset+steps)%steps;
            if(parts[0][backward]>=0)return parts[0][backward];
        }
        return -1;
    }
    int relatedNoteFromMain(int mainNote,int track,int step,float distance,int roleValue,int previous) {
        if(mainNote<0)return noteForStep(step,previous);
        int noteValue=mainNote;
        const float close=1.0f-distance;
        const bool bassRole=roleValue==1&&track>1;
        const int octaveBias=bassRole?-12:(track%3==0?12:0);
        if(chance()<0.42f+distance*0.28f)noteValue+=octaveBias;
        if(chance()<0.18f+distance*0.32f){
            static constexpr int closeIntervals[]={0,0,0,7,-5,12,-12};
            static constexpr int distantIntervals[]={0,2,-2,5,-5,7,-7,10,-10,12,-12,14,-14};
            const auto& intervals=distance<0.55f?closeIntervals:distantIntervals;
            const int count=distance<0.55f?static_cast<int>(sizeof(closeIntervals)/sizeof(closeIntervals[0])):static_cast<int>(sizeof(distantIntervals)/sizeof(distantIntervals[0]));
            noteValue+=intervals[random()%static_cast<uint32_t>(count)];
        }
        if(roleValue==2&&chance()<0.28f)noteValue-=12;
        if(roleValue==3&&chance()<0.36f)return noteForStep(step,previous);
        if(previous>=0&&chance()<0.45f*close){
            while(noteValue-previous>7)noteValue-=12;
            while(previous-noteValue>7)noteValue+=12;
        }
        return std::clamp(noteValue,24,88);
    }
    int noteForStep(int step,int previous){
        constexpr int pent[]={0,3,5,7,10},minor[]={0,2,3,5,7,8,10},major[]={0,2,4,5,7,9,11};
        const int sc=scale.load(),length=sc==0?5:7;
        const int* tones=sc==0?pent:(sc==1?minor:major);
        const int octaveCount=std::clamp(octaves.load(),1,2);
        std::array<std::pair<int,int>,14> candidates{};
        int candidateCount=0;
        for(int octave=0;octave<octaveCount;++octave)for(int degree=0;degree<length;++degree){
            const int value=48+std::clamp(key.load(),0,11)+tones[degree]+12*octave;
            int weight=10;
            if(previous>=0){
                const int interval=std::abs(value-previous);
                if(interval==0)weight+=shape.load()==4?18:5;
                if(interval<=2)weight+=shape.load()==4?12:8;
                if(interval<=5)weight+=shape.load()==5?0:10;
                if(interval>=7)weight+=shape.load()==5?18:-4;
                if(shape.load()==1)weight+=value>previous?12:-5;
                if(shape.load()==2)weight+=value<previous?12:-5;
                if(shape.load()==3&&step>=0)weight+=(step<steps/2)==(value>previous)?12:-4;
            }
            if(step==0||step==4||step==8||step==12||step==steps-1){
                const int relative=(value-std::clamp(key.load(),0,11)+120)%12;
                if(relative==0)weight+=step==steps-1?18:10;
                if(relative==7)weight+=8;
            }
            candidates[static_cast<size_t>(candidateCount++)]={value,std::max(1,weight)};
        }
        int total=0;for(int i=0;i<candidateCount;++i)total+=candidates[static_cast<size_t>(i)].second;
        int pick=static_cast<int>(random()%static_cast<uint32_t>(std::max(1,total)));
        for(int i=0;i<candidateCount;++i){const auto& c=candidates[static_cast<size_t>(i)];if(pick<c.second)return c.first;pick-=c.second;}
        return candidateCount==0?60:candidates[static_cast<size_t>(candidateCount-1)].first;
    }
    void refreshDisplay(){
        const int trackCount=std::clamp(activeTracks.load(),0,maxTracks);
        for(int c=0;c<maxTracks;++c)for(int j=0;j<steps;++j)display[c*steps+j].store(c<trackCount?parts[c][j]:-1);
    }
    void generate(){
        const int trackCount=std::clamp(activeTracks.load(),0,maxTracks);
        for(int c=0;c<trackCount;++c)if(!hold[c].load())generateTrackInternal(c);
        refreshDisplay();
    }
    void generateTrackInternal(int track){
        if(track>0){
            generateFollowerTrack(track);
            return;
        }
        generateIndependentTrack(track);
    }
    void generateIndependentTrack(int track){
        int previous=-1;
        for(int j=0;j<steps;++j){
            parts[track][j]=chance()<density.load()?noteForStep(j,previous):-1;
            if(parts[track][j]>=0)previous=parts[track][j];
        }
        if(std::all_of(parts[track].begin(),parts[track].end(),[](int n){return n<0;}))parts[track][0]=noteForStep(0,-1);
    }
    void generateFollowerTrack(int track){
        if(std::all_of(parts[0].begin(),parts[0].end(),[](int n){return n<0;})){
            generateIndependentTrack(track);
            return;
        }
        const float distance=std::clamp(relationship[track].load(),0.f,1.f);
        const int roleValue=std::clamp(role[track].load(),0,3);
        const float baseDensity=std::clamp(density.load(),0.f,1.f);
        float laneDensity=baseDensity*(0.78f+distance*0.34f);
        if(roleValue==0)laneDensity=baseDensity*(0.86f-distance*0.22f);
        if(roleValue==1)laneDensity=baseDensity*(0.66f+distance*0.26f);
        if(roleValue==2)laneDensity=baseDensity*(0.34f+distance*0.14f);
        if(roleValue==3)laneDensity=baseDensity*(0.72f+distance*0.42f);
        laneDensity=std::clamp(laneDensity,0.08f,0.98f);
        int previous=-1;
        int phraseShift=0;
        if(distance>0.28f&&chance()<distance)phraseShift=static_cast<int>(random()%5)-2;
        if(roleValue==1&&phraseShift==0)phraseShift=2;
        for(int j=0;j<steps;++j){
            int relatedStep=(j+phraseShift+steps)%steps;
            if(distance>0.72f&&chance()<distance*0.28f)relatedStep=(relatedStep+static_cast<int>(random()%7)-3+steps)%steps;
            const int mainNote=parts[0][relatedStep]>=0?parts[0][relatedStep]:nearestMainNote(relatedStep);
            const bool mainActive=parts[0][relatedStep]>=0;
            float activeChance=mainActive?(0.34f+laneDensity*0.64f):(laneDensity*(0.22f+distance*0.58f));
            if(roleValue==0&&mainActive)activeChance+=0.18f*(1.0f-distance);
            if(roleValue==1&&j%4==0)activeChance+=0.20f;
            if(roleValue==2&&mainActive)activeChance*=0.58f;
            if(roleValue==3)activeChance=laneDensity*(0.54f+distance*0.34f);
            if(chance()<std::clamp(activeChance,0.02f,0.98f)){
                const bool useMain=mainActive?chance()<(0.86f-distance*0.48f):chance()<(0.38f-distance*0.18f);
                parts[track][j]=useMain?relatedNoteFromMain(mainNote,track,j,distance,roleValue,previous):noteForStep(j,previous);
                previous=parts[track][j];
            }else{
                parts[track][j]=-1;
            }
        }
        if(std::all_of(parts[track].begin(),parts[track].end(),[](int n){return n<0;})){
            const int index=static_cast<int>(random()%steps);
            parts[track][index]=relatedNoteFromMain(nearestMainNote(index),track,index,distance,roleValue,-1);
        }
    }
    void evolve(){
        const int trackCount=std::clamp(activeTracks.load(),0,maxTracks);
        const int phraseStart=static_cast<int>(random()%4)*4;
        for(int c=0;c<trackCount;++c)if(!hold[c].load()){
            const auto original=parts[c];
            generateTrackInternal(c);
            const auto candidate=parts[c];
            parts[c]=original;
            const int changes=1+static_cast<int>(random()%2);
            int applied=0;
            for(int offset=0;offset<steps&&applied<changes;++offset){
                const int index=(phraseStart+offset)%steps;
                if(candidate[index]==original[index])continue;
                parts[c][index]=candidate[index];
                ++applied;
            }
            if(std::all_of(parts[c].begin(),parts[c].end(),[](int n){return n<0;}))parts[c]=original;
        }
        refreshDisplay();
    }
    void advance(){
        position=(position+1)%steps;
        if(position==0){++cycles;
            if(cycles>1&&(cycles-1)%std::max(1,every.load())==0){
                if(mode.load()==2)generate();
                if(mode.load()==1)evolve();
            }
        }
        playhead.store(position);
        const int trackCount=std::clamp(activeTracks.load(),0,maxTracks);
        const int solo=soloTrack.load();
        for(int c=0;c<trackCount;++c){
            auto& v=voices[c];
            if(parts[c][position]<0||(mute[c].load()&&c!=solo)){v.connectHold=false;continue;}
            const int src=std::clamp(sourceType[c].load(),0,13);
            const int mat=std::clamp(material[c].load(),0,6);
            const float laneColor=std::clamp(color[c].load(),0.f,1.f);
            const float laneMove=std::clamp(move[c].load(),0.f,1.f);
            const float laneWeather=std::clamp(weather[c].load(),0.f,1.f);
            const float laneDynamics=std::clamp(dynamics[c].load(),0.f,1.f);
            const float laneArticulation=std::clamp(articulation[c].load(),0.f,1.f);
            const int laneLength=std::clamp(noteLength[c].load(),0,3);
            const double noteHz=440*std::pow(2.0,(parts[c][position]-69)/12.0);
            constexpr double lengthBeats[]={0.25,1.0,2.0,4.0};
            constexpr double sourceTail[]={0.72,0.64,1.55,2.05,1.85,1.22,1.65,1.55,1.42,1.08,2.25,1.18,2.65,1.65};
            const double beatSeconds=60.0/std::clamp(bpm.load(),10.f,240.f);
            double duration=beatSeconds*lengthBeats[laneLength]*sourceTail[src]*(0.85+laneMove*0.7);
            duration*=0.65+materialSoftness(mat)*0.85+materialBody(mat)*0.18;
            duration*=0.38+laneArticulation*1.62;
            v.increment=6.283185307*noteHz*(src==3?0.25+laneMove*0.35:(src==11?0.5:1.0))/sampleRate;
            v.wanderIncrement=6.283185307*(0.03+laneMove*0.13+laneWeather*0.24)/sampleRate;
            const int sampleMode=samplePlayMode[c].load();
            const bool sampleLoopMode=src==8&&sampleMode>=1;
            const bool sampleBedMode=src==8&&sampleMode==2;
            if(sampleBedMode)duration=3600.0;
            else if(sampleLoopMode)duration=std::max(duration,8.0+laneMove*8.0);
            if(!sampleBedMode){
                const float tailAmount=std::clamp(tail[c].load(),0.0f,1.0f);
                duration*=1.0+3.0*tailAmount*tailAmount;
            }
            v.decay=static_cast<float>(std::exp(-7.0/(duration*sampleRate)));
            const bool legato=connect[c].load()&&v.connectHold&&v.source==src&&v.envelope>0.0001f
                &&((src!=8&&src!=13)||v.sampleActive);
            v.connectHold=connect[c].load()&&parts[c][(position+1)%steps]>=0;
            if(legato){
                const double ratio=noteHz/std::max(1.0,v.noteHz);
                v.noteHz=noteHz;
                v.bodyIncrement*=ratio;
                for(auto& increment:v.resonatorIncrement)increment*=ratio;
                if((src==8||src==13)&&!sampleLibrary.empty()){
                    const auto& sample=sampleLibrary[static_cast<size_t>(std::clamp(v.sampleIndex,0,static_cast<int>(sampleLibrary.size())-1))];
                    const double pitch=sampleLoopMode?1.0:(src==13?std::pow(2.0,(parts[c][position]-v.sampleRootNote)/12.0):std::pow(2.0,(parts[c][position]-60)/24.0));
                    v.sampleIncrement=(sample.rate/sampleRate)*pitch;
                }
                continue;
            }
            v.source=src;
            v.noteHz=noteHz;
            v.pan=static_cast<float>((static_cast<int>((random()>>8)%2000)-1000)/1000.0)*laneWeather*0.65f;
            const float accent=(position%4==0?0.16f:(position%2==0?0.07f:0.0f));
            const float randomVelocity=(static_cast<float>((random()>>8)&0xffff)/65535.0f-0.5f)*laneDynamics;
            v.envelope=std::clamp(0.78f+accent*laneDynamics+randomVelocity,0.32f,1.18f);
            v.strike=1.0f;
            v.bodyIncrement=6.283185307*noteHz*(src==12?2.01+laneColor*0.035:0.5+materialBody(mat)*0.25)/sampleRate;
            v.bodyPhase=static_cast<double>(random()%1000)/1000.0*6.283185307;
            v.bodyLevel=0.8f+laneColor*0.35f;
            v.bodyDecay=static_cast<float>(std::exp(-7.0/((0.25+laneMove*1.8+materialSoftness(mat))*sampleRate)));
            if(!sampleBedMode||!v.sampleActive)v.sampleActive=false;
            if((src==8||src==13)&&!sampleLibrary.empty()){
                int rootNote=60;
                if(src==13&&!mappedInstruments.empty()){
                    const int chosenInstrument=std::clamp(instrumentChoice[c].load(),0,static_cast<int>(mappedInstruments.size())-1);
                    const auto& zones=mappedInstruments[static_cast<size_t>(chosenInstrument)].zones;
                    const SampleZone* best=nullptr;
                    for(const auto& zone:zones)if(parts[c][position]>=zone.lowNote&&parts[c][position]<=zone.highNote){best=&zone;break;}
                    if(best==nullptr&&!zones.empty()){
                        best=&zones.front();
                        int bestDistance=std::abs(parts[c][position]-best->rootNote);
                        for(const auto& zone:zones){
                            const int distance=std::abs(parts[c][position]-zone.rootNote);
                            if(distance<bestDistance){best=&zone;bestDistance=distance;}
                        }
                    }
                    if(best!=nullptr){
                        v.sampleIndex=std::clamp(best->sampleIndex,0,static_cast<int>(sampleLibrary.size())-1);
                        rootNote=best->rootNote;
                    }else{
                        v.sampleIndex=static_cast<int>((random()+static_cast<uint32_t>(c*17+position*5+parts[c][position]))%sampleLibrary.size());
                    }
                }else{
                    const int chosenSample=sampleChoice[c].load();
                    v.sampleIndex=chosenSample>=0?std::clamp(chosenSample,0,static_cast<int>(sampleLibrary.size())-1):static_cast<int>((random()+static_cast<uint32_t>(c*17+position*5+parts[c][position]))%sampleLibrary.size());
                }
                const auto& sample=sampleLibrary[static_cast<size_t>(v.sampleIndex)];
                v.sampleRootNote=rootNote;
                const bool loopMode=sampleLoopMode;
                const double pitch=loopMode?1.0:(src==13?std::pow(2.0,(parts[c][position]-rootNote)/12.0):std::pow(2.0,(parts[c][position]-60)/24.0));
                v.sampleIncrement=(sample.rate/sampleRate)*pitch;
                const int sampleFrames=static_cast<int>(sample.mono.size());
                v.sampleStartFrame=0;
                v.sampleEndFrame=std::max(2,sampleFrames-1);
                v.sampleFadeSamples=0;
                v.sampleReverse=false;
                if(src==8){
                    const float startNorm=std::clamp(sampleStart[c].load(),0.0f,0.98f);
                    const float sizeNorm=std::clamp(sampleSize[c].load(),0.02f,1.0f);
                    v.sampleStartFrame=std::clamp(static_cast<int>(startNorm*static_cast<float>(std::max(1,sampleFrames-2))),0,std::max(0,sampleFrames-2));
                    const int remaining=std::max(2,sampleFrames-v.sampleStartFrame-1);
                    const int segment=std::clamp(static_cast<int>(remaining*sizeNorm),2,remaining);
                    v.sampleEndFrame=std::clamp(v.sampleStartFrame+segment,v.sampleStartFrame+2,std::max(v.sampleStartFrame+2,sampleFrames-1));
                    v.sampleFadeSamples=static_cast<double>(v.sampleEndFrame-v.sampleStartFrame)*std::clamp(sampleFade[c].load(),0.0f,0.45f);
                    v.sampleReverse=sampleReverse[c].load();
                }
                if(sampleBedMode&&v.sampleActive){
                    // Bed mode keeps running underneath instead of restarting on each marked step.
                }else if(src==13){
                    v.samplePosition=std::min<double>(sample.mono.size()>2?sample.mono.size()-2:0,sample.rate*0.015);
                }else if(loopMode){
                    v.samplePosition=v.sampleReverse?static_cast<double>(std::max(v.sampleStartFrame+1,v.sampleEndFrame-2)):static_cast<double>(v.sampleStartFrame);
                }else{
                    v.samplePosition=v.sampleReverse?static_cast<double>(std::max(v.sampleStartFrame+1,v.sampleEndFrame-2)):static_cast<double>(v.sampleStartFrame);
                }
                v.sampleLevel=loopMode?1.35f+laneColor*0.45f:0.85f+laneColor*0.35f;
                v.sampleActive=sample.mono.size()>1;
            }
            if(laneWeather>0.38f&&chance()<laneWeather*0.45f){
                const double shadowHz=noteHz*std::pow(2.0,((static_cast<int>(random()%9)-4))/12.0);
                v.shadowIncrement=6.283185307*shadowHz*(0.5+0.5*laneMove)/sampleRate;
                v.shadowLevel=0.18f*laneWeather;
                v.shadowDecay=static_cast<float>(std::exp(-7.0/((1.0+laneMove*3.0)*sampleRate)));
            }
            if(src==2){
                constexpr double ratios[]={1.0,2.01,3.02,5.01};
                for(int h=0;h<4;++h){
                    v.resonatorIncrement[h]=6.283185307*noteHz*(ratios[h]+laneColor*0.015*h+materialGrit(mat)*0.025*h)/sampleRate;
                    v.resonatorLevel[h]=(0.8f+laneColor*0.4f)/(1.0f+static_cast<float>(h)*(0.55f+laneColor*0.35f));
                    v.resonatorDecay[h]=static_cast<float>(std::exp(-7.0/((0.8+materialSoftness(mat)*2.2+laneMove*2.5+0.45*h)*sampleRate)));
                }
            }else if(src==7){
                constexpr double ratios[]={1.0,2.72,4.18,5.43};
                for(int h=0;h<4;++h){
                    v.resonatorIncrement[h]=6.283185307*noteHz*(ratios[h]+laneColor*0.04*h+materialRough(mat)*0.08*h)/sampleRate;
                    v.resonatorLevel[h]=(0.9f+laneColor*0.35f)/(1.0f+static_cast<float>(h)*0.58f);
                    v.resonatorDecay[h]=static_cast<float>(std::exp(-7.0/((0.9+laneMove*2.8+0.36*h)*sampleRate)));
                }
            }
        }
    }
};
