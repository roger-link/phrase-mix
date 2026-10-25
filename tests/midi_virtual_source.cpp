#include <CoreMIDI/CoreMIDI.h>
#include <cstdio>
#include <cstring>

int main() {
 MIDIClientRef client=0;
 MIDIEndpointRef source=0;
 const auto clientStatus=MIDIClientCreate(CFSTR("Phrase Mix MIDI Test"),nullptr,nullptr,&client);
 const auto sourceStatus=clientStatus==noErr?MIDISourceCreate(client,CFSTR("Phrase Mix Test Input"),&source):clientStatus;
 if(sourceStatus!=noErr){
  std::fprintf(stderr,"Could not create virtual MIDI input (client=%d, source=%d)\n",static_cast<int>(clientStatus),static_cast<int>(sourceStatus));
  if(client!=0)MIDIClientDispose(client);
  return 1;
 }
 std::puts("Virtual input ready. Enter: cc CHANNEL NUMBER VALUE, note CHANNEL NUMBER VELOCITY, off CHANNEL NUMBER, or quit");
 std::fflush(stdout);
 char line[128];
 while(std::fgets(line,sizeof(line),stdin)!=nullptr){
  if(std::strncmp(line,"quit",4)==0)break;
  char command[16]{};
  int channel=0,number=0,value=0;
  const int fields=std::sscanf(line,"%15s %d %d %d",command,&channel,&number,&value);
  const bool cc=std::strcmp(command,"cc")==0;
  const bool note=std::strcmp(command,"note")==0;
  const bool off=std::strcmp(command,"off")==0;
  if(((!off&&fields!=4)||(off&&fields!=3))||(!cc&&!note&&!off)||
     channel<1||channel>16||number<0||number>127||value<0||value>127){
   std::puts("Invalid message");
   std::fflush(stdout);
   continue;
  }
  const UInt8 bytes[3]={static_cast<UInt8>((cc?0xb0:(off?0x80:0x90))|(channel-1)),
                        static_cast<UInt8>(number),static_cast<UInt8>(off?0:value)};
  MIDIPacketList packets{};
  auto* packet=MIDIPacketListInit(&packets);
  if(MIDIPacketListAdd(&packets,sizeof(packets),packet,0,3,bytes)==nullptr||MIDIReceived(source,&packets)!=noErr){
   std::puts("Send failed");
  }else{
   std::puts("Sent");
  }
  std::fflush(stdout);
 }
 MIDIEndpointDispose(source);
 MIDIClientDispose(client);
}
