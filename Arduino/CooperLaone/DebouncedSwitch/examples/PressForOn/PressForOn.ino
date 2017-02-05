#include <DebouncedSwitch.h>

/**
 * This example debounces a switch connected between D4 and GND
 * when the switch is pressed the LED is turned on
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
  
  if (sw.isDown()) { // debounced switch is down
    digitalWrite(led, HIGH);
  } else {
    digitalWrite(led, LOW);      
  }
}


