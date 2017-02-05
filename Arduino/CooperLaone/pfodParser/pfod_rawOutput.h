#ifndef pfod_rawOutput_h
#define pfod_rawOutput_h

#include <Arduino.h>
#include "pfod_Base.h"

/**

 (c)2012 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
 */

class pfod_rawOutput : public Print {
  public:
    size_t write(uint8_t c);
    size_t write(const uint8_t *buffer, size_t size);
    void set_pfod_Base(pfod_Base* arg);
  private:
    pfod_Base* _pfod_Base;

};
#endif // pfod_rawOutput_h
