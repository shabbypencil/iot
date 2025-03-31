// https://docs.m5stack.com/en/quick_start/m5stickc_plus/arduino
#include <WiFi.h>
#include <WebServer.h>
#include <M5StickCPlus.h>

/* Put your SSID & Password */
const char* ssid = "amos";
const char* password =  "thankyouamos";

// read new set of sensors value on button push
float pitch, roll, yaw, temperature;

WebServer server(80);

void setup() {
  Serial.begin(115200);

  // put your setup code here, to run once:
  M5.begin();

  int x = M5.IMU.Init(); //return 0 is ok, return -1 is unknown
  if(x!=0)
    Serial.println("IMU initialisation fail!");  

  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.printf("RESTful API", 0);

  WiFi.begin(ssid, password);

  // Setting the hostname
  WiFi.setHostname("group01-stick");

  Serial.print("Start WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("Connecting ..");
  }
  M5.Lcd.setCursor(0, 20, 2);
  M5.Lcd.print("IP: ");
  M5.Lcd.println(WiFi.localIP());

  pinMode(M5_BUTTON_HOME, INPUT);
  pinMode(M5_LED, OUTPUT);
  delay(100);

  server.on("/", handle_JsonResponse);
  server.on("/", handle_Root);
  server.on("/gyro/", handle_Gyro);
  server.on("/accel/", handle_Accel);
  server.on("/temp/", handle_Temp);
  server.on("/led/0", [](){handle_LED(0);});
  server.on("/led/1", [](){handle_LED(1);});
  server.on("/buzzer/0", [](){handle_Buzzer(0);});
  server.on("/buzzer/1", [](){handle_Buzzer(1);});
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Connected to the WiFi network. IP: ");
  Serial.println(WiFi.localIP());
}

void handle_Root() {
  String response = "{ \"status\": \"Device is running\" }";
  server.send(200, "application/json", response);
}

void handle_Gyro() {
  readGyro;
  String response = "{ \"gyro\": { \"pitch\": " + String(pitch, 6) + ", \"roll\": " + String(roll, 6) + ", \"yaw\": " + String(yaw, 6) + " } }";
  server.send(200, "application/json", response);
}

void handle_Accel() {
  float ax, ay, az;
  M5.IMU.getAccelData(&ax, &ay, &az);
  String response = "{ \"accel\": { \"ax\": " + String(ax, 6) + ", \"ay\": " + String(ay, 6) + ", \"az\": " + String(az, 6) + " } }";
  server.send(200, "application/json", response);
}

void handle_Temp() {
  temperature = readTemperature();
  String response = "{ \"temperature\": " + String(temperature, 2) + " }";
  server.send(200, "application/json", response);
}

void handle_LED(int state) {
  digitalWrite(M5_LED, state);
  String response = "{ \"led\": " + String(state) + " }";
  server.send(200, "application/json", response);
}

void handle_Buzzer(int state) {
  if (state == 1) {
    M5.Beep.mute();
    M5.Beep.tone(1000, 500);
  } else {
    M5.Beep.mute();
  }
  String response = "{ \"buzzer\": " + String(state) + " }";
  server.send(200, "application/json", response);
}
 
void handle_JsonResponse(){
  String response;
  response += "{ \"imu\": { \"pitch\": ";
  response += String(pitch, 6);
  response += ", \"roll\": ";
  response += String(roll, 6);
  response += ", \"yaw\": ";
  response += String(yaw, 6);
  response += ", \"temperature\": ";
  response += String(temperature, 6);
  response += " } }";
  Serial.println(response);
  digitalWrite(M5_LED, 1);

  server.send(200, "application/json", response);
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}


bool readGyro(){
  M5.IMU.getAhrsData(&pitch, &roll, &yaw);
  return true;
}
 

float readTemperature(){
  float t;
  M5.IMU.getTempData(&t);
  t = (t-32.0)*5.0/9.0;
  return t;
}

uint8_t setup_flag = 1;

bool readSensors() {
  bool status = readGyro();
  if (status==true) {
    M5.Lcd.setCursor(0, 40, 2);
    M5.Lcd.printf("                                        ");
  }
  Serial.println("Gyro:");
  Serial.print("Pitch[X]: ");
  Serial.println(pitch);
  Serial.print("Roll[Y]: ");
  Serial.println(roll);
  Serial.print("Yaw[Z]: ");
  Serial.println(yaw);

  temperature = readTemperature();
  M5.Lcd.setCursor(0, 60, 2);
  M5.Lcd.printf("Temperature = %2.1f", temperature);
  Serial.print("Temperature: ");
  Serial.println(temperature);

  return status;
}
 
void loop() {
  server.handleClient();

  if (setup_flag == 1) {
    M5.Lcd.setCursor(0, 40, 2);
    M5.Lcd.printf("X = %3.2f, Y = %3.2f, Z = %3.2f", pitch, roll, yaw);
    M5.Lcd.setCursor(0, 60, 2);
    M5.Lcd.printf("Temperature = %2.1f", temperature);
  }

  if(!setup_flag){
    setup_flag = 1;
    bool status = readSensors();
    if (status)
      Serial.print("\n\rRead Sensors success...\n");
    else
      Serial.print("\n\rRead Sensors failed...\n");
  }

  if(digitalRead(M5_BUTTON_HOME) == LOW){
    setup_flag = 0;
    while(digitalRead(M5_BUTTON_HOME) == LOW);
  }
}