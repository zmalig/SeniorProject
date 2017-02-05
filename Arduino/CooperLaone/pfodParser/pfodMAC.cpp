/**
pfodMAC for Arduino
 Updates power cycles, builds the Challenge, keeps track in message counts and
 hashes and check message hashes
 It uses upto 19 bytes of EEPROM starting from the address passed to init()

 (c)2013 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commercial use.
 Provide this copyright is maintained.
*/

#include "pfodEEPROM.h"
#include "pfodMAC.h"
#include "HexConversionUtils.h"
#include "SipHash_2_4.h"
#include "Stream.h"
#include "pfodParserUtils.h"

//#define DEBUG_BUILD_CHALLENGE
//#define DEBUG_CHAP_EEPROM
//#define DEBUG_HASH
// this next define will keep the challange the same for ALL connections.
// MUST only be used for testing, make use this line is commented out for REAL use
//#define DEBUG_DISABLE_POWER_AND_CONNECTION_COUNTERS

// the current address in the EEPROM (i.e. which byte
// we're going to write to next)
const  unsigned char pfodMAC::powerCycleSize = 2; // int
const  unsigned char pfodMAC::counterSize = 2; // int
const  unsigned char pfodMAC::timerSize = 4; // long

const  unsigned char pfodMAC::PowerCyclesOffset = 0;
const  unsigned char pfodMAC::KeyOffset = pfodMAC::PowerCyclesOffset + pfodMAC::powerCycleSize; // length comes first then key

const unsigned char pfodMAC::maxKeySize;// = 16;


uint8_t pfodMAC::eeprom_read(int address) {
#ifdef __no_pfodEEPROM__
  return 0;
#else
  return EEPROM.read(address);
#endif
}

void pfodMAC::eeprom_write(int address, uint8_t value) {
#ifdef __no_pfodEEPROM__
  
#else	
  EEPROM.write(address, value);
#endif
}

union _uintBytes {
  unsigned int i;
  byte b[sizeof(int)]; // 2 bytes
} ;
union _ulongBytes {
  unsigned long l;
  byte b[sizeof(long)]; // 4 bytes
} ;


pfodMAC::pfodMAC() {
  debugOut = NULL;
  keyLen = 0;
  bigEndian = isBigEndian();
  connectionCounter = 0; // increment each connection
  powerCycles = 0;
  CHAP_EEPROM_Address = 0;
}

/**
* Override debugOut stream.
* This can be called before or after calling pfodMAC::init()
*
* debugOut is not written to unless you uncomment one of the #define DEBUG_ settings above
*/
void pfodMAC::setDebugStream(Print* out) {
  debugOut = out;
}

/**
 * Returns true if uC stores numbers in BigEndian format,
 * else returns false if stored in LittleEndian format
 */
boolean pfodMAC::isBigEndian() {
  boolean bigEndian = true;
  _ulongBytes longBytes;
  longBytes.l = 1;
  if (longBytes.b[0] == 1) {
    bigEndian = false;
  } else {
    bigEndian = true;
  }
  return bigEndian;
}

/**
*  Initialize the class
*
*  @param debug_out -- the stream to used for debug output
*  @param eepromAddress -- the starting address in the EEPROM where the powercycles, keyLen and key are stored i.e. upto 19 bytes in all
*  @return true if powercycles still > 0
*/
boolean pfodMAC::init(int eepromAddress) {
#ifdef DEBUG_HASH
  if (debugOut != NULL) {
    debugOut->print(F("isBigEndian = "));
    if (bigEndian) {
      debugOut->println(F("true"));
    } else {
      debugOut->println(F("false"));
    }
  }
#endif

  CHAP_EEPROM_Address = eepromAddress;
  keyLen = readSecretKey(key, CHAP_EEPROM_Address + KeyOffset);
  if (keyLen == 0) {
    powerCycles = 0xffff; // make sure we are valid
#ifdef DEBUG_CHAP_EEPROM
    if (debugOut != NULL) {
      debugOut->print(F(" NO secretKey "));
      debugOut->print(F("powerCycles:"));
      debugOut->println(powerCycles);
    }
#endif
  } else {
    powerCycles = readPowerCycles(CHAP_EEPROM_Address + PowerCyclesOffset);
#ifdef DEBUG_CHAP_EEPROM
    if (debugOut != NULL) {
      debugOut->print(F("powerCycles:"));
      debugOut->println(powerCycles);
      debugOut->print(F(" Secret Key length:"));
      debugOut->print(keyLen);
      debugOut->print(F(" 0x"));
      printBytesInHex(debugOut, key, keyLen);
      debugOut->println();
    }
#endif
  }
  // key must be 16 bytes, if keyLen >0 and < 16 padd with zeros
  if ((keyLen > 0) && (keyLen < pfodMAC::maxKeySize)) {
    for (unsigned char i = keyLen; i < pfodMAC::maxKeySize; i++) {
      key[i] = 0;
    }
    keyLen = pfodMAC::maxKeySize;
  }
  return isValid(); // tests powerCycles > 0
}


