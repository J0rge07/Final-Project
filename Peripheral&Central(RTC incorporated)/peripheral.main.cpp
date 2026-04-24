#include <BLE.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_LSM6DSOX.h>
#include <math.h>

Adafruit_LSM6DSOX imu;
RTC_DS1307 rtc;

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

bool advertisingOn = true;
uint32_t lastShakeTime = 0;
uint32_t lastHeartbeat = 0;

// ======================================================
// SHAKE / ORIENTATION SETTINGS
// ======================================================
const float SHAKE_THRESHOLD = 18.0;
const uint32_t SHAKE_COOLDOWN = 1200;

// orientation thresholds
const float MAIN_AXIS_MIN = 7.0;
const float OTHER_AXIS_MAX = 4.0;   // relaxed a little to reduce UNKNOWNs

// delay after shake before sampling orientation
const uint32_t ORIENTATION_SAMPLE_DELAY = 180;

// ======================================================
// IMU SETUP
// ======================================================
void setupIMU() {
  if (!imu.begin_I2C(0x6B)) {
    Serial.println("Failed to find IMU!");
    while (1) delay(10);
  }

  Serial.println("IMU Found!");
  imu.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
}

// ======================================================
// RTC SETUP
// ======================================================
void setupRTC() {
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1) delay(10);
  }

  Serial.println("RTC ready");

  // Uncomment once if your RTC needs initial time set,
  // then comment it back out and upload again.
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

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

String detectOrientation(float ax, float ay, float az) {
  // choose dominant axis first
  float absX = fabs(ax);
  float absY = fabs(ay);
  float absZ = fabs(az);

  if (absZ >= absX && absZ >= absY && absZ >= MAIN_AXIS_MIN) {
    return (az > 0) ? "FACE_UP" : "FACE_DOWN";
  }

  if (absX >= absY && absX >= absZ && absX >= MAIN_AXIS_MIN) {
    return (ax > 0) ? "RIGHT" : "LEFT";
  }

  if (absY >= absX && absY >= absZ && absY >= MAIN_AXIS_MIN) {
    return (ay > 0) ? "FRONT" : "BACK";
  }

  return "UNKNOWN";
}

bool readAccel(float &ax, float &ay, float &az, float &magnitude) {
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  imu.getEvent(&accel, &gyro, &temp);

  ax = accel.acceleration.x;
  ay = accel.acceleration.y;
  az = accel.acceleration.z;

  magnitude = sqrt(ax * ax + ay * ay + az * az);
  return true;
}

bool shakeDetected(float &ax, float &ay, float &az, float &magnitude) {
  readAccel(ax, ay, az, magnitude);

  if (magnitude > SHAKE_THRESHOLD && (millis() - lastShakeTime > SHAKE_COOLDOWN)) {
    lastShakeTime = millis();
    return true;
  }

  return false;
}

String getRTCStamp() {
  DateTime now = rtc.now();

  char buffer[32];
  sprintf(buffer, "%02d-%02d-%04d %02d:%02d:%02d",
          now.month(), now.day(), now.year(),
          now.hour(), now.minute(), now.second());

  return String(buffer);
}

String getStableOrientation() {
  delay(ORIENTATION_SAMPLE_DELAY);

  float ax1, ay1, az1, mag1;
  float ax2, ay2, az2, mag2;
  float ax3, ay3, az3, mag3;

  readAccel(ax1, ay1, az1, mag1);
  delay(40);
  readAccel(ax2, ay2, az2, mag2);
  delay(40);
  readAccel(ax3, ay3, az3, mag3);

  float avgX = (ax1 + ax2 + ax3) / 3.0;
  float avgY = (ay1 + ay2 + ay3) / 3.0;
  float avgZ = (az1 + az2 + az3) / 3.0;

  Serial.print("Stable avg X: "); Serial.print(avgX);
  Serial.print(" Y: "); Serial.print(avgY);
  Serial.print(" Z: "); Serial.println(avgZ);

  return detectOrientation(avgX, avgY, avgZ);
}

void sendShakeAlert(const String &orientation, const String &rtcStamp) {
  String payload = "SHAKE," + orientation + "," + rtcStamp;
\
  alertEvent.setValue(payload);
  statusChar.setValue("ALERT_SENT");

  Serial.print("Sent BLE alert: ");
  Serial.println(payload);
}

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  setupRTC();
  setupIMU();

  BLE.setSecurity(BLESecurityJustWorks);
  BLE.begin("PICO_SHAKE_PERIPHERAL");

  alertService.addCharacteristic(&alertEvent);
  alertService.addCharacteristic(&statusChar);
  BLE.server()->addService(&alertService);

  alertEvent.setValue("READY,UNKNOWN,00-00-0000 00:00:00");
  statusChar.setValue("READY");

  BLE.startAdvertising();
  Serial.println("Peripheral Ready and Advertising");
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  if (BOOTSEL) {
    while (BOOTSEL) {}

    if (advertisingOn) {
      BLE.stopAdvertising();
      Serial.println("Stopping advertising");
    } else {
      BLE.startAdvertising();
      Serial.println("Starting advertising");
    }
    advertisingOn = !advertisingOn;
  }

  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    statusChar.setValue("ARMED");
    Serial.println("Status: ARMED");
  }

  float ax, ay, az, magnitude;

  if (shakeDetected(ax, ay, az, magnitude)) {
    Serial.println("SHAKE DETECTED (Peripheral)");
    Serial.print("Shake magnitude: ");
    Serial.println(magnitude);

    String orientation = getStableOrientation();
    String rtcStamp = getRTCStamp();

    Serial.print("Orientation: ");
    Serial.println(orientation);
    Serial.print("RTC time: ");
    Serial.println(rtcStamp);

    sendShakeAlert(orientation, rtcStamp);
    blinkLED(2, 100, 100);
  }

  delay(50);
}