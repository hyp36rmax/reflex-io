#pragma once
#include "../../features/jogcon_capture/jogcon_capture.h"
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
template<class Base> class JogconCaptureDriver final:public Base {
  jogcon_capture::Event event{};
  const uint8_t port;
  uint16_t wireCount=0;
  bool jogconSeen=false;
protected:
  byte shiftInOut(const byte requested) override {
    byte sent=requested;
    const bool poll=wireCount>=2 && event.tx[0]==0x01 && event.tx[1]==0x42;
    if(wireCount==3 && poll && jogconSeen) {
      event.motorRequested=requested;
      if(jogconCaptureMotorDisabled()) { sent=0; event.flags|=jogcon_capture::MotorOff; }
      event.motorSent=sent;
    }
    const byte in=Base::shiftInOut(sent);
    if(wireCount<jogcon_capture::kWireBytes) { event.tx[wireCount]=sent; event.rx[wireCount]=in; }
    if(wireCount==1 && poll==false && event.tx[0]==0x01 && requested==0x42) {
      if(in==0xE3) jogconSeen=true;
      else if(in==0x73 || in==0x79 || in==0x23 || in==0x53 || in==0x63) jogconSeen=false;
    }
    ++wireCount; return in;
  }
public:
  JogconCaptureDriver(uint8_t spi,uint8_t att,uint8_t ack,uint8_t cmd,uint8_t dat,uint8_t clk,uint8_t p)
    :Base(spi,att,ack,cmd,dat,clk),port(p) {}
  void attention() override {
    event={}; event.kind=uint8_t(jogcon_capture::Kind::Wire); event.port=port;
    event.us=micros(); event.gapUs=this->getAttentionInterval(); wireCount=0; Base::attention();
  }
  void noAttention() override {
    Base::noAttention();
    // Only controller polls from a JogCon session; no card/configuration payloads.
    if(!jogconSeen || wireCount<2 || event.tx[0]!=0x01 || event.tx[1]!=0x42) return;
    event.durationUs=uint32_t(micros()-event.us);
    event.count=wireCount>jogcon_capture::kWireBytes?jogcon_capture::kWireBytes:uint8_t(wireCount);
    if(wireCount>jogcon_capture::kWireBytes) event.flags|=jogcon_capture::Truncated;
    if(wireCount==9 && event.rx[0]==0xFF && event.rx[1]==0xE3 && event.rx[2]==0x5A) event.flags|=jogcon_capture::HeaderOk;
    jogconCaptureWire(event);
  }
};
#endif