/**
* Is this MAC still valid
* currently only checks that the the powerCycles still > 0
*/
boolean pfodMAC::isValid() {
  return (powerCycles > 0);
}

/**
* Hashes challenge of size challengeSize
*
* returns a pointer to an 8 byte array the hash
*/
byte *pfodMAC::hashChallenge(byte *challenge) {
  sipHash.initFromRAM(key);
  for (int i = 0; i < challengeByteSize; i++) {
    sipHash.updateHash(challenge[i]);
  }
  sipHash.finish();
  return sipHash.result;
}

/**
* Init the underlying hash with the key that has been loaded into RAM
*/
void pfodMAC::initHash() {
#ifdef DEBUG_HASH
  if (debugOut != NULL) {
    debugOut->println(" initHash");
  }
#endif
  sipHash.initFromRAM(key);
}

/**
* hash another byte
*/
void pfodMAC::putByteToHash(byte b) {
#ifdef DEBUG_HASH
  if (debugOut != NULL) {
    byte bs[1];
    bs[0] = b;
    debugOut->println();
    debugOut->print(F("hash "));
    if (b < 32 || b >= 127) {
      printBytesInHex(debugOut, bs, 1);
    } else {
      debugOut->print((char)b);
    }
    debugOut->println();
  }
#endif
  sipHash.updateHash(b);
}

/**
* hash byte[]
*/
void pfodMAC::putBytesToHash(byte* b, int len) {
  for (int i = 0; i < len; i++) {
#ifdef DEBUG_HASH
    if (debugOut != NULL) {
      byte bs[1];
      bs[0] = b[i];
      debugOut->println();
      debugOut->print(F("hash "));
      if (b[i] < 32 || b[i] >= 127) {
        printBytesInHex(debugOut, bs, 1);
      } else {
        debugOut->print((char)b[i]);
      }
      debugOut->println();
    }
#endif
    sipHash.updateHash(b[i]);
  }
}

/**
* hash a long (convert to bigEndian first)
*/
void pfodMAC::putLongToHash(long l) {
  union _ulongBytes longBytes;
  longBytes.l = l;
  if (!bigEndian) {
    reverseBytes(longBytes.b, sizeof(long));
  }
#ifdef DEBUG_HASH
  if (debugOut != NULL) {
    debugOut->print(F("Hash long"));
    printBytesInHex(debugOut, longBytes.b, sizeof(long));
    debugOut->println();
  }
#endif
  for (unsigned int i = 0; i < sizeof(long); i++) {
    sipHash.updateHash(longBytes.b[i]);
  }
}


/**
* finish the hash to get result
* The result is available from getHashedResult()
*/
void pfodMAC::finishHash() {
#ifdef DEBUG_HASH
  if (debugOut != NULL) {
    debugOut->println(" finishHash");
  }
#endif
  sipHash.finish();
#ifdef DEBUG_HASH
  if (debugOut != NULL) {
    printBytesInHex(debugOut, sipHash.result, 8);
    debugOut->println();
  }
#endif
}

/**
* Get the hash result
* Returns a byte* to the 8 byte array containing the hash result
*/
byte* pfodMAC::getHashedResult() {
  return sipHash.result;
}


/**
* Converts msgHash from hex Digits to bytes
* and then compares the resulting byte[] with the current hash result
* @param msgHash null terminated ascii string of hex chars (0..9A..F) or (0..9a..f)
*      this input is overwritten with the converted bytes
* @param hashSize the number of bytes expected in the hash must be < length of msgHash since the converted bytes are store in that array.
*/
boolean pfodMAC::checkMsgHash(const byte * msgHash, int hashSize) {
  unsigned int len = asciiToHex((const char*)msgHash, (unsigned char*)msgHash, hashSize);
  if (len != (unsigned)hashSize) {
    return false;
  }
  return cmpToResult(msgHash, hashSize);
}


/**
* returns true if response bytes match upto length len
* @param response byte* the response to check
* @param len int the number of bytes to check. Must be <= 8
*/
boolean pfodMAC::cmpToResult(const byte * response, int len) {
  byte *result = getHashedResult();
  for (int i = 0; i < len; i++) {
    if (*result++ != *response++) {
      return false;
    }
  }
  return true;
}

/**
* For each byte in the array upto length len
* convert to a two digit HEX and print to Stream
* @param out the Stream* to print to
* @param b the byte* to the bytes to be converted and printed
* @param len the number of bytes to print
*/
void pfodMAC::printBytesInHex(Print * out, byte * b, int len) {
  char hex[3]; // two hex bytes and null
  for (int i = 0; i < len; i++) {
    hexToAscii(b, 1, hex, 3);
    b++;
    out->print(hex);
  }
}

/**
*  read secret key from EEPROM and return len
*  only reads upto maxKeySize bytes regardless
*
* @param bRtn -- pointer to array long enough to hold the key read from EEPROM
* @param keyAdd -- EEPROM address where the key lengh (byte) followed by the key is stored
*/
unsigned  int pfodMAC::readSecretKey(byte * bRtn, int keyAdd) {
  byte keyLen;
  keyLen = eeprom_read(keyAdd);
  byte *keyPtr = bRtn;

  int add = keyAdd + 1;
  for (unsigned int i = 0; (i < keyLen) && (i < maxKeySize); i++) {
    *keyPtr++ = eeprom_read(add++);
  }

#ifdef DEBUG_CHAP_EEPROM
  if (debugOut != NULL) {
    debugOut->print(" Read Key from EEPROM length:");
    debugOut->print(keyLen);
    debugOut->print(" 0x");
    printBytesInHex(debugOut, bRtn, keyLen);
    debugOut->println();
  }
#endif
  return keyLen;
}

/**
* read power cycles if >0 decrement by 1 and save again
* else if 0 leave as is
*
* @param powerCycleAdd -- the EEPROM address where the 2byte power cycle unsigned int is stored
*
* returns the updated power cycle count as an unsigned int
*/
unsigned int pfodMAC::readPowerCycles(int powerCycleAdd) {
  unsigned int rtn = 0;
  union _uintBytes intBytes;
  intBytes.i = 0;

  // read it back
  intBytes.i = 0;
  int add = powerCycleAdd;
  for (int i = 0; i < 2; i++) {
    // note since the power cycle count is save the same way
    // no need to worry about endian ness
    intBytes.b[i] = eeprom_read(add++);
  }
#ifdef DEBUG_CHAP_EEPROM
  if (debugOut != NULL) {
    debugOut->print(" PowerCycles read from EEPROM 0x");
    printBytesInHex(debugOut, intBytes.b, 2);
    debugOut->println();
  }
#endif
  rtn = intBytes.i;
  if (intBytes.i != 0) {
    intBytes.i--;
#ifdef DEBUG_CHAP_EEPROM
    if (debugOut != NULL) {
      debugOut->print(" powerCycles is not zero Subtract 1 bytes before write 0x");
      printBytesInHex(debugOut, intBytes.b, 2);
      debugOut->println();
    }
#endif

    add = powerCycleAdd;
    for (int i = 0; i < 2; i++) {
      // save back in the same byte order as read
      // no need to worry about endian ness
      eeprom_write(add++, intBytes.b[i]);
    }
#ifdef DEBUG_CHAP_EEPROM
    if (debugOut != NULL) {
      debugOut->println(" EEPROM written");
    }
#endif
  }

  return rtn;
}

/**
* buildChallenge  -- NOTE: each call to this method increments the connection counter!!!
* Challenge built from
* powerCycles + connectionCounter + mS_sinceLastConnection (limited to 31 bits) 32th bit set if last connection failed
* finally hashes the challange bytes (result stored in hash fn)
* use getHashedResult() to retrieve pointer to result when needed for checking return value
*
* All counters stored in BigEndian format into the challenge byte array
* powerCycles read on start up
*
* @param challengeBytes byte* to array to store challenge
* @param mS_sinceLastConnection  mS since last connection (max value about 25days), -ve if last connection failed
*
* returns true if isValid() is true;
*/
boolean pfodMAC::buildChallenge(byte * challengeBytes, long mS_sinceLastConnection) {
  connectionCounter++;
  if (connectionCounter == 0) {
    // read next power cycle
    powerCycles = readPowerCycles(CHAP_EEPROM_Address);
#ifdef DEBUG_BUILD_CHALLENGE
    if (debugOut != NULL) {
      debugOut->println(" decremented Power cycles ");
      debugOut->println("----------------------------");
    }
#endif
  }
  if (!isValid()) {
    return false;
  }

#ifdef DEBUG_DISABLE_POWER_AND_CONNECTION_COUNTERS
  if (debugOut != NULL) {
    powerCycles = 0xffff;
    connectionCounter = 0;
    mS_sinceLastConnection = 0;
  }
#endif
#ifdef DEBUG_BUILD_CHALLENGE
  if (debugOut != NULL) {
    debugOut->print("powerCycles:");
    debugOut->println(powerCycles);
    debugOut->print(" 0x");
    {
      union _uintBytes intBytes;
      intBytes.i = powerCycles;
      if (!bigEndian) {
        reverseBytes(intBytes.b, powerCycleSize);
      }
      printBytesInHex(debugOut, intBytes.b, powerCycleSize);
      debugOut->println();
    }
    debugOut->print("connectionCounter:");
    debugOut->println(connectionCounter);
    debugOut->print(" 0x");
    {
      union _uintBytes intBytes;
      intBytes.i = connectionCounter;
      if (!bigEndian) {
        reverseBytes(intBytes.b, counterSize);
      }
      printBytesInHex(debugOut, intBytes.b, counterSize);
      debugOut->println();
    }
    debugOut->print("mS_sinceLastConnection is :");
    debugOut->println(mS_sinceLastConnection);
    debugOut->print(" 0x");
    {
      union _ulongBytes longBytes;
      longBytes.l = mS_sinceLastConnection;
      if (!bigEndian) {
        reverseBytes(longBytes.b, timerSize);
      }
      printBytesInHex(debugOut, longBytes.b, timerSize);
      debugOut->println();
    }
  }
#endif
  // set challenge bytes
  int challengeIdx;
  challengeIdx = 0;
  union _uintBytes intBytes;
  intBytes.i = powerCycles;
  if (!bigEndian) {
    reverseBytes(intBytes.b, powerCycleSize);
  }
  for (int i = 0; i < powerCycleSize; i++) {
    challengeBytes[challengeIdx++] = intBytes.b[i];
  }
  intBytes.i = connectionCounter;
  if (!bigEndian) {
    reverseBytes(intBytes.b, counterSize);
  }
  for (int i = 0; i < counterSize; i++) {
    challengeBytes[challengeIdx++] = intBytes.b[i];
  }
  union _ulongBytes longBytes;
  longBytes.l = mS_sinceLastConnection;
  if (!bigEndian) {
    reverseBytes(longBytes.b, timerSize);
  }
  for (int i = 0; i < timerSize; i++) {
    challengeBytes[challengeIdx++] = longBytes.b[i];
  }

  hashChallenge(challengeBytes); // set result

#ifdef DEBUG_BUILD_CHALLENGE
  if (debugOut != NULL) {
    debugOut->println("Challenge ");
    printBytesInHex(debugOut, challengeBytes, challengeByteSize);
    debugOut->println();
    debugOut->println("Hash of Challenge ");
    printBytesInHex(debugOut, getHashedResult(), challengeByteSize);
    debugOut->println();
  }
#endif

  return true; // valid challenge
}

/**
* reverse the bytes in the byte array of length len
* @param b byte* pointer to start position on byte array
* @param len length of bytes to swap
*
*/
void pfodMAC::reverseBytes(byte * b, int len) {
  byte *b_r = b + len - 1;
  for (int i = 0; i < (len >> 1); i++) {
    byte b1 = *b;
    byte b2 = *b_r;
    *b++ = b2;
    *b_r-- = b1;
  }
}

