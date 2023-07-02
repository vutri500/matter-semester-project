#include <ArduinoBLE.h>
#include <Arduino_HTS221.h>

// variables for button:
long timestamp = 0;
int temp;

BLECharacteristic sensorCharacteristic;

void setup() {
  Serial.begin(9600);
  //while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT);

  // begin initialization HTS sensor
  if (!HTS.begin()) {
    Serial.println("Failed to initialize humidity temperature sensor!");
    while (1);
  }

  // initialize the BLE hardware
  BLE.begin();

  // start scanning for peripherals
  BLE.scanForUuid("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
  Serial.println("central is scanning");
}

void loop() {
  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    Serial.println("Got a peripheral");
    // peripheral detected, see if it's the right one:
    if (peripheral.localName() == "MY_BLE_BRIDGE") {
      Serial.println("Got the right peripheral");
      // If so, stop scanning and start communicating with it:
      BLE.stopScan();
      digitalWrite(LED_BUILTIN, HIGH);
      on_connect(peripheral);
    }
    // when the peripheral disconnects, resume scanning:
    digitalWrite(LED_BUILTIN, LOW);
    BLE.scanForUuid("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
  }
}

void on_connect(BLEDevice peripheral) {
  // connect to the peripheral
  Serial.println("Connecting ...");
  // if you can't connect, go back to the main loop:
  if (!peripheral.connect()) {
    Serial.println("Can't connect");
    return;
  }
  // If you can't discover peripheral attributes
  // go back to the main loop:
  if (!peripheral.discoverAttributes()) {
    Serial.println("Didn't discover attributes");
    peripheral.disconnect();
    return;
  }
  // when you know the UUIDs of the characteristics you want,
  // you can just pull them out of the remote peripheral like this:
  sensorCharacteristic = peripheral.characteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
  // this while loop will run as long as you're connected to the peripheral:
  while (peripheral.connected()) {
    // while the peripheral is connected, read temperature value every 2 seconds
    if (millis() - timestamp > 2000){
      timestamp = millis();
      temp = (int)(HTS.readTemperature());
      String temp_string = String(temp);
      Serial.println(temp);
      sensorCharacteristic.writeValue(temp_string .c_str(), temp_string .length());
    }
  }
}
/*
// these functions ar for exploring a remote peripheral's
// services and characteristics:
void exploreService(BLEService service) {
  // loop the characteristics of the service and explore each
  for (int i = 0; i < service.characteristicCount(); i++) {
    BLECharacteristic characteristic = service.characteristic(i);
    exploreCharacteristic(characteristic);
  }
}

void exploreCharacteristic(BLECharacteristic characteristic) {
  // check if the characteristic is readable
  String thisUuid = String(characteristic.uuid());
  if (characteristic.canSubscribe() &&
      // does it match the UUID you care about?
      thisUuid == "473dab7c-c93a-11e9-a32f-2a2ae2dbcce4") {
    Serial.println("can subscribe");
    // read the characteristic value
    buttonCharacteristic = characteristic;
    if (buttonCharacteristic.subscribe()) {
      Serial.println("button subscribed");
    }
  }
  // Check if it's writable:
  if (characteristic.canWrite() &&
      // does it match the UUID you care about?
      thisUuid == "473dacc6-c93a-11e9-a32f-2a2ae2dbcce4") {
    ledCharacteristic = characteristic;
    Serial.println("led available");
  }
}
*/