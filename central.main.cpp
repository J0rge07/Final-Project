#include <Arduino.h>
#include <BLE.h>
#include <Adafruit_LSM6DSOX.h>
#include <Wire.h>
#include <RTClib.h>

// ===== BLE UUIDs =====
const char *myserviceuuid = "3687d5e6-22a7-4b4d-a1e6-60d516e63649";
const char *strDatauuid   = "34e0e5b7-1a4a-4ea6-b0c8-df0f50e8a0ce";

// ===== RTC =====
RTC_DS3231 rtc;

// ===== RTC HELPER =====
String getTimeString() {
  DateTime now = rtc.now();

  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
           now.hour(), now.minute(), now.second());

  return String(buffer);
}

// ===== IMU =====
const int imuPin = 20;
Adafruit_LSM6DSOX imu;
volatile bool imuTriggered = false;

void handleIMUTrigger() {
  imuTriggered = true;
}

void setupIMU() {
  if (!imu.begin_I2C(0x6B)) {
    Serial.println("IMU not found");
    while (1);
  }

  imu.configIntOutputs(true, false);
  imu.configInt1(false, false, false, false, true);
  imu.enableWakeup(true, 20, 1);
}

// ===== BLE NOTIFY =====
void notify(BLERemoteCharacteristic *c, const uint8_t *data, uint32_t len) {
  String msg;
  for (uint32_t i = 0; i < len; i++) msg += (char)data[i];

  Serial.print("Received: ");
  Serial.println(msg);

  if (msg.startsWith("SHAKE")) {
    Serial.println("SHAKE received on Central");

    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(imuPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(imuPin), handleIMUTrigger, FALLING);

  setupIMU();

  // ===== RTC INIT (MATCH PERIPHERAL STYLE) =====
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  BLE.begin();
}

// ===== LOOP =====
void loop() {
  Serial.println("Scanning...");

  auto report = BLE.scan(BLEUUID(myserviceuuid), 5);
  if (report->empty()) {
    delay(2000);
    return;
  }

  BLEAdvertising dev = report->front();
  BLE.client()->connect(dev, 10);

  auto svc = BLE.client()->service(BLEUUID(myserviceuuid));
  if (!svc) return;

  auto characteristic = svc->characteristic(BLEUUID(strDatauuid));
  if (!characteristic) return;

  characteristic->onNotify(notify);
  characteristic->enableNotifications();

  // ===== SEND SHAKE WITH RTC =====
  if (imuTriggered) {
    imuTriggered = false;

    String timestamp = getTimeString();
    String message = "SHAKE|" + timestamp;

    Serial.print("Sending: ");
    Serial.println(message);

    characteristic->setValue(message);
  }

  delay(5000);
  BLE.client()->disconnect();
}
