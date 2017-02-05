#include "pfod_Base.h"
/**
pfod_Base for Arduino
Base class for all pfod_Base_xxxx classes
The subclasses pfod_Base_xxx must override all the methods below

 (c)2015 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
*/

/* default implementations: must be overridden */
/**
 * time out in seconds
 */
void pfod_Base::_setIdleTimeout(unsigned long timeout) {
  _idleTimeout = timeout * 1000;
}

size_t pfod_Base::write(uint8_t) {
  return 0;
}

int pfod_Base::available() {
  return 0;
}

int pfod_Base::peek() {
  return -1;
}

void pfod_Base::flush()	{}

void pfod_Base::setDebugStream(Print* out) {}

