#ifndef __CSMFPLAY_HPP__
#define __CSMFPLAY_HPP__
#include <stdint.h>
#include <vector>

#include "CMIDIModule.hpp"

namespace dsa {

class CSMFPlay {
  CMIDIModule m_module[16];
  int m_mods;

public:  
  CSMFPlay(uint32_t rate, int mods=4);
  ~CSMFPlay();
  void Start(bool reset = true);
  void SendMIDIMessage(const CMIDIMsg &msg);
  uint32_t Render16(int16_t *buf, uint32_t length);
};

} // namespace dsa

#endif
