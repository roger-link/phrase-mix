#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class MidiMapping {
public:
 enum class Target {Level, Mute, Hold, Focus, Relationship, Dynamics, Articulation, Generate, Back, Forward, Keep, Play};
 struct Address {
  std::string device;
  int channel=1;
  bool note=false;
  int number=0;
  bool operator==(const Address& other) const {
   return device==other.device&&channel==other.channel&&note==other.note&&number==other.number;
  }
 };
 struct Binding {
  Address address;
  Target target=Target::Level;
  int track=-1;
  bool pickedUp=false;
  float lastPhysical=-1.0f;
  bool wasPressed=false;
 };
 struct Event {Address address;int value=0;};
 struct Action {Target target;int track;float value;};

 static bool continuous(Target target) {
  return target==Target::Level||target==Target::Relationship||target==Target::Dynamics||target==Target::Articulation;
 }
 void learn(Target target,int track) {pending=std::make_pair(target,track);}
 void cancelLearn() {pending.reset();}
 bool learning() const {return pending.has_value();}
 std::optional<std::pair<Target,int>> pendingTarget() const {return pending;}
 const std::vector<Binding>& all() const {return bindings;}
 void clear() {bindings.clear();pending.reset();}
 void remove(size_t index) {if(index<bindings.size())bindings.erase(bindings.begin()+static_cast<std::ptrdiff_t>(index));}
 void add(Address address,Target target,int track) {
  bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[&](const Binding& b){
   return b.address==address||(b.target==target&&b.track==track);
  }),bindings.end());
  bindings.push_back({std::move(address),target,track});
 }
 void resetPickup() {
  for(auto& binding:bindings){binding.pickedUp=false;binding.lastPhysical=-1.0f;binding.wasPressed=false;}
 }
 void resetPickup(Target target,int track) {
  for(auto& binding:bindings)if(binding.target==target&&binding.track==track){binding.pickedUp=false;binding.lastPhysical=-1.0f;}
 }
 std::optional<Action> receive(const Event& event,const std::function<float(Target,int)>& current) {
  if(event.value<0||event.value>127)return std::nullopt;
  if(pending.has_value()){
   if(event.address.note&&event.value==0)return std::nullopt;
   const auto [target,track]=*pending;
   if(event.address.note&&continuous(target))return std::nullopt;
   pending.reset();
   add(event.address,target,track);
   auto& binding=bindings.back();
   binding.lastPhysical=event.value/127.0f;
   binding.wasPressed=event.address.note?event.value>0:event.value>=64;
   return std::nullopt;
  }
  for(auto& binding:bindings){
   if(!(binding.address==event.address))continue;
   if(continuous(binding.target)){
    if(event.address.note)return std::nullopt;
    const float value=event.value/127.0f;
    const float software=std::clamp(current(binding.target,binding.track),0.0f,1.0f);
    if(!binding.pickedUp){
     const bool near=std::abs(value-software)<=0.025f;
     const bool crossed=binding.lastPhysical>=0.0f&&(binding.lastPhysical-software)*(value-software)<=0.0f;
     binding.lastPhysical=value;
     if(!near&&!crossed)return std::nullopt;
     binding.pickedUp=true;
    }
    binding.lastPhysical=value;
    return Action{binding.target,binding.track,value};
   }
   const bool pressed=event.address.note?event.value>0:event.value>=64;
   const bool trigger=pressed&&!binding.wasPressed;
   binding.wasPressed=pressed;
   if(trigger)return Action{binding.target,binding.track,1.0f};
   return std::nullopt;
  }
  return std::nullopt;
 }
private:
 std::vector<Binding> bindings;
 std::optional<std::pair<Target,int>> pending;
};
