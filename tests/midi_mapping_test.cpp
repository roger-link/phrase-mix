#include "../Source/MidiMapping.h"
#include <cassert>

int main(){
 using Target=MidiMapping::Target;
 MidiMapping mapping;
 const MidiMapping::Address fader{"controller",2,false,7};
 const MidiMapping::Address button{"controller",2,false,20};
 const MidiMapping::Address note{"controller",3,true,60};
 auto current=[](Target,int){return 0.75f;};

 mapping.learn(Target::Level,0);
 assert(!mapping.receive({fader,20},current));
 assert(!mapping.learning());
 assert(mapping.all().size()==1);
 assert(!mapping.receive({fader,40},current));
 assert(!mapping.receive({fader,90},current));
 auto action=mapping.receive({fader,100},current);
 assert(action&&action->target==Target::Level&&action->track==0);
 mapping.resetPickup(Target::Level,0);
 assert(!mapping.receive({fader,30},current));
 assert(mapping.receive({fader,96},current));

 mapping.learn(Target::Mute,1);
 assert(!mapping.receive({button,0},current));
 assert(mapping.receive({button,127},current));
 assert(!mapping.receive({button,127},current));
 assert(!mapping.receive({button,0},current));
 assert(mapping.receive({button,127},current));

 mapping.learn(Target::Generate,-1);
 assert(!mapping.receive({note,0},current));
 assert(mapping.learning());
 assert(!mapping.receive({note,10},current));
 assert(!mapping.receive({note,0},current));
 assert(mapping.receive({note,5},current));

 mapping.learn(Target::Dynamics,2);
 assert(!mapping.receive({note,80},current));
 assert(mapping.learning());
 mapping.cancelLearn();
 mapping.add(fader,Target::Level,2);
 assert(mapping.all().size()==3);
 assert(mapping.all().back().track==2);
 mapping.remove(2);
 assert(mapping.all().size()==2);
 mapping.clear();
 assert(mapping.all().empty());
}
