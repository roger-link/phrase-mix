#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

struct TakeTiming {
    double bpm=0,startStep=0;
    bool steady=false,startedPlaying=false;
    int64_t frames=0;
    void observe(double tempo,double step,bool playing,int count,double rate){
        if(frames==0){bpm=tempo;startStep=step;startedPlaying=playing;steady=playing;}
        const double expected=startStep+static_cast<double>(frames)*bpm*4.0/(60.0*rate);
        if(!playing||std::abs(tempo-bpm)>0.001||std::abs(step-expected)>0.01)steady=false;
        frames+=count;
    }
    bool canAlign() const {return steady&&startedPlaying&&std::isfinite(bpm)&&bpm>=10&&bpm<=240&&std::isfinite(startStep)&&startStep>=0&&startStep<=1.0e12;}
};

// Immutable stereo audio is decoded off the audio thread. Publication and reclamation
// happen on the message thread; render never takes a lock or releases a clip.
class BackingTracks {
public:
    static constexpr int maxTracks=8;
    struct Clip {
        std::vector<float> left,right;
        double rate=48000;
        size_t frames() const {return std::min(left.size(),right.size());}
        double duration() const {return static_cast<double>(frames())/std::max(1.0,rate);}
    };
    using Clips=std::array<std::shared_ptr<const Clip>,maxTracks>;
    struct Controls {bool active=false,loop=false,audible=true,focused=true;float level=1;};
    using Processor=void (*)(void*,int,float*,float*,int);
    void prepare(int count){scratchLeft.resize(static_cast<size_t>(std::max(1,count)));scratchRight.resize(scratchLeft.size());gains.fill(0);focusGains.fill(1);}
    void publish(const Clips& clips){
        auto bank=std::make_unique<Bank>();bank->clips=clips;
        const auto* next=bank.get();banks.push_back(std::move(bank));published.store(next);
        reclaim();
    }
    void reclaim(){
        if(rendering.load())return;
        const auto* current=published.load();
        banks.erase(std::remove_if(banks.begin(),banks.end(),[current](const auto& bank){return bank.get()!=current;}),banks.end());
    }
    void render(float* left,float* right,int count,double startSeconds,double outputRate,bool playing,bool restarted,
                const std::array<Controls,maxTracks>& controls,float focusSeconds,Processor processor=nullptr,void* context=nullptr){
        if(!playing)return;
        rendering.store(true);
        const auto* bank=published.load();
        if(bank==nullptr||count>static_cast<int>(scratchLeft.size())){rendering.store(false);return;}
        const float slew=1.0f-static_cast<float>(std::exp(-1.0/(std::max(1.0,outputRate)*0.012)));
        const float focusSlew=1.0f-static_cast<float>(std::exp(-6.9/(std::max(1.0,outputRate)*std::clamp(focusSeconds,0.1f,8.0f))));
        for(int track=0;track<maxTracks;++track){
            const auto& control=controls[track];const auto* clip=bank->clips[track].get();
            if(!control.active||clip==nullptr||clip->frames()==0)continue;
            const float target=control.audible?std::clamp(control.level,0.0f,1.0f):0.0f;
            if(restarted){gains[track]=target;focusGains[track]=control.focused?1.0f:0.0f;}
            for(int i=0;i<count;++i){
                double position=(startSeconds+static_cast<double>(i)/outputRate)*clip->rate;
                if(control.loop)position=std::fmod(position,static_cast<double>(clip->frames()));
                float l=0,r=0;
                if(position>=0&&position<static_cast<double>(clip->frames())){
                    const size_t index=static_cast<size_t>(position);
                    const size_t next=index+1<clip->frames()?index+1:control.loop?0:index;
                    const float fraction=static_cast<float>(position-static_cast<double>(index));
                    l=clip->left[index]+(clip->left[next]-clip->left[index])*fraction;
                    r=clip->right[index]+(clip->right[next]-clip->right[index])*fraction;
                }
                scratchLeft[static_cast<size_t>(i)]=l;scratchRight[static_cast<size_t>(i)]=r;
            }
            if(processor)processor(context,track,scratchLeft.data(),scratchRight.data(),count);
            for(int i=0;i<count;++i){
                gains[track]+=(target-gains[track])*slew;
                focusGains[track]+=((control.focused?1.0f:0.0f)-focusGains[track])*focusSlew;
                const float gain=gains[track]*focusGains[track];
                if(right){left[i]+=scratchLeft[static_cast<size_t>(i)]*gain;right[i]+=scratchRight[static_cast<size_t>(i)]*gain;}
                else left[i]+=(scratchLeft[static_cast<size_t>(i)]+scratchRight[static_cast<size_t>(i)])*0.5f*gain;
            }
        }
        for(int i=0;i<count;++i){left[i]=std::clamp(left[i],-1.0f,1.0f);if(right)right[i]=std::clamp(right[i],-1.0f,1.0f);}
        rendering.store(false);
    }
private:
    struct Bank {Clips clips;};
    std::vector<std::unique_ptr<Bank>> banks;
    std::atomic<const Bank*> published{nullptr};
    std::atomic<bool> rendering{false};
    std::vector<float> scratchLeft,scratchRight;
    std::array<float,maxTracks> gains{},focusGains{};
};
