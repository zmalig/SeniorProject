/*
 DebouncedSwitch debounces normally open mechanical swithes connected between a digital input and GND
 Switches can be momentary push button or change over.
 For change over type switches connect the normally open contacts to between the digital input and GND
 The digital input specified in the constructor is set as an INPUT_PULLUP
 so if the switch is disconnected the isDown() returns FALSE

 NOTE: this code delays isChanged() signal until Debounce_mS_.. after the last bounce detected

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

#include "DebouncedSwitch.h"

/**
 * Returns true when the debounced state changes
 * this returns TRUE until the next time update() is called
 */
bool DebouncedSwitch::isChanged() {
  return isSwitch(SwitchChanged);
}

/**
 * Returns true when the debounced state of the switch is DOWN (connected to GND)
 */
bool DebouncedSwitch::isDown() {
  return isSwitch(SwitchDown);
}

/**
 * private helper method
 */
bool DebouncedSwitch::isSwitch(unsigned char state) {
  return SwitchStatus & _BV(state);
}

/**
 * Construct a debounced switch that monitors this pin
 */
DebouncedSwitch::DebouncedSwitch(byte pin, bool _ACinput) {
  ACinput = _ACinput;
  switchPin = pin;
  pinMode(switchPin, INPUT_PULLUP);
  SwitchStatus = 0;

  // set past debouce count so do not fire another switch changed
  timerRunning = false;

  byte pinInput = !digitalRead(switchPin); // digitalRead returns HIGH (0x01) if switch open, LOW (0x00) if switch connected to GND
  // pinInput is 0 for switch open and 1 for switched connected to GND
  if (pinInput) {
    // update switch down and changed
    union _reg_SwitchStatus_Union switchUnion;
    switchUnion._reg = SwitchStatus;
    switchUnion._struct.lastSwitchReading = 1;
    switchUnion._struct.Down = 1;
    switchUnion._struct.Changed = 1;
    SwitchStatus = switchUnion._reg;
  }
}

/**
 * Update switch state.<br>
 * Need to call this from loop() so that it is executed every loop
 */
void DebouncedSwitch::update(void) {
  // clear debouced switch changed and press timed out  every loop
  union _reg_SwitchStatus_Union switchUnion;
  switchUnion._reg = SwitchStatus;
  switchUnion._struct.Changed = 0;
  switchUnion._struct.Pressed = 0;
  switchUnion._struct.DownRepeat = 0;
  switchUnion._struct.UpTimeout = 0;
  SwitchStatus = switchUnion._reg;

  long ms = millis(); // only call this once
  byte pinInput = !digitalRead(switchPin); // digitalRead returns HIGH (0x01) if switch open, LOW (0x00) if switch connected to GND
  // pinInput is 0 for switch open and 1 for switched connected to GND

  if (ACinput) {
    if (pinInput) {
      // have seen AC pulse or noise
      if (switchUnion._struct.Down) {
	  	// debounced output was down/on/AC present
        // and have seen another AC pulse so reset timeout timer 
        lastChangeMillis = ms; // start timer again for this pulse
        timerRunning = true;
        switchUnion._struct.lastSwitchReading = 0; // force start of AC_DEBOUNCE_LENGTH time next AC pulse see while debounce is OFF/UP
      } else {
      	// debounced output was off have seen another AC pulse so reset short timer
      	powerOnTimerStart = ms;
      	powerOnTimerRunning = true;
      	if (!switchUnion._struct.lastSwitchReading) { 
      	  // this is the first AC pulse seen short timer timed out
          lastChangeMillis = ms;
          switchUnion._struct.lastSwitchReading = 1;
          timerRunning = true;
        }  
      }	  
	}	
	if (powerOnTimerRunning && ((ms - powerOnTimerStart) > POWER_ON_CYCLE_LENGTH)) {
	  powerOnTimerRunning = false;
	  // missing AC pulse for 2 cycles
	  // if was AC off stop timeout to turn on 
      if (!switchUnion._struct.Down) {
      	// was AC off and no AC pulses
        timerRunning = false;
        switchUnion._struct.lastSwitchReading = 0; // force start of AC_DEBOUNCE_LENGTH time next AC pulse
      }
    }
    
	// have we timed out?
	if (timerRunning && ((ms - lastChangeMillis) > AC_DEBOUNCE_LENGTH)) {
	  powerOnTimerRunning = false;
  	  timerRunning = false;
      switchUnion._struct.lastSwitchReading = 0; // force start of AC_DEBOUNCE_LENGTH time next AC pulse
	  if (switchUnion._struct.Down) {
	  	// debounced output was down/on/AC present
        // > 25 cycles since last AC pulse reset timer, turn off debounced
        // i.e. AC is missing
        switchUnion._struct.Down = 0; //de-bounced OFF
		switchUnion._struct.Changed = 1; //de-bounced switch changed
	  } else {
	  	// debounced output was up/off/AC missing
	  	// have seen AC pulses for > 25 cycles so turn output on
        switchUnion._struct.Down = 1; //de-bounced
        switchUnion._struct.Changed = 1; //de-bounced switch changed
      }        
	} // else not timed out

  } else {
    // not AC just use debounce
    if (switchUnion._struct.lastSwitchReading != pinInput) { // switch changed reset time
      lastChangeMillis = ms;
      switchUnion._struct.lastSwitchReading = pinInput;
      timerRunning = true;
    } else {  // no change in switch since last check has the debounce timed out
      if (pinInput && (!switchUnion._struct.Down)
          && (timerRunning && ((ms - lastChangeMillis) > Debounce_mS_Down))) { // was debounced up and now down
        switchUnion._struct.Down = 1; //de-bounced
        switchUnion._struct.Changed = 1; //de-bounced switch changed
        timerRunning = false;
      } else if (!pinInput && (switchUnion._struct.Down)
                 && (timerRunning && ((ms - lastChangeMillis) > Debounce_mS_Up))) { // was debounced down and now up
        switchUnion._struct.Down = 0; //de-bounced
        switchUnion._struct.Changed = 1; //de-bounced switch changed
        timerRunning = false;
      }
    }
  }
  SwitchStatus = switchUnion._reg;
}


