#include <Arduino.h>
#include <BLE.h>

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
// BLE UUIDS - must match the peripheral
// ======================================================
const char *ALERT_SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
const char *ALERT_EVENT_UUID   = "19B10001-E8F2-537E-4F6C-D104768A1214";
const char *STATUS_UUID        = "19B10002-E8F2-537E-4F6C-D104768A1214";

// ======================================================
// SPEAKER / I2S SETUP
// Keep your working structure
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

// optional debounce so repeated messages do not pile up speech
const uint32_t SPEECH_COOLDOWN = 1200;

// ======================================================
// HELPERS
// ======================================================
void blinkLED(int times, int onTime, int offTime) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(onTime);
    digitalWrite(LED_BUILTIN, LOW);
    delay(offTime);
  }
}

String orientationToSpeech(String orientation) {
  if (orientation == "FACE_UP") return "face up";
  if (orientation == "FACE_DOWN") return "face down";
  if (orientation == "LEFT") return "left";
  if (orientation == "RIGHT") return "right";
  if (orientation == "FRONT") return "front";
  if (orientation == "BACK") return "back";
  return "unknown orientation";
}

void speakShakeAlert(const String &orientation, const String &timestamp) {
  if (millis() - lastSpeechTime < SPEECH_COOLDOWN) {
    return;
  }
  lastSpeechTime = millis();

  String phrase = "Shake detected. Orientation " + orientationToSpeech(orientation);
  BMP.speak(phrase.c_str());

  Serial.print("Spoken alert at timestamp ms: ");
  Serial.println(timestamp);
}

void parseAndHandleMessage(const String &msg) {
  Serial.print("Received: ");
  Serial.println(msg);

  // Expected format: SHAKE,ORIENTATION,TIMESTAMP
  int firstComma = msg.indexOf(',');
  int secondComma = msg.indexOf(',', firstComma + 1);

  if (firstComma == -1 || secondComma == -1) {
    Serial.println("Invalid message format");
    return;
  }

  String eventType   = msg.substring(0, firstComma);
  String orientation = msg.substring(firstComma + 1, secondComma);
  String timestamp   = msg.substring(secondComma + 1);

  if (eventType == "SHAKE") {
    Serial.println("SHAKE received on Central");
    Serial.print("Orientation: ");
    Serial.println(orientation);
    Serial.print("Timestamp(ms): ");
    Serial.println(timestamp);

    blinkLED(2, 120, 120);

    // This is where NeoPixel code can go later
    // triggerNeoPixelAlert(orientation);

    speakShakeAlert(orientation, timestamp);
  }
}

// ======================================================
// NOTIFY CALLBACK
// ======================================================
void onAlertNotify(BLERemoteCharacteristic *c, const uint8_t *data, uint32_t len) {
  String msg = "";
  for (uint32_t i = 0; i < len; i++) {
    msg += (char)data[i];
  }

  parseAndHandleMessage(msg);
}

// ======================================================
// CONNECT TO PERIPHERAL
// ======================================================
bool connectToPeripheral() {
  Serial.println("Scanning for peripheral...");

  auto report = BLE.scan(BLEUUID(ALERT_SERVICE_UUID), 5);

  if (!report || report->empty()) {
    Serial.println("No peripheral found");
    return false;
  }

  connectedDevice = report->front();

  Serial.println("Peripheral found, connecting...");

  if (!BLE.client()->connect(connectedDevice, 10)) {
    Serial.println("Connection failed");
    return false;
  }

  Serial.println("Connected to peripheral");

  alertService = BLE.client()->service(BLEUUID(ALERT_SERVICE_UUID));
  if (!alertService) {
    Serial.println("Alert service not found");
    BLE.client()->disconnect();
    return false;
  }

  alertEvent = alertService->characteristic(BLEUUID(ALERT_EVENT_UUID));
  if (!alertEvent) {
    Serial.println("Alert characteristic not found");
    BLE.client()->disconnect();
    return false;
  }

  statusChar = alertService->characteristic(BLEUUID(STATUS_UUID));

  alertEvent->onNotify(onAlertNotify);
  alertEvent->enableNotifications();

  Serial.println("Notifications enabled");
  isConnected = true;
  return true;
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  BMP.setVoice(v[1]);

  if (!BMP.begin()) {
    Serial.println("Failed to start BMP");
  } else {
    BMP.speak("Central board ready");
  }

  BLE.begin();
  Serial.println("BLE Central ready");
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  if (!isConnected || !BLE.client()->connected()) {
    isConnected = false;

    if (BLE.client()->connected()) {
      BLE.client()->disconnect();
    }

    if (!connectToPeripheral()) {
      delay(2000);
      return;
    }
  }

  // optional status read every few seconds
  if (statusChar && millis() - lastStatusRead > 5000) {
    lastStatusRead = millis();
    String status = statusChar->getString();
    Serial.print("Peripheral status: ");
    Serial.println(status);
  }

  delay(100);
}