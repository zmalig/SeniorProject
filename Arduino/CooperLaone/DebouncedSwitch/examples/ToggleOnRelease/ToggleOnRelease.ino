#include <DebouncedSwitch.h>

/**
 * This example debounces a switch connected between D4 and GND
 * each time the switch is released the LED toggles from ON to OFF or vis versa
 */
int D4 = 4; // give the pin a name

DebouncedSwitch sw(D4); // monitor a switch on input D4

// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int led = 13;

// the setup routine runs once when you press reset:
void setup() {
  pinMode(led, OUTPUT); // initially low (OFF)
}

// the loop routine runs over and over again forever:
void loop() {
  sw.update(); // call this every loop to update switch state
  
  if (sw.isChanged()) { // debounced switch changed state Up or Down
    // isChanged() is only true for one loop(), cleared when update() called again
    if (!sw.isDown()) { // switch was just released
      // toggle the led
      // read current value and set opposite one
      digitalWrite(led, !digitalRead(led));
    }
  }
}


