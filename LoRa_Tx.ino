// LoRa_9x_TX
// Example of manually implementing "reliable sends" and ACK reception
// without using RHReliableDatagram.

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

// Change frequency to match your hardware/environment
#define RF95_FREQ 422.0

// Addresses: we'll treat this as the "Client" at address 2
#define MY_ADDRESS     2
// We'll assume the "Server" we want to talk to is at address 1
#define SERVER_ADDRESS 1
// Broadcast address (optional). Packets with toID = BROADCAST_ADDRESS are accepted by all.
#define BROADCAST_ADDRESS 0xFF

// Create an instance of the driver (no ReliableDatagram)
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
// Helper: compute a simple XOR-based checksum
// ---------------------------------
uint8_t computeChecksum(const Packet& pkt) {
  // We XOR everything except the 'checksum' field itself
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
// Helper: create a Packet struct
// ---------------------------------
Packet createPacket(uint8_t toID, uint8_t fromID, const char* msg) {
  Packet pkt;
  pkt.toID      = toID;
  pkt.fromID    = fromID;

  // Truncate if msg is too long for our buffer
  uint8_t length = strlen(msg);
  if (length > (MAX_PAYLOAD_LEN - 1)) { 
    length = MAX_PAYLOAD_LEN - 1;
  }

  pkt.payloadLen = length;
  memset(pkt.payload, 0, MAX_PAYLOAD_LEN);
  strncpy(pkt.payload, msg, length);

  // Compute the checksum
  pkt.checksum = computeChecksum(pkt);

  return pkt;
}

// ---------------------------------
// Helper: validate a received packet
// Returns true if addresses match (toID == MY_ADDRESS or broadcast) and checksum is OK
// ---------------------------------
bool validatePacket(const Packet& pkt) {
  // Check if it's addressed to us or broadcast
  if (pkt.toID != MY_ADDRESS && pkt.toID != BROADCAST_ADDRESS) {
    return false;
  }

  // Check the checksum
  uint8_t expected = computeChecksum(pkt);
  if (pkt.checksum != expected) {
    return false;
  }

  return true;
}

// ---------------------------------
// Helper function to show text on OLED
// ---------------------------------
void updateDisplay(const char* message) {
  display.clearDisplay();       
  display.setCursor(0, 0);      
  display.print(message);       
  display.display();            
  delay(200);
}

// ---------------------------------
// Send with (manual) ack
// Attempts multiple times to send `message` and waits for an "ACK" reply
// Returns `true` if an "ACK" was received.
// ---------------------------------
bool sendWithAck(const Packet& packetToSend, uint8_t retries = 3, unsigned long ackTimeout = 2000) 
{
  uint8_t *rawPtr = (uint8_t*)&packetToSend; 
  uint8_t  rawSize = sizeof(packetToSend);

  for (uint8_t attempt = 0; attempt < retries; attempt++) 
  {
    Serial.print("TX > Attempt #");
    Serial.println(attempt+1);

    // Send
    rf95.send(rawPtr, rawSize);
    rf95.waitPacketSent();

    // Put receiver in RX mode to listen for ACK
    rf95.setModeRx();

    unsigned long startTime = millis();
    while (millis() - startTime < ackTimeout) 
    {
      if (rf95.available()) 
      {
        Packet ackPacket;
        uint8_t len = sizeof(ackPacket);
        uint8_t buf[sizeof(ackPacket)];

        if (rf95.recv(buf, &len)) 
        {
          // Copy buffer into ackPacket struct
          if (len == sizeof(Packet)) {
            memcpy(&ackPacket, buf, len);

            // Validate that the packet is addressed to us and has correct checksum
            if (validatePacket(ackPacket)) {
              // Check if the payload is "ACK"
              if (strncmp(ackPacket.payload, "ACK", 3) == 0) {
                Serial.println("TX > Received valid ACK!");
                return true;
              }
            }
          }
        }
      }
    }
    Serial.println("TX > No ACK this attempt...");
  }

  // If we fall through all attempts, no ACK was received
  return false;
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
    updateDisplay("Setup Failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  // Set frequency
  if (!rf95.setFrequency(RF95_FREQ)) 
  {
    Serial.println("setFrequency failed");
    updateDisplay("Setup Failed");
    while (1);
  }
  Serial.print("Set Freq to: "); 
  Serial.println(RF95_FREQ);

  // Set power (PA_BOOST pin). Valid range is 5-23 for RFM95 w/PA_BOOST
  rf95.setTxPower(13, false);

  updateDisplay("Setup Successful");
  delay(2000);
}

// ---------------------------------
// Main Loop
// ---------------------------------
int16_t packetnum = 0;  // packet counter

void loop()
{
  // Construct a message and packet
  char outgoing[40];
  sprintf(outgoing, "Hello #%d from node(%d)", packetnum++, MY_ADDRESS);

  // Create a packet for sending to the server
  Packet pkt = createPacket(SERVER_ADDRESS, MY_ADDRESS, outgoing);

  // 1) Send message with manual ack
  Serial.println("-----------------------------------");
  Serial.print("Sending to node #");
  Serial.print(SERVER_ADDRESS);
  Serial.println("...");

  updateDisplay("Sending Message");

  if (sendWithAck(pkt)) 
  {
    // We got an ACK from the server
    Serial.println("Message sent & ACK received");
    updateDisplay("Waiting for Reply");

    // 2) Check if the server also sent back data
    bool gotReply = false;
    uint8_t buf[sizeof(Packet)];
    uint8_t len = sizeof(buf);

    // Wait a bit for the server's follow-up message
    unsigned long start = millis();
    rf95.setModeRx(); // ensure we are in RX mode

    while (millis() - start < 2000) // 2 second window (adjust as needed)
    {
      if (rf95.available()) 
      {
        if (rf95.recv(buf, &len)) 
        {
          // Ensure the size matches
          if (len == sizeof(Packet)) {
            Packet replyPacket;
            memcpy(&replyPacket, buf, len);

            // Validate the packet
            if (validatePacket(replyPacket)) {
              // It's for us
              Serial.print("Got reply: ");
              Serial.println(replyPacket.payload);
              updateDisplay("Message Received");
              gotReply = true;
            }
          }
          break;
        }
      }
    }

    if (!gotReply) {
      Serial.println("No reply from server (but we got an ACK).");
    }
  } 
  else 
  {
    // We did not get an ACK after all retries
    Serial.println("Send failed (no ACK)");
    updateDisplay("No ACK");
  }

  delay(5000); // Wait before sending the next packet
}
