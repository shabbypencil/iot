// LoRa_9x_RX
// Example of manually receiving a message and sending back both an "ACK"
// and a separate "Hello from Server" message.

#include <SPI.h>
#include <RH_RF95.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------
// OLED Display Settings
// ---------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 16
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------------------------
// Radio Settings
// ---------------------------------
#define RFM95_CS  10
#define RFM95_RST 9
#define RFM95_INT 2

// Change frequency to match TX
#define RF95_FREQ 422.0

// We'll treat this as the "Server" at address 1
#define MY_ADDRESS 1
// A known client might be at address 2, or you can support more devices
#define BROADCAST_ADDRESS 0xFF

RH_RF95 rf95(RFM95_CS, RFM95_INT);

// ---------------------------------
// Packet Structure
// ---------------------------------
#define MAX_PAYLOAD_LEN 32

struct Packet {
  uint8_t toID;
  uint8_t fromID;
  uint8_t payloadLen;
  char    payload[MAX_PAYLOAD_LEN];
  uint8_t checksum;
};

// ---------------------------------
// Helper: compute XOR-based checksum
// ---------------------------------
uint8_t computeChecksum(const Packet& pkt) {
  uint8_t sum = 0;
  sum ^= pkt.toID;
  sum ^= pkt.fromID;
  sum ^= pkt.payloadLen;
  for (uint8_t i = 0; i < pkt.payloadLen; i++) {
    sum ^= pkt.payload[i];
  }
  return sum;
}

// ---------------------------------
// Helper: validate a received packet
// ---------------------------------
bool validatePacket(const Packet& pkt) {
  // Address filter
  if (pkt.toID != MY_ADDRESS && pkt.toID != BROADCAST_ADDRESS) {
    return false;
  }
  // Checksum
  uint8_t expected = computeChecksum(pkt);
  if (pkt.checksum != expected) {
    return false;
  }
  return true;
}

// ---------------------------------
// Helper: create a Packet struct
// ---------------------------------
Packet createPacket(uint8_t toID, uint8_t fromID, const char* msg) {
  Packet pkt;
  pkt.toID      = toID;
  pkt.fromID    = fromID;

  uint8_t length = strlen(msg);
  if (length > (MAX_PAYLOAD_LEN - 1)) { 
    length = MAX_PAYLOAD_LEN - 1;
  }

  pkt.payloadLen = length;
  memset(pkt.payload, 0, MAX_PAYLOAD_LEN);
  strncpy(pkt.payload, msg, length);

  pkt.checksum = computeChecksum(pkt);
  return pkt;
}

// ---------------------------------
// Simple display function
// ---------------------------------
void displayMessage(const char* msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(msg);
  display.display();
  Serial.println(msg);
  delay(500);
}

// ---------------------------------
// Setup
// ---------------------------------
void setup() 
{
  Serial.begin(9600);
  delay(100);

  // Initialize display
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    Serial.println(F("SSD1306 allocation failed"));
    while (1) { delay(1000); }
  }
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  displayMessage("Starting...");

  // Reset LoRa module
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  // Initialize the driver
  if (!rf95.init()) 
  {
    Serial.println("LoRa radio init failed");
    displayMessage("Setup Failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  // Set frequency
  if (!rf95.setFrequency(RF95_FREQ)) 
  {
    Serial.println("setFrequency failed");
    displayMessage("Freq Set Failed");
    while (1);
  }
  Serial.print("Set Freq to: "); 
  Serial.println(RF95_FREQ);

  // Set power
  rf95.setTxPower(13, false);

  displayMessage("Setup Successful");
}

// ---------------------------------
// Main Loop
// ---------------------------------
void loop()
{
  if (rf95.available()) 
  {
    // Prepare buffer for incoming data
    Packet incomingPacket;
    uint8_t len = sizeof(incomingPacket);
    uint8_t buf[sizeof(incomingPacket)];

    // Attempt to receive
    if (rf95.recv(buf, &len)) 
    {
      // Check size matches our Packet struct
      if (len == sizeof(Packet)) {
        memcpy(&incomingPacket, buf, len);

        // Now validate
        if (validatePacket(incomingPacket)) 
        {
          // We only process if it's valid and for us/broadcast
          Serial.println("-----------------------------------");
          Serial.print("Received from node(");
          Serial.print(incomingPacket.fromID);
          Serial.print("): ");
          Serial.println(incomingPacket.payload);
          Serial.print("RSSI: ");
          Serial.println(rf95.lastRssi(), DEC);

          displayMessage("Message Received");

          // 1) Immediately send back an "ACK" (addressed to the sender)
          Packet ackPkt = createPacket(incomingPacket.fromID, MY_ADDRESS, "ACK");
          rf95.send((uint8_t*)&ackPkt, sizeof(ackPkt));
          rf95.waitPacketSent();
          Serial.println("Sent ACK to sender");

          // 2) (Optionally) send a separate message back
          //    e.g., "Hello from Server (#1)"
          const char* replyStr = "Hello from node(1)";
          Packet replyPkt = createPacket(incomingPacket.fromID, MY_ADDRESS, replyStr);
          Serial.print("Sending reply: ");
          Serial.println(replyStr);

          rf95.send((uint8_t*)&replyPkt, sizeof(replyPkt));
          rf95.waitPacketSent();
          Serial.println("Reply message sent");
        }
        else {
          Serial.println("Received a packet, but invalid address/checksum.");
        }
      }
      else {
        Serial.println("Receive failed or size mismatch.");
      }
    }
    else 
    {
      Serial.println("Receive failed (CRC?)");
    }
  }
  else 
  {
    // No message available
    displayMessage("Waiting for MSG");
    delay(500);
  }
}
