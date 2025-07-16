#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
#define USB1 8
#define USB2 9
#define USB3 10
#define HV1 4
#define HV2 3
#define HV3 5
#define IP1 2
#define IP2 1

typedef enum {
    USB1_PIN = 0,
    USB2_PIN = 1,
    USB3_PIN = 2,
    HV1_PIN = 3,
    HV2_PIN = 4,
    HV3_PIN = 5
} device_index;

int npins[] = {8,9,10,4,3,5};
bool masks[] = {false,false,false,true,true,true};
bool states[] = {false,false,false,false,false,false};

void setState(device_index channel,bool state)
{
  if(masks[channel])
  {
    digitalWrite(npins[channel],state);
    states[channel] = state;
  }
  else{
    digitalWrite(npins[channel],!state);
    states[channel] = state;
  }
}

bool getState(device_index channel)
{
  return states[channel];
}

unsigned long ms100=0,s1=0,ms500=0;

unsigned long seconds = 0;

unsigned long t_pump=0,t_light=0,t_air=0;
bool pump=false,light=false,air=false;

unsigned long pumpOn = 45, pumpOff = 180;
unsigned long lightOn = 43200, lightOff=43200;
unsigned long airOn=60, airOff=500;

bool running = false;
bool started = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// Authentication token - change this to your desired token
const char* AUTH_TOKEN = "MySecretToken123";

String mac = "";

// Maximum number of peers to store
#define MAX_PEERS 10

// Structure to store authenticated peers
typedef struct {
    uint8_t mac_addr[6];
    bool active;
} authenticated_peer;

// Array to store authenticated peers
authenticated_peer peerList[MAX_PEERS];
int peerCount = 0;

// Structure for incoming messages
typedef struct {
    char token[32];
    char command[16];
} incoming_message;


// Structure for broadcast messages
typedef struct {
    bool usb1;
    bool usb2;
    bool usb3;
    bool hv1;
    bool hv2;
    bool hv3;
    bool isRunning;
    unsigned long pumpOn,pumpOff,lightOn,lightOff,airOn,airOff;
    char sender_mac[18];
    unsigned long timestamp;
} broadcast_message;

bool addPeerToList(const uint8_t *mac_addr) {
    // Check if peer already exists
    for (int i = 0; i < peerCount; i++) {
        if (memcmp(peerList[i].mac_addr, mac_addr, 6) == 0) {
            peerList[i].active = true;
            Serial.println("Peer already in list - marked as active");
            return true;
        }
    }
    
    // Add new peer if there's space
    if (peerCount < MAX_PEERS) {
        memcpy(peerList[peerCount].mac_addr, mac_addr, 6);
        peerList[peerCount].active = true;
        peerCount++;
        
        Serial.printf("Added peer to list. Total peers: %d\n", peerCount);
        return true;
    } else {
        Serial.println("Peer list full - cannot add more peers");
        return false;
    }
}


// Function to broadcast temperature and humidity to all peers
void broadcastData() {
    if (peerCount == 0) {
        Serial.println("No authenticated peers to broadcast to");
        return;
    }
    
    broadcast_message broadcastMsg;
    broadcastMsg.usb1 = getState(USB1_PIN);
    broadcastMsg.usb2 = getState(USB2_PIN);
    broadcastMsg.usb3 = getState(USB3_PIN);
    broadcastMsg.hv1 = getState(HV1_PIN);
    broadcastMsg.hv2 = getState(HV2_PIN);
    broadcastMsg.hv3 = getState(HV3_PIN);
    broadcastMsg.pumpOn = pumpOn;
    broadcastMsg.pumpOff = pumpOff;
    broadcastMsg.lightOn = lightOn;
    broadcastMsg.lightOff = lightOff;
    broadcastMsg.airOn = airOn;
    broadcastMsg.airOff = airOff;
    broadcastMsg.isRunning = running;

    broadcastMsg.timestamp = millis();
    strcpy(broadcastMsg.sender_mac, mac.c_str());

    
    int successCount = 0;
    
    for (int i = 0; i < peerCount; i++) {
        if (peerList[i].active) {
            esp_err_t result = esp_now_send(peerList[i].mac_addr, 
                                          (uint8_t *) &broadcastMsg, 
                                          sizeof(broadcastMsg));
            
            if (result == ESP_OK) {
                successCount++;
                Serial.printf("  Broadcast sent to peer %d\n", i + 1);
            } else {
                Serial.printf("  Failed to send to peer %d (error: %d)\n", i + 1, result);
            }
        }
    }
    
}

// Callback function for received ESP-NOW data (v2.0.3 compatible)
// Callback function for received ESP-NOW data (v2.0.3 compatible)
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    // Get the MAC address from recv_info

    incoming_message receivedMsg;

    // Check if received data size matches expected structure
    if (len != sizeof(incoming_message)) {
        Serial.printf("ERROR: Data size mismatch! Expected %d, got %d\n",
                      sizeof(incoming_message), len);
        return;
    }

    // Copy incoming data to structure
    memcpy(&receivedMsg, incomingData, sizeof(receivedMsg));

    // Ensure null termination for strings
    receivedMsg.token[31] = '\0';
    receivedMsg.command[15] = '\0';

    Serial.printf("Token: %s\n", receivedMsg.token);
    Serial.printf("Command: %s\n", receivedMsg.command);

    // Check if token matches
    if (strcmp(receivedMsg.token, AUTH_TOKEN) == 0) {
        Serial.println("Token validated successfully!");
        bool peerExists = esp_now_is_peer_exist(mac_addr);
        if (!peerExists) {
            esp_now_peer_info_t peerInfo;
            memset(&peerInfo, 0, sizeof(peerInfo));
            memcpy(peerInfo.peer_addr, mac_addr, 6);
            peerInfo.channel = 0;  // Use current channel
            peerInfo.encrypt = false;
            esp_err_t addStatus = esp_now_add_peer(&peerInfo);
            addPeerToList(mac_addr);
            if (addStatus == ESP_OK) {
                Serial.println("Peer registered in ESP-NOW successfully");
            } else {
                Serial.printf("Failed to register peer in ESP-NOW: %d\n", addStatus);
                return;
            }
        } else {
            Serial.println("Peer already registered in ESP-NOW");
        }

        if (receivedMsg.command[0] == 0xAA && receivedMsg.command[1] == 0xCC) {
            if (receivedMsg.command[2] == 0xDA) {
                if (!running) {
                    if ((receivedMsg.command[3] >= 0x00) && (receivedMsg.command[3] <= 0x05)) {
                      bool tst = false;
                      if (receivedMsg.command[4] == 0xAF)
                      {
                        tst = true;
                      }

                      if (receivedMsg.command[3] == 0x00)
                      {
                        setState(USB1_PIN, tst);
                      }
                      else if (receivedMsg.command[3] == 0x01)
                      {
                        setState(USB2_PIN, tst);
                      }
                      else if (receivedMsg.command[3] == 0x02)
                      {
                        setState(USB3_PIN, tst);
                      }
                      else if (receivedMsg.command[3] == 0x03)
                      {
                        setState(HV1_PIN, tst);
                      }
                      else if (receivedMsg.command[3] == 0x04)
                      {
                        setState(HV2_PIN, tst);
                      }
                      else if (receivedMsg.command[3] == 0x05)
                      {
                        setState(HV3_PIN, tst);
                      }

                    }
                }
            } else if (receivedMsg.command[2] == 0xDB) {
                running = true;
            } else if (receivedMsg.command[2] == 0xDC) {
                running = false; 
            } else if (receivedMsg.command[2] == 0xCC) {
                setState(USB3_PIN, false);
                setState(USB2_PIN, false);
                setState(HV1_PIN, false);
                started = false;
                running = false;
                pump = false;
                air = false;
                light = false;
                t_pump = 0;
                t_light = 0;
                t_air = 0;
            }
        }
    } else {
        Serial.println("Invalid token - ignoring request");
        // Don't send any response for invalid tokens
    }
}
        











// Callback function for sent data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}















void callback_100ms()
{
  if(!digitalRead(IP1)){running = true;}
  if(!digitalRead(IP2)){running = false;}
  
}

void callback_500ms()
{
  updateDisplay();
}

void callback_1s()
{
  seconds++;
  if(running)
  {
    if(!started)
    {
      Serial.println("Started");
      seconds = 0;
      setState(USB3_PIN,true);
      //digitalWrite(USB3,LOW);
      Serial.println("USB3 ON");
      delay(2000);
      setState(USB2_PIN,true);
      //digitalWrite(USB2,LOW);
      Serial.println("USB2 ON");
      delay(2000);
      setState(HV1_PIN,true);
      //digitalWrite(HV1,HIGH);
      Serial.println("HV1 ON");
      started = true;
      pump = true;
      air = true;
      light = true;
      t_pump = seconds + pumpOn;
      t_light = seconds + lightOn;
      t_air = seconds + airOn;
    }

    if(seconds >= t_pump)
    {
      if(pump)
      {
        setState(HV1_PIN,false);
        Serial.println("HV1 OFF");
        pump = false;
        t_pump = seconds + pumpOff;
      }
      else
      {
        setState(HV1_PIN,true);
        Serial.println("HV1 ON");
        pump = true;
        t_pump = seconds + pumpOn;
      }
    }

     if(seconds >= t_light)
    {
      if(light)
      {
        setState(USB3_PIN,false);
        Serial.println("USB3 OFF");
        light = false;
        t_light = seconds + lightOff;
      }
      else
      {
        setState(USB3_PIN,true);
        Serial.println("USB3 ON");
        light = true;
        t_light = seconds + lightOn;
      }
    }

    if(seconds >= t_air)
    {
      if(air)
      {
        setState(USB2_PIN,false);
        Serial.println("USB2 OFF");
        air = false;
        t_air = seconds + airOff;
      }
      else
      {
        setState(USB2_PIN,true);
        Serial.println("USB2 ON");
        air = true;
        t_air = seconds + airOn;
      }
    }

    
  }
  else
  {
    
    if(started == true)
    {
          Serial.println("STOPPED");
          setState(USB3_PIN, false);
          setState(USB2_PIN, false);
          setState(HV1_PIN, false);
          started = false;
          pump = false;
          air = false;
          light = false;
          t_pump = 0;
          t_light=0;
          t_air=0;
    }

  }

}


void updateDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);
  if(running)
  {
    display.println("Running");
  }
  else{
    display.println("Stopped");
  }
  display.display();
}





void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(USB1,OUTPUT_OPEN_DRAIN);
  pinMode(USB2,OUTPUT_OPEN_DRAIN);
  pinMode(USB3,OUTPUT_OPEN_DRAIN);
  pinMode(HV1,OUTPUT);
  pinMode(HV2,OUTPUT);
  pinMode(HV3,OUTPUT);
  pinMode(IP1,INPUT);
  pinMode(IP2,INPUT);
  digitalWrite(USB1,HIGH);
  digitalWrite(USB2,HIGH);
  digitalWrite(USB3,HIGH);
  digitalWrite(HV1,LOW);
  digitalWrite(HV2,LOW);
  digitalWrite(HV3,LOW);
  Wire.setPins(6,7);
   


 if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED init failed");
    while(true)
    {

    } // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();

  WiFi.mode(WIFI_STA);
    
    // Print MAC address
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    // Register callback functions
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
    
    Serial.println("ESP-NOW initialized successfully");
    Serial.println("Waiting for incoming messages...");
    Serial.printf("Valid token: %s\n", AUTH_TOKEN);
    Serial.println("---");

    mac = WiFi.macAddress();
    
    
}



void loop() {
  unsigned long current = millis();
  if(current >= ms100)
  {
    callback_100ms();
    ms100 = 100 + current;
  }

  if(current >= ms500)
  {
    callback_500ms();
    ms500 = 500 + current;
  }

  if(current >= s1)
  {
    callback_1s();
    Serial.println(mac);
    broadcastData();
    s1 = 1000 + current;
  }
}
