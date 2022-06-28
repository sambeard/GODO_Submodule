#include <TinyWireS.h>
#include <elapsedMillis.h>
#include <Adafruit_NeoPixel.h>
#include <SharedState.h>

#define BUTTON_PIN 4
#define LED_PIN 1
#define LEDRING_PIN 3

#define I2C_ADR 9
#define LED_AMNT 16
#define FRAME_RATE_HZ 30

#define DEBUG_TIME 3000

elapsedMillis time;
elapsedMillis sinceLastFrame;
elapsedMillis sinceReceive = DEBUG_TIME;
// elapsedMillis confirmTimer = CONFIRMATION_TIME;
float gear_t =0;

bool RECEIVE_DATA_FLAG = false;



Frequency Freq;
uint8_t goal, completions, localMomentum, globalMomentum, displacement;
volatile byte received_data[5] = {0};

LMODE cmode;
Adafruit_NeoPixel _ring(LED_AMNT, LEDRING_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // initialize the digital pin as an output.
  pinMode(LED_PIN, OUTPUT); //LED on Model A  or Pro
  pinMode(BUTTON_PIN, INPUT_PULLUP); //LED on Model A  or Pro
  TinyWireS.begin(I2C_ADR);
  TinyWireS.onRequest(handleRequest);
  TinyWireS.onReceive(handleReceive);
  _ring.begin();
  _ring.clear();
  // init variables
  cmode = CONNECTED;
  goal, completions, localMomentum, globalMomentum, displacement = 0;
  gear_t =0;
  time = 0;
  sinceLastFrame = 0;
}

// the loop routine runs over and over again forever:
void loop() {
  if(RECEIVE_DATA_FLAG){
    sinceReceive = 0;
    process_data();
    RECEIVE_DATA_FLAG = false;
  }
  renderMode();
}

void process_data(){
  LMODE prev = cmode;
  cmode = LMODE(received_data[0] >> 4);
  Freq = Frequency(received_data[0] & 0xF);

  // parse goal and completions
  goal = received_data[1] >> 4;
  completions = received_data[1] & 0xF;

  // parse momentum
  localMomentum = received_data[2];
  globalMomentum = received_data[3];

  // parse displacement
  displacement = received_data[4];

  if((cmode == GEAR && cmode != prev) || cmode == CLEAR_WARNING || cmode == CLEAR_ON_SETUP_WARNING || cmode == CLEAR_PROGRESS_WARNING || cmode == RECONNECT){
    time = 0;
    gear_t = 0;
  }
}

void handleReceive(uint8_t amount) {
  if(amount == PACKET_SIZE){
    uint8_t i = 0;
    for (i = 0; i < amount && TinyWireS.available(); i++){
      received_data[i] = TinyWireS.receive();
    }
    if(i == amount){
      RECEIVE_DATA_FLAG = true;
    }
  }
}

void handleRequest() {
  // do actions
  if(true){
    uint8_t msg = 0;
    msg |= !digitalRead(BUTTON_PIN);
    msg |= 1 << (cmode +1);
    TinyWireS.send(msg);
  }
  else {
    for(uint8_t i=0; i<PACKET_SIZE;i++){
      TinyWireS.send(received_data[i]);
    }
  }
}

void renderMode() {
  if(sinceLastFrame > (1000 / FRAME_RATE_HZ)){
    // general pre render
    _ring.clear();
      switch (cmode) {
        case GEAR:
          renderGear();
          break;
        case RECONNECT:
          renderReconnect();
          break;
        case SETUP:
          renderSetup();
          break;
        case CONNECTED:
          renderConnected();
          break;
        case PROGRESS:
          renderProgress();
          break;
        case COMPLETION:
          renderCompletion();
          break;
        case CLEAR_WARNING:
          renderClearWarning();
          break;
        case CLEAR_PROGRESS_WARNING:
        case CLEAR_ON_SETUP_WARNING:
          renderClearProgressWarning();
          break;
        default:
          break;
     }
    // general post render
    for (uint8_t i =0; i < LED_AMNT; i++){
      _ring.setPixelColor(i, Adafruit_NeoPixel::gamma32(_ring.getPixelColor(i)));
    }
    _ring.show();
    sinceLastFrame = 0;
  }
}

void renderGear() {
  uint8_t freqMod = (1 << int(Freq));
  gear_t += (sinceLastFrame / freqMod) * (1 + (globalMomentum*4. / 255.)) / GEAR_TIME_DIV;
  uint16_t hue = goal == completions ? COMPLETION_HUE : HueForFreq(Freq);
  uint8_t sat = goal == completions ? 200: 35 + map(localMomentum, 0, SatForFreq(Freq));
  for (int i = 0; i < LED_AMNT / 4; i++) {
    // loop over display
    for (int j = 0; j < 3; j++) {
      int head = (LED_AMNT -1)  - ((i * 4 + j + displacement + int(gear_t)) % LED_AMNT);
      float alpha = gear_t - int(gear_t);
      float brightness = 1;
      if (j != 1) {
        // 0 for first, 1 for second
        int ind = (j / 2) % 2;
        brightness = (1 - ind) + (-1 + 2 * ind) * alpha;
        brightness = pow(brightness, 2.5);
      }
  
      _ring.setPixelColor(head, _ring.ColorHSV(hue << 8, sat , uint8_t(brightness *DEFAULT_BRIGHTNESS)));
    }
  }
}

void renderConnected() {
  long t = time / BLINK_TIME_DIV;
  for(int i = 0; i < LED_AMNT; i++){
    _ring.setPixelColor(i,_ring.ColorHSV(0,0,map(Adafruit_NeoPixel::sine8(t),50,80)));
  }
}

void renderReconnect() {
  long t = time/ BLINK_TIME_DIV;
  _ring.fill(_ring.ColorHSV(HueForFreq(Freq) << 8, SatForFreq(Freq), DEFAULT_BRIGHTNESS - map(Adafruit_NeoPixel::sine8(t), 0, DEFAULT_BRIGHTNESS/2)));
}

void renderSetup() {
  _ring.fill(_ring.ColorHSV(HueForFreq(Freq) << 8, SatForFreq(Freq), DEFAULT_BRIGHTNESS));
}

void renderProgress() {
    uint8_t w = LED_AMNT / goal;           // width
    uint8_t r = LED_AMNT % goal;           // remainder
    uint8_t s = progSpaceBetween(w,r, goal);
    uint32_t col;
    for(uint8_t i = 0; i < goal; i++){
      col = completions > i? _ring.ColorHSV(COMPLETION_HUE << 8,255,255) : _ring.ColorHSV(HueForFreq(Freq) << 8, SatForFreq(Freq), DEFAULT_BRIGHTNESS);
      _ring.fill(col , progStartPoint(i,w,r,goal),goal>1?w-s:w);
    }
}

void renderCompletion() {
    _ring.fill(_ring.ColorHSV(135<<8,255,255));
}

void renderClearWarning() {
    uint8_t t = time / BLINK_FAST_TIME_DIV;
    uint8_t v = (time > LONG_PRESS_TIME - SHORT_PRESS_TIME)?255:Adafruit_NeoPixel::sine8(t);
    _ring.fill(_ring.ColorHSV(WARNING_HUE << 8,255,v));
}
void renderClearProgressWarning() {
    float prog = 0.1 + 0.9 * float(time) / (LONG_PRESS_TIME- SHORT_PRESS_TIME);
    _ring.fill(_ring.ColorHSV(WARNING_HUE << 8,255,constrain(prog*2,0,1) * 255), 0, prog * LED_AMNT);
}
