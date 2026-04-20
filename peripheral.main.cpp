#include <Arduino.h>
#include <BLE.h>
#include <Adafruit_LSM6DSOX.h>
#include <I2S.h> // for the speaker 

// the pins ( maybe need to change)
#define I2S_BCLK 26
#define I2S_LRC  27  
#define I2S_DOUT 28

const int sampleRate = 16000;

void setup() {
  Serial.begin(115200);
  
  // Inicialize I2S
  I2S.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
  if (!I2S.begin(sampleRate, 16)) {
    Serial.println("Falha no I2S!");
    while(1);
  }
  
  Serial.println("Speaker pronto!");
}

void loop() {
  // play a beep ( test )
  for(int i = 0; i < 3; i++) {
    playTone(880, 200);   // 880Hz por 200ms
    delay(100);
  }
  
  delay(3000);
}

void playTone(int frequency, int durationMs) {
  int numSamples = (sampleRate * durationMs) / 1000;
  int16_t* buffer = (int16_t*)malloc(numSamples * sizeof(int16_t));
  
  for (int i = 0; i < numSamples; i++) {
    float t = (float)i / sampleRate;
    buffer[i] = (int16_t)(8000 * sin(2 * 3.14159 * frequency * t));
  }
  
  I2S.write(buffer, numSamples);
  free(buffer);
}

const char *myserviceuuid = "3687d5e6-22a7-4b4d-a1e6-60d516e63649";
const char *strDatauuid   = "34e0e5b7-1a4a-4ea6-b0c8-df0f50e8a0ce";