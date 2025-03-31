#include <BLEDevice.h>
#include <BLEServer.h>
//#include <BLEUtils.h>
//#include <BLE2902.h>
#include <M5StickCPlus.h>

#include <Wire.h>

//Default Temperature in Celsius
#define temperatureCelsius

//change to unique BLE server name
#define bleServerName "CSC2106-BLE#01AMOS"

float tempC = 25.0;
float tempF;
float vBatt = 5.0;  // initial value

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 15000;   // update refresh every 15sec

bool deviceConnected = false;
bool ledState = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "01234567-0123-4567-89ab-0123456789ab"

// Temperature Characteristic and Descriptor
#ifdef temperatureCelsius
  BLECharacteristic imuTemperatureCelsiusCharacteristics("01234567-0123-4567-89ab-0123456789cd", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor imuTemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2902));
#else
  BLECharacteristic imuTemperatureFahrenheitCharacteristics("01234567-0123-4567-89ab-0123456789de", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor imuTemperatureFahrenheitDescriptor(BLEUUID((uint16_t)0x2902));
#endif

// Battery Voltage Characteristic and Descriptor
BLECharacteristic axpVoltageCharacteristics("01234567-0123-4567-89ab-0123456789ef", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor axpVoltageDescriptor(BLEUUID((uint16_t)0x2903));

// LED Characteristic and Descriptor
BLECharacteristic ledCharacteristics("01234567-0123-4567-89ab-0123456789ff", BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor ledDescriptor(BLEUUID((uint16_t)0x2904));

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("MyServerCallbacks::Connected...");
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("MyServerCallbacks::Disconnected...");
    // Restart advertising
    pServer->getAdvertising()->start();
  }
};

class LedCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // Get the value written by the client
    std::string value = pCharacteristic->getValue();
    Serial.print("Received data: ");
    Serial.println(value.c_str());

    // Example: Control LED based on received value
    if (value == "1") {
      digitalWrite(10, LOW);  // Turn on the LED
      Serial.println("LED turned ON");
    } else if (value == "0") {
      digitalWrite(10, HIGH);   // Turn off the LED
      Serial.println("LED turned OFF");
    }

  }
};

void setup() {
  // Start serial communication 
  Serial.begin(115200);
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);

  // put your setup code here, to run once:
  M5.begin();
  M5.IMU.Init();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.printf("BLE Server", 0);

  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  

  // Create the BLE Service
  BLEService *bleService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics and Create a BLE Descriptor
  // Temperature
  #ifdef temperatureCelsius
    bleService->addCharacteristic(&imuTemperatureCelsiusCharacteristics);
    imuTemperatureCelsiusDescriptor.setValue("IMU Temperature(C)");
    imuTemperatureCelsiusCharacteristics.addDescriptor(&imuTemperatureCelsiusDescriptor);
  #else
    bleService->addCharacteristic(&imuTemperatureFahrenheitCharacteristics);
    imuTemperatureFahrenheitDescriptor.setValue("IMU Temperature(F)");
    imuTemperatureFahrenheitCharacteristics.addDescriptor(&imuTemperatureFahrenheitDescriptor);
  #endif  

  // Battery
  bleService->addCharacteristic(&axpVoltageCharacteristics);
  axpVoltageDescriptor.setValue("AXP Battery(V)");
  axpVoltageCharacteristics.addDescriptor(&axpVoltageDescriptor); 

  //LED
  bleService->addCharacteristic(&ledCharacteristics);
  ledDescriptor.setValue("LED Control");
  ledCharacteristics.addDescriptor(&ledDescriptor);

  ledCharacteristics.setCallbacks(new LedCharacteristicCallbacks());

  // Start the service
  bleService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (deviceConnected) {
    M5.update();
    if (M5.BtnA.isPressed()) {
      //Togggle LED state
      ledState = !ledState;
      if (ledState) {
        digitalWrite(10,LOW);
      } else {
        digitalWrite(10,HIGH);
      }
      // Read temperature as Celsius (the default)
        tempC = readTemperature();
      // Fahrenheit
      tempF = 1.8*tempC + 32;
      // Battery voltage
      if (vBatt<1.0)
        vBatt = 5.0;
      else
        vBatt -= 0.01;


      //Notify LED state
      static char led[2];
      led = ledState ? "1\0" : "0\0";

      ledCharacteristics.setValue(ledState ? "1\0" : "0\0");
      Serial.print(ledState ? "1" : "0");
      ledCharacteristics.notify();
  
      //Notify temperature reading from IMU
      #ifdef temperatureCelsius
        static char temperatureCTemp[6];
        dtostrf(tempC, 6, 2, temperatureCTemp);
        //Set temperature Characteristic value and notify connected client
        imuTemperatureCelsiusCharacteristics.setValue(temperatureCTemp);
        imuTemperatureCelsiusCharacteristics.notify();
        Serial.print("Temperature = ");
        Serial.print(tempC);
        Serial.print(" C");

        M5.Lcd.setCursor(0, 20, 2);
        M5.Lcd.print("Temperature = ");
        M5.Lcd.print(tempC);
        M5.Lcd.println(" C");
      #else
        static char temperatureFTemp[6];
        dtostrf(tempF, 6, 2, temperatureFTemp);
        //Set temperature Characteristic value and notify connected client
        imuTemperatureFahrenheitCharacteristics.setValue(temperatureFTemp);
        imuTemperatureFahrenheitCharacteristics.notify();
        Serial.print("Temperature = ");
        Serial.print(tempF);
        Serial.print(" F");

        M5.Lcd.setCursor(0, 20, 2);
        M5.Lcd.print("Temperature = ");
        M5.Lcd.print(tempF);
        M5.Lcd.println(" F");
      #endif
      
      //Notify battery status reading from AXP192
      static char voltageBatt[6];
      dtostrf(vBatt, 6, 2, voltageBatt);
      //Set voltage Characteristic value and notify connected client
      axpVoltageCharacteristics.setValue(voltageBatt);
      axpVoltageCharacteristics.notify();   
      Serial.print(" - Battery Voltage = ");
      Serial.print(vBatt);
      Serial.println(" V");

      M5.Lcd.setCursor(0, 40, 2);
      M5.Lcd.print("Battery Votage = ");
      M5.Lcd.print(vBatt);
      M5.Lcd.println(" V");
      
      lastTime = millis();

    }
  }
  delay(500);
}

 
float readTemperature(){
  float t;
  M5.IMU.getTempData(&t);
  t = (t-32.0)*5.0/9.0;

  Serial.print("Temperature: ");
  Serial.print(t);

  return t;
}

