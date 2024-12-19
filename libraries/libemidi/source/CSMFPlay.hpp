#ifndef __CSMFPLAY_HPP__
#define __CSMFPLAY_HPP__
#include <stdint.h>
#include <vector>

#include "CMIDIModule.hpp"

namespace dsa {

class CSMFPlay {
public:  
  enum PlayerMode
  {
      OPLL_MODE,
      SCC_PSG_MODE
  };

  CMIDIModule m_module[2];
  int m_mods;
  PlayerMode m_mode;

  CSMFPlay(uint32_t rate, PlayerMode mode = SCC_PSG_MODE);
  ~CSMFPlay();
  void Start(bool reset = true);
  void SendMIDIMessage(const CMIDIMsg &msg);
  uint32_t Render16(int16_t *buf, uint32_t length);
};

} // namespace dsa

#endif
