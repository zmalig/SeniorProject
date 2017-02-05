
#ifndef pfodEEPROM_h
#define pfodEEPROM_h

#ifdef __RFduino__
#define __no_pfodEEPROM__
#elif ARDUINO_ARCH_AVR
#include <EEPROM.h>
#elif ARDUINO_ARCH_ESP8266
#include <EEPROM.h>
#elif ARDUINO_ARCH_ARC32
#include <EEPROM.h>
#else
#define __no_pfodEEPROM__
#endif

#endif //pfodEEPROM_h

