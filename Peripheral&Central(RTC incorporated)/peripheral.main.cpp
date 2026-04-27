#include <BLE.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

Adafruit_LSM6DSOX imu;
RTC_DS1307 rtc;

// ======================================================
// NEOPIXEL
// ======================================================
#define PIXEL_PIN 28
#define NUM_PIXELS 1
Adafruit_NeoPixel pixel(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ======================================================
// BLE SETUP
// ======================================================
BLEService alertService(BLEUUID("19B10000-E8F2-537E-4F6C-D104768A1214"));

BLECharacteristic alertEvent(
  BLEUUID("19B10001-E8F2-537E-4F6C-D104768A1214"),
  BLERead | BLENotify,
  "Alert Event"
);

BLECharacteristic statusChar(
  BLEUUID("19B10002-E8F2-537E-4F6C-D104768A1214"),
  BLERead | BLENotify,
  "Status"
);

uint32_t lastShakeTime = 0;

// ======================================================
// SETTINGS
// ======================================================
const float SHAKE_THRESHOLD = 18.0;
const uint32_t SHAKE_COOLDOWN = 1200;

// ======================================================
// NEOPIXEL ALERT
// ======================================================
void alertFlash() {
  for (int i = 0; i < 2; i++) {
    pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // BLUE
    pixel.show();
    delay(120);

    pixel.clear();
    pixel.show();
    delay(120);
  }
}

// ======================================================
// IMU
// ======================================================
void setupIMU() {
  if (!imu.begin_I2C(0x6B)) {
    Serial.println("IMU fail");
    while (1);
  }
}

bool shakeDetected(float &mag) {
  sensors_event_t a, g, t;
  imu.getEvent(&a, &g, &t);

  mag = sqrt(a.acceleration.x * a.acceleration.x +
             a.acceleration.y * a.acceleration.y +
             a.acceleration.z * a.acceleration.z);

  if (mag > SHAKE_THRESHOLD && millis() - lastShakeTime > SHAKE_COOLDOWN) {
    lastShakeTime = millis();
    return true;
  }
  return false;
}

// ======================================================
// RTC
// ======================================================
void setupRTC() {
  Wire.begin();
  rtc.begin();
}

String getTime() {
  DateTime now = rtc.now();
  char buf[32];
  sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(buf);
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(2000);

  pixel.begin();
  pixel.clear();
  pixel.show();

  setupRTC();
  setupIMU();

  BLE.begin("PERIPHERAL");
  alertService.addCharacteristic(&alertEvent);
  alertService.addCharacteristic(&statusChar);
  BLE.server()->addService(&alertService);

  BLE.startAdvertising();
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  float mag;

  if (shakeDetected(mag)) {
    Serial.println("SHAKE!");

    String msg = "SHAKE,UNKNOWN," + getTime();

    alertEvent.setValue(msg);
    statusChar.setValue("SENT");

    alertFlash();  // NeoPixel instead of LED
  }

  delay(50);
}
