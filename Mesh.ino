#include <painlessMesh.h>
#include <M5StickCPlus.h>


#define MESH_PREFIX     "M5MeshMesh"
#define MESH_PASSWORD   "password123"
#define MESH_PORT       5555

Scheduler userScheduler;
painlessMesh mesh;

// Define message priority levels
enum MessagePriority {
    PRIORITY_HIGH = 0,
    PRIORITY_MEDIUM = 1,
    PRIORITY_LOW = 2
};

struct Message {
  String msg;
  String from;
  enum MessagePriority priority;
};

// Separate queues for each priority
SimpleList<Message> highPriorityQueue;
SimpleList<Message> mediumPriorityQueue;
SimpleList<Message> lowPriorityQueue;

void processReceivedMessages();

// Received message handler
void receivedCallback(uint32_t from, String & msg) {
    int separatorIndex = msg.indexOf('|');
    if (separatorIndex == -1) {
        Serial.println("Invalid message format");
        return;
    }

    String priorityStr = msg.substring(0, separatorIndex);
    String actualMsg = msg.substring(separatorIndex + 1);

    MessagePriority priority;
    if (priorityStr == "0") priority = PRIORITY_HIGH;
    else if (priorityStr == "1") priority = PRIORITY_MEDIUM;
    else priority = PRIORITY_LOW;

    Message newMessage;
    // strcpy(newMessage.msg, actualMsg.c_str(), sizeof(newMessage.msg)-1);
    // newMessage.msg[sizeof(newMessage.msg) - 1] = '\0';
    newMessage.msg = actualMsg;
    newMessage.from = String(from);
    newMessage.priority = priority;

    // Store message in the appropriate queue
    switch (priority) {
        case PRIORITY_HIGH:
            highPriorityQueue.push_back(newMessage);
            break;
        case PRIORITY_MEDIUM:
            mediumPriorityQueue.push_back(newMessage);
            break;
        case PRIORITY_LOW:
            lowPriorityQueue.push_back(newMessage);
            break;
    }
    Serial.println("High Priority Queue: " + String(highPriorityQueue.size()) + " | Medium Priority Queue: " + String(mediumPriorityQueue.size()) + " | Low Priority Queue: " + String(lowPriorityQueue.size()));
    
    // Display the highest priority message available
    // processReceivedMessages();
}

// Function to process received messages
void processReceivedMessages() {
    Message message;

    if (!highPriorityQueue.empty()) {
        message = highPriorityQueue.front();
        highPriorityQueue.pop_front();
    } else if (!mediumPriorityQueue.empty()) {
        message = mediumPriorityQueue.front();
        mediumPriorityQueue.pop_front();
    } else if (!lowPriorityQueue.empty()) {
        message = lowPriorityQueue.front();
        lowPriorityQueue.pop_front();
    }

    if (!message.msg.isEmpty()) {
        M5.Lcd.fillRect(0, 10, 160, 70, BLACK);
        M5.Lcd.setCursor(10, 20);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setTextSize(2);
        M5.Lcd.print("Msg: ");
        M5.Lcd.println(String(message.msg));

        Serial.println("Received from: " + String(message.from) +": "+ message.msg +" priority :"+ String(message.priority));
    }
}

Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, []() {
    MessagePriority priority = static_cast<MessagePriority>(random(0, 3));
    String msg = String(priority) + "|" + "Hello from HAO YI 2303340" ;
    
    Serial.println("Sending message: " + String(msg.c_str()) +" with priority " +String(priority));
    mesh.sendBroadcast(msg);

    taskSendMessage.setInterval(random(TASK_SECOND * 1, TASK_SECOND * 5));
});

void setup() {
    M5.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("M5StickC Mesh");

    Serial.begin(115200);

    mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.onReceive(&receivedCallback);

    userScheduler.addTask(taskSendMessage);
    taskSendMessage.enable();
}

unsigned long lastProcessTime = 0;  // Variable to store the last time processReceivedMessages was run
const unsigned long interval = 5000; // 5 seconds in milliseconds

void loop() {
    mesh.update();

      if (millis() - lastProcessTime >= interval) {
        Serial.println("Processing From Priority Queue..");
        processReceivedMessages();  // Run the message processing
        lastProcessTime = millis();  // Update the last time we ran the function
    }
}