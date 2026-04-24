#include <Arduino.h>
#include <BLE.h>
#include <Adafruit_NeoPixel.h>

#include <BackgroundAudioSpeech.h>
#include <libespeak-ng/voice/en_029.h>
#include <libespeak-ng/voice/en_gb_scotland.h>
#include <libespeak-ng/voice/en_gb_x_gbclan.h>
#include <libespeak-ng/voice/en_gb_x_gbcwmd.h>
#include <libespeak-ng/voice/en_gb_x_rp.h>
#include <libespeak-ng/voice/en.h>
#include <libespeak-ng/voice/en_shaw.h>
#include <libespeak-ng/voice/en_us.h>
#include <libespeak-ng/voice/en_us_nyc.h>
#include <I2S.h>
#include <BackgroundAudio.h>

// ======================================================
// NEOPIXEL
// ======================================================
#define PIXEL_PIN 28
#define NUM_PIXELS 1
Adafruit_NeoPixel pixel(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ======================================================
// BLE UUIDS
// ======================================================
const char *ALERT_SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
const char *ALERT_EVENT_UUID   = "19B10001-E8F2-537E-4F6C-D104768A1214";
const char *STATUS_UUID        = "19B10002-E8F2-537E-4F6C-D104768A1214";

// ======================================================
// AUDIO
// ======================================================
#define BCLK 26
#define I2SDATA 21
#define STREAMBUFF (16 * 1024)

I2S audio(OUTPUT, BCLK, I2SDATA);
BackgroundAudioSpeech BMP(audio);
BackgroundAudioMP3Class<RawDataBuffer<STREAMBUFF>> mp3(audio);

BackgroundAudioVoice v[] = {
  voice_en_029,
  voice_en_gb_scotland,
  voice_en_gb_x_gbclan,
  voice_en_gb_x_gbcwmd,
  voice_en,
  voice_en_shaw,
  voice_en_us,
  voice_en_us_nyc
};

// ======================================================
// BLE STATE
// ======================================================
BLEAdvertising connectedDevice;
BLERemoteService *alertService = nullptr;
BLERemoteCharacteristic *alertEvent = nullptr;
BLERemoteCharacteristic *statusChar = nullptr;

bool isConnected = false;
uint32_t lastStatusRead = 0;
uint32_t lastSpeechTime = 0;
const uint32_t SPEECH_COOLDOWN = 1200;

// ======================================================
// NEOPIXEL ALERT
// ======================================================
void alertFlash(int times, int delayTime) {
  for (int i = 0; i < times; i++) {
    pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // RED
    pixel.show();
    delay(delayTime);

    pixel.clear();
    pixel.show();
    delay(delayTime);
  }
}

// ======================================================
// HELPERS
// ======================================================
String orientationToSpeech(String orientation) {
  if (orientation == "FACE_UP") return "face up";
  if (orientation == "FACE_DOWN") return "face down";
  if (orientation == "LEFT") return "left";
  if (orientation == "RIGHT") return "right";
  if (orientation == "FRONT") return "front";
  if (orientation == "BACK") return "back";
  return "unknown orientation";
}

void speakShakeAlert(const String &orientation, const String &rtcTime) {
  if (millis() - lastSpeechTime < SPEECH_COOLDOWN) return;
  lastSpeechTime = millis();

  String phrase = "Shake detected. Orientation " + orientationToSpeech(orientation);
  BMP.speak(phrase.c_str());

  Serial.print("RTC timestamp received: ");
  Serial.println(rtcTime);
}

void parseAndHandleMessage(const String &msg) {
  Serial.print("Received: ");
  Serial.println(msg);

  int firstComma = msg.indexOf(',');
  int secondComma = msg.indexOf(',', firstComma + 1);

  if (firstComma == -1 || secondComma == -1) {
    Serial.println("Invalid message format");
    return;
  }

  String eventType   = msg.substring(0, firstComma);
  String orientation = msg.substring(firstComma + 1, secondComma);
  String rtcTime     = msg.substring(secondComma + 1);

  if (eventType == "SHAKE") {
    Serial.println("SHAKE received on Central");

    alertFlash(3, 120);          // NeoPixel instead of LED
    speakShakeAlert(orientation, rtcTime); // Audio
  }
}

void onAlertNotify(BLERemoteCharacteristic *c, const uint8_t *data, uint32_t len) {
  String msg = "";
  for (uint32_t i = 0; i < len; i++) msg += (char)data[i];
  parseAndHandleMessage(msg);
}

bool connectToPeripheral() {
  Serial.println("Scanning for peripheral...");

  auto report = BLE.scan(BLEUUID(ALERT_SERVICE_UUID), 5);
  if (!report || report->empty()) return false;

  connectedDevice = report->front();

  if (!BLE.client()->connect(connectedDevice, 10)) return false;

  alertService = BLE.client()->service(BLEUUID(ALERT_SERVICE_UUID));
  if (!alertService) return false;

  alertEvent = alertService->characteristic(BLEUUID(ALERT_EVENT_UUID));
  if (!alertEvent) return false;

  statusChar = alertService->characteristic(BLEUUID(STATUS_UUID));

  alertEvent->onNotify(onAlertNotify);
  alertEvent->enableNotifications();

  isConnected = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  // NeoPixel init
  pixel.begin();
  pixel.clear();
  pixel.show();

  BMP.setVoice(v[1]);
  BMP.begin();

  BLE.begin();
}

void loop() {
  if (!isConnected || !BLE.client()->connected()) {
    isConnected = false;
    BLE.client()->disconnect();

    if (!connectToPeripheral()) {
      delay(2000);
      return;
    }
  }

  delay(100);
}
