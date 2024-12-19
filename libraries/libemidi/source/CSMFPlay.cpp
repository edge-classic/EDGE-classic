#include <cstdio>
#include <limits.h>
#include "CSMFPlay.hpp"
#include "COpllDevice.hpp"
#include "CPSGDrum.hpp"
#include "CSccDevice.hpp"

using namespace dsa;

CSMFPlay::CSMFPlay(uint32_t rate, CSMFPlay::PlayerMode mode) {
  m_mode = mode;
  if (m_mode == SCC_PSG_MODE) {
    m_mods = 2;
    m_module[0].AttachDevice(new CSccDevice(rate,2));
    m_module[1].AttachDevice(new CPSGDrum(rate,1));
  } else {
    m_mods = 1;
    m_module[0].AttachDevice(new COpllDevice(rate, 2));
  }
}

CSMFPlay::~CSMFPlay() {
  for(int i=0;i<m_mods;i++) delete m_module[i].DetachDevice();
}

void CSMFPlay::Start(bool reset) {
  if (reset) for(int i=0;i<m_mods;i++) m_module[i].Reset();
}

void CSMFPlay::SendMIDIMessage(const CMIDIMsg &msg) {
  if (m_mode == SCC_PSG_MODE) {
    if (msg.m_ch == 9)
      m_module[1].SendMIDIMsg(msg);
    else
      m_module[0].SendMIDIMsg(msg);
  } else {
    m_module[0].SendMIDIMsg(msg);
  }
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