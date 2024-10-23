#include <cstdio>
#include <limits.h>
#include "CSMFPlay.hpp"
#include "COpllDevice.hpp"
#include "CSccDevice.hpp"

using namespace dsa;

CSMFPlay::CSMFPlay(uint32_t rate, int mods) {
  m_mods = mods;
  for(int i=0;i<m_mods;i++){
    if(i&1)
      m_module[i].AttachDevice(new CSccDevice(rate,2));
    else
      m_module[i].AttachDevice(new COpllDevice(rate,2));
  }
}

CSMFPlay::~CSMFPlay() {
  for(int i=0;i<m_mods;i++) delete m_module[i].DetachDevice();
}

void CSMFPlay::Start(bool reset) {
  if (reset) for(int i=0;i<m_mods;i++) m_module[i].Reset();
}

void CSMFPlay::SendMIDIMessage(const CMIDIMsg &msg) {
  m_module[(msg.m_ch*2)%m_mods].SendMIDIMsg(msg);
  if(msg.m_ch!=9)
    m_module[(msg.m_ch*2+1)%m_mods].SendMIDIMsg(msg);
}

uint32_t CSMFPlay::Render16(int16_t *buf, uint32_t length) {
  uint32_t idx;
  for (idx = 0; idx < length; idx++) {
    buf[idx*2] = buf[idx*2+1] = 0;
    for(int i=0; i<m_mods; i++) {
      int32_t b[2];
      m_module[i].Render(b);
      buf[idx*2] += (int16_t)b[0]; 
      buf[idx*2+1] += (int16_t)b[1];
    }
  }
  return idx;
}