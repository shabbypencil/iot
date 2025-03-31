/*
*******************************************************************************
* Copyright (c) 2021 by M5Stack
*                  Equipped with M5StickC-Plus sample source code
*                             M5StickC-Plus
* Visit for more information: https://docs.m5stack.com/en/core/m5stickc_plus
*
* Describe: MQTT
* Date: 2021/11/5
*******************************************************************************
*/
#include "M5StickCPlus.h"
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

// Configure the name and password of the connected wifi and your MQTT Serve
// host.  
const char* ssid        = "SimPhone";
const char* password    = "a1234567";
const char* mqtt_server = "172.20.10.3";

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE]; 
int value = 0;

void setupWifi();
void callback(char* topic, byte* payload, unsigned int length);
void reConnect();

void setup() {
    M5.begin();
    M5.Lcd.setRotation(3);
    setupWifi();
    pinMode(10, OUTPUT);
    client.setServer(mqtt_server, 1883);  // Sets the server details.  
    client.setCallback(callback);  		  // Sets the message callback function.  
}

void loop() {
    if (!client.connected()) {
        reConnect();
    }
    client.loop();  // This function is called periodically to allow clients to
                    // process incoming messages and maintain connections to the
                    // server.
    M5.update();
    if (M5.BtnA.isPressed()){
      Serial.println("Button Pressed");
      
      snprintf(msg, MSG_BUFFER_SIZE, "toggle");
      client.publish("LED_A", msg);
    }
    delay(200);
}

void setupWifi() {
    delay(10);
    M5.Lcd.printf("Connecting to %s", ssid);
    WiFi.mode(WIFI_STA);  		 // Set the mode to WiFi station mode.  
    WiFi.begin(ssid, password);  // Start Wifi connection.  

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }
    M5.Lcd.printf("\nSuccess\n");
}

void callback(char* topic, byte* payload, unsigned int length) {
    M5.Lcd.print("Message arrived [");
    M5.Lcd.print(topic);
    M5.Lcd.print("] ");
    for (int i = 0; i < length; i++) {
        M5.Lcd.print((char)payload[i]);
    }
    M5.Lcd.println();
    static bool isOn = false;
    if (!isOn) {
      digitalWrite(10, LOW);  // Turn on the LED
      Serial.println("LED turned ON");
    } else {
      digitalWrite(10, HIGH);   // Turn off the LED
      Serial.println("LED turned OFF");
    }
    isOn = !isOn;
}


void reConnect() {
    while (!client.connected()) {
        M5.Lcd.print("Attempting MQTT connection...");
        // Create a random client ID.  
        String clientId = "LED_B-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.  
        if (client.connect(clientId.c_str())) {
            M5.Lcd.printf("\nSuccess\n");
            // Once connected, publish an announcement to the topic.
            
            client.publish("csc2006", "Hello CSC2006");
            // ... and resubscribe.  
            client.subscribe("LED_B");
        } else {
            M5.Lcd.print("failed, rc=");
            M5.Lcd.print(client.state());
            M5.Lcd.println("try again in 5 seconds");
            delay(5000);
        }
    }
}