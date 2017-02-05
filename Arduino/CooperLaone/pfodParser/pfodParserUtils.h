#ifndef pfodParserUtils_h
#define pfodParserUtils_h
/**
pfodParserUtils for Arduino

(c)2013 Forward Computing and Control Pty. Ltd.
www.forward.com.au
This code may be freely used for both private and commercial use.
Provide this copyright is maintained.
*/

// will always put '\0' at dest[maxLen]
size_t strncpy_safe(char* dest, const char* src, size_t maxLen);
uint32_t ipStrToNum(const char* ipStr);
void getProgStr(const __FlashStringHelper *ifsh, char*str, int maxLen);

#endif