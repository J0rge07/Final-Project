#include <BLE.h>
#include <Adafruit_LSM6DSOX.h>
#include <math.h>

Adafruit_LSM6DSOX imu;


// BLE SETUP


// Custom alert service
BLEService alertService(BLEUUID("19B10000-E8F2-537E-4F6C-D104768A1214"));

// Characteristic used to send shake/orientation/timestamp messages
BLECharacteristic alertEvent(
  BLEUUID("19B10001-E8F2-537E-4F6C-D104768A1214"),
  BLERead | BLENotify,
  "Alert Event"
);

// Optional status/debug characteristic
BLECharacteristic statusChar(
  BLEUUID("19B10002-E8F2-537E-4F6C-D104768A1214"),
  BLERead | BLENotify,
  "Status"
);

bool advertisingOn = true;
uint32_t lastShakeTime = 0;
uint32_t lastHeartbeat = 0;


// SHAKE / ORIENTATION SETTINGS


const float SHAKE_THRESHOLD = 18.0;      // tune if needed
const uint32_t SHAKE_COOLDOWN = 1000;    // ms between valid shake events

// Orientation thresholds
const float MAIN_AXIS_MIN = 7.0;         // dominant axis must exceed this
const float OTHER_AXIS_MAX = 3.0;        // other axes should stay under this


// IMU SETUP


void setupIMU() {
  if (!imu.begin_I2C(0x6B)) {
    Serial.println("Failed to find IMU!");
    while (1) {
      delay(10);
    }
  }

  Serial.println("IMU Found!");
  imu.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
}


// HELPERS


void blinkLED(int times, int onTime, int offTime) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(onTime);
    digitalWrite(LED_BUILTIN, LOW);
    delay(offTime);
  }
}

String detectOrientation(float ax, float ay, float az) {
  // Z axis
  if (fabs(az) >= MAIN_AXIS_MIN && fabs(ax) <= OTHER_AXIS_MAX && fabs(ay) <= OTHER_AXIS_MAX) {
    return (az > 0) ? "FACE_UP" : "FACE_DOWN";
  }

  // X axis
  if (fabs(ax) >= MAIN_AXIS_MIN && fabs(ay) <= OTHER_AXIS_MAX && fabs(az) <= OTHER_AXIS_MAX) {
    return (ax > 0) ? "RIGHT" : "LEFT";
  }

  // Y axis
  if (fabs(ay) >= MAIN_AXIS_MIN && fabs(ax) <= OTHER_AXIS_MAX && fabs(az) <= OTHER_AXIS_MAX) {
    return (ay > 0) ? "FRONT" : "BACK";
  }

  return "UNKNOWN";
}

bool shakeDetected(float &ax, float &ay, float &az, float &magnitude) {
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  imu.getEvent(&accel, &gyro, &temp);

  ax = accel.acceleration.x;
  ay = accel.acceleration.y;
  az = accel.acceleration.z;

  magnitude = sqrt(ax * ax + ay * ay + az * az);

  if (magnitude > SHAKE_THRESHOLD && (millis() - lastShakeTime > SHAKE_COOLDOWN)) {
    lastShakeTime = millis();
    return true;
  }

  return false;
}

void sendShakeAlert(const String &orientation, uint32_t timestampMs) {
  String payload = "SHAKE," + orientation + "," + String(timestampMs);

  alertEvent.setValue(payload);
  statusChar.setValue("ALERT_SENT");

  Serial.print("Sent BLE alert: ");
  Serial.println(payload);
}


// SETUP


void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  setupIMU();

  BLE.setSecurity(BLESecurityJustWorks);
  BLE.begin("PICO_SHAKE_PERIPHERAL");

  alertService.addCharacteristic(&alertEvent);
  alertService.addCharacteristic(&statusChar);

  BLE.server()->addService(&alertService);

  alertEvent.setValue("READY,UNKNOWN,0");
  statusChar.setValue("READY");

  BLE.startAdvertising();
  Serial.println("Peripheral Ready and Advertising");
}


// LOOP


void loop() {
  // Optional: BOOTSEL toggles advertising on/off for testing
  if (BOOTSEL) {
    while (BOOTSEL) {
    }

    if (advertisingOn) {
      BLE.stopAdvertising();
      Serial.println("Stopping advertising");
    } else {
      BLE.startAdvertising();
      Serial.println("Starting advertising");
    }
    advertisingOn = !advertisingOn;
  }

  // Heartbeat status update every 5 seconds
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    statusChar.setValue("ARMED");
    Serial.println("Status: ARMED");
  }

  float ax, ay, az, magnitude;

  if (shakeDetected(ax, ay, az, magnitude)) {
    String orientation = detectOrientation(ax, ay, az);
    uint32_t timestampMs = millis();

    Serial.println("SHAKE DETECTED (Peripheral)");
    Serial.print("Magnitude: ");
    Serial.println(magnitude);

    Serial.print("Accel X: ");
    Serial.print(ax);
    Serial.print("  Y: ");
    Serial.print(ay);
    Serial.print("  Z: ");
    Serial.println(az);

    Serial.print("Orientation: ");
    Serial.println(orientation);

    Serial.print("Timestamp(ms): ");
    Serial.println(timestampMs);

    sendShakeAlert(orientation, timestampMs);

    // Local visual confirmation on sender board
    blinkLED(2, 100, 100);
  }

  delay(50);
}