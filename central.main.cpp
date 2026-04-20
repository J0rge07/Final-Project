#include <Arduino.h>
#include <BLE.h>
#include <Adafruit_LSM6DSOX.h>

const char *myserviceuuid = "3687d5e6-22a7-4b4d-a1e6-60d516e63649";
const char *strDatauuid   = "34e0e5b7-1a4a-4ea6-b0c8-df0f50e8a0ce";

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

// ===== NOTIFY CALLBACK =====
void notify(BLERemoteCharacteristic *c, const uint8_t *data, uint32_t len) {
  String msg = "";
  for (uint32_t i = 0; i < len; i++) msg += (char)data[i];

  Serial.print("Received: "); Serial.println(msg);

  if (msg.startsWith("SHAKE")) {
    Serial.println("SHAKE received on Central");

    // LED blink (SHORT)
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);

    // TODO: Add Neopixel + Speaker here
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(imuPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(imuPin), handleIMUTrigger, FALLING);

  setupIMU();

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

  auto strData = svc->characteristic(BLEUUID(strDatauuid));
  if (!strData) return;

  // Enable notifications
  strData->onNotify(notify);
  strData->enableNotifications();

  // ===== SEND SHAKE =====
  if (imuTriggered) {
    imuTriggered = false;

    String timestamp = String(millis() / 1000);
    String message = "SHAKE|" + timestamp;

    Serial.print("Sending: "); Serial.println(message);

    strData->setValue(message);
  }

  delay(5000); // allow time for notifications
  BLE.client()->disconnect();
}
