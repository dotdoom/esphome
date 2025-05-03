#include <ELECHOUSE_CC1101_SRC_DRV.h>

#ifndef cc1101_h
#define cc1101_h

static bool cc1101_initialized = false;

static void cc1101_Init() {
  if (!cc1101_initialized) {
    ELECHOUSE_cc1101.addSpiPin(14, 16, 15, 13, 0);
    ELECHOUSE_cc1101.setModul(0);
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setPA(7);
    ELECHOUSE_cc1101.setMHZ(433.34);
    ELECHOUSE_cc1101.SetTx();
  }
  cc1101_initialized = true;
}

#endif
