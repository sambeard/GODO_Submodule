#define F_CPU 1000000
#include <TinyWireS.h>
#include <elapsedMillis.h>

#define DURATION 125 // 1000 / 8

bool was_pressed = false;
elapsedMillis sinceLastPress = DURATION;

uint8_t d_button;


void setup() {
  // initialize the digital pin as an output.
  pinMode(1, OUTPUT); //LED on Model A  or Pro
  pinMode(4, INPUT_PULLUP); //LED on Model A  or Pro
  TinyWireS.begin(9);
  delay(1000.);
  TinyWireS.onRequest(handleRequest);
  TinyWireS.onReceive(handleReceive);
}

// the loop routine runs over and over again forever:
void loop() {
  bool p = !digitalRead(4);
  if (p) {
    activateLight();
  }
  // on button release
  else if (was_pressed)
    d_button++;
  was_pressed = p;
  digitalWrite(1, sinceLastPress < DURATION);
}

void activateLight() {
  sinceLastPress = 0;
}

void handleReceive(uint8_t amount) {
  while(TinyWireS.available()){
    byte t = TinyWireS.receive();
    if(int(t) == 0xFA) {
      digitalWrite(1, HIGH);  
    }
  }
}

void handleRequest() {
  // do actions
  TinyWireS.send(d_button);
  d_button = 0;
  activateLight();
}
