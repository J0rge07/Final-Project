#include <Arduino.h>
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

// ======================================================
// SETTINGS
// ======================================================
uint32_t lastShakeTime = 0;

const float SHAKE_THRESHOLD = 18.0;
const uint32_t SHAKE_COOLDOWN = 1200;

// orientation tuning
const float MAIN_AXIS_MIN = 6.5;       // slightly easier to classify
const float AXIS_MARGIN   = 2.0;       // dominant axis must beat others by this much
const uint32_t SETTLE_DELAY_MS = 180;  // wait after shake before classifying
const int ORIENTATION_SAMPLES = 5;

// ======================================================
// NEOPIXEL ALERT
// ======================================================
void alertFlash() {
  for (int i = 0; i < 2; i++) {
    pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // BLUE on sender
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
    while (1) {}
  }

  imu.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);

  Serial.println("IMU ready");
}

bool readAccel(float &ax, float &ay, float &az, float &mag) {
  sensors_event_t a, g, t;
  imu.getEvent(&a, &g, &t);

  ax = a.acceleration.x;
  ay = a.acceleration.y;
  az = a.acceleration.z;

  mag = sqrt(ax * ax + ay * ay + az * az);
  return true;
}

bool shakeDetected(float &mag) {
  float ax, ay, az;
  readAccel(ax, ay, az, mag);

  if (mag > SHAKE_THRESHOLD && (millis() - lastShakeTime > SHAKE_COOLDOWN)) {
    lastShakeTime = millis();
    return true;
  }
  return false;
}

// ======================================================
// ORIENTATION
// Uses the coordinate meaning from your reference code:
// az < 0 = up, az > 0 = down
// ax > 0 = right, ax < 0 = left
// ay > 0 = front, ay < 0 = back
// ======================================================
String detectOrientationStable() {
  delay(SETTLE_DELAY_MS);

  float sumX = 0, sumY = 0, sumZ = 0;

  for (int i = 0; i < ORIENTATION_SAMPLES; i++) {
    float ax, ay, az, mag;
    readAccel(ax, ay, az, mag);

    sumX += ax;
    sumY += ay;
    sumZ += az;

    delay(30);
  }

  float ax = sumX / ORIENTATION_SAMPLES;
  float ay = sumY / ORIENTATION_SAMPLES;
  float az = sumZ / ORIENTATION_SAMPLES;

  float absX = fabs(ax);
  float absY = fabs(ay);
  float absZ = fabs(az);

  Serial.print("Avg X: ");
  Serial.print(ax);
  Serial.print("  Y: ");
  Serial.print(ay);
  Serial.print("  Z: ");
  Serial.println(az);

  // dominant axis + margin check to avoid mixed orientations
  if (absZ >= MAIN_AXIS_MIN && absZ > absX + AXIS_MARGIN && absZ > absY + AXIS_MARGIN) {
    return (az < 0) ? "UP" : "DOWN";
  }

  if (absX >= MAIN_AXIS_MIN && absX > absY + AXIS_MARGIN && absX > absZ + AXIS_MARGIN) {
    return (ax > 0) ? "RIGHT" : "LEFT";
  }

  if (absY >= MAIN_AXIS_MIN && absY > absX + AXIS_MARGIN && absY > absZ + AXIS_MARGIN) {
    return (ay > 0) ? "FRONT" : "BACK";
  }

  // fallback: whichever axis is strongest
  if (absZ >= absX && absZ >= absY && absZ >= MAIN_AXIS_MIN) {
    return (az < 0) ? "UP" : "DOWN";
  }
  if (absX >= absY && absX >= absZ && absX >= MAIN_AXIS_MIN) {
    return (ax > 0) ? "RIGHT" : "LEFT";
  }
  if (absY >= absX && absY >= absZ && absY >= MAIN_AXIS_MIN) {
    return (ay > 0) ? "FRONT" : "BACK";
  }

  return "UNKNOWN";
}

// ======================================================
// RTC
// ======================================================
void setupRTC() {
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1) {}
  }

  Serial.println("RTC ready");

  // Uncomment once if your RTC needs to be set, then comment out again:
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
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
  pixel.setBrightness(50);
  pixel.show();

  setupRTC();
  setupIMU();

  BLE.begin("PERIPHERAL");

  alertService.addCharacteristic(&alertEvent);
  alertService.addCharacteristic(&statusChar);
  BLE.server()->addService(&alertService);

  alertEvent.setValue("READY,UNKNOWN,00:00:00");
  statusChar.setValue("READY");

  BLE.startAdvertising();
  Serial.println("Peripheral advertising");
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  float mag;

  if (shakeDetected(mag)) {
    Serial.print("SHAKE! Magnitude: ");
    Serial.println(mag);

    String orientation = detectOrientationStable();
    String rtcTime = getTime();

    String msg = "SHAKE," + orientation + "," + rtcTime;

    Serial.print("Sending: ");
    Serial.println(msg);

    alertEvent.setValue(msg);
    statusChar.setValue("SENT");

    alertFlash();
  }

  delay(50);
}