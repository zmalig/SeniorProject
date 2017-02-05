#ifndef switchDebounce_h
#define switchDebounce_h
// Ver 3.1 reduced AC input debounce
// Ver 3.0 revised AC input debounce to improve noise rejection
/*
 DebouncedSwitch debounces normally open mechanical switches connected between a digital input and GND
 Switches can be momentary push button or change over.
 For change over type switches connect the normally open contacts to between the digital input and GND
 The digital input specified in the constructor is set as an INPUT_PULLUP
 so if the switch is disconnected the isDown() returns FALSE

 NOTE: this code delays isChanged() signal until Debounce_mS_.. after the last bounce detected
 
 For AC inputs, half wave AC is assumed (but full wave is also OK).
 isDown() is true if have AC input for more then 25 cycles ~0.5 sec.
 isDown() if false if have not seen an AC pulse in the last ~0.5 sec.  
 This longer 'debounce' time is to allow for power line spikes and temporary dropouts.

 Each time switch state changes isChanged() returns TRUE for one loop. It is cleared on the next call to update()
 If the switch is connected to GND when the board is reset / powered up (i.e. when setup() is executed)
 then isChanged() returns true until the first call to update()

 DebouncedSwitch adds about 330 bytes to the program and uses about 6 bytes RAM per switch

 (c)2012 Forward Computing and Control Pty. Ltd. 
 This code may be freely used for both private and commerical use.
 Provide this copyright is maintained.
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "Arduino.h"

// mS to debounce for connect to GND (DOWN) or disconnecting from GND (going UP)
// edit these if your switch requires it
// usually the switch release (going UP) requires little if any debounce at all
#define Debounce_mS_Down 50
#define Debounce_mS_Up 50

// for ACinput switch after 5 pulses on or missing 5 pulses at 50Hz i.e. 0.1sec

class DebouncedSwitch {
public:

  /**
   * Construct a debounced switch that monitors this pin
   */
  DebouncedSwitch(byte pin, bool ACinput = false);

  /**
   * Call this method in loop()
   * so that is get called every loop to update the switch state
   */
  void update(void);

  /**
   * Returns true when the debounced state changes
   * this returns TRUE until the next time update() is called
   */
  bool isChanged();

  /**
   * Returns true when the debounced state of the switch is DOWN (connected to GND)
   */
  bool isDown();

private:
	const unsigned long AC_DEBOUNCE_LENGTH = 5 * (1000 / 50); // 5 cycles of 50Hz in mS works for 60Hz system also
	const unsigned long POWER_ON_CYCLE_LENGTH = 2 * (1000 / 50); // 2 cycles of 50Hz in mS works for 60Hz system also
	// must see a pulse within POWER_ON_CYCLE_LENGTH to keep timer running to for turn ON of AC

	bool ACinput; // true is input is from half wave optio isolator
  bool isSwitch(unsigned char state);
  byte switchPin;
  long lastChangeMillis;
  bool timerRunning;
  long powerOnTimerStart;
  bool powerOnTimerRunning;
  struct _reg_SwitchStatus {
    unsigned char Changed :1; // set if debounced switch state changed from up / down, set for one loop only
    unsigned char Down :1; // set if switch down, closed, digital input connected to GND
    unsigned char Click :1; // not implemented yet // set if switch pressed and released in short (click) periods, set on release. set for one loop only
    unsigned char Pressed :1; // not implemented yet  // set if switch pressed for more then Press_count, i.e. long press, set for one loop only and not set again until switch released and pressed again.
    unsigned char DownRepeat :1; // not implemented yet // set repeatedly if switch pressed for more then RepeatedPress_Count, each set for one loop only, reset again after RepeatedPress_Count
    // cleared on next loop and counter rest for repeated firing
    unsigned char UpTimeout :1; // not implemented yet // set switch has been released for more then the Release_Count, set for one loop only, reset again after Release_Count
    // cleared on next loop and counter rest for repeated firing
    unsigned char switchStatus_6 :1; //  not used yet
    unsigned char lastSwitchReading :1; // set if switch down (GND) on prevous update. This is not debounced
  };

// this union lets you use the above structure to access the bits in the SwitchStatus.
// It also encourages the compiler to put the SwitchStatus in a register.
  union _reg_SwitchStatus_Union {
    byte _reg;
    struct _reg_SwitchStatus _struct;
  };

  byte SwitchStatus;  // hold the 8 status bits

  // defines for accessing status bits
#define SwitchChanged 0
#define SwitchDown 1
  // the following defines are not used yet
#define SwitchClicked 2
#define SwitchPressed 3
#define SwitchDownRepeat 4
#define SwitchUpTimeout 5
};

#endif // switchDebounce_h

