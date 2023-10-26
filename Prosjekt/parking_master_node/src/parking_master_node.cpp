/**
 * @file masterNode.cpp
 *
 * @brief This program is the master node in the peer-to-peer network.
 *
 * @details The master node works like a router, by passing messages from node red along to the receiving nodes.
 * It is also responsible for receiving data from the other nodes, and publishing it to the MQTT broker.
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

//==== FUNCTIONS ====
// ESPNOW
void setupEspNow();                                                                                       // Sets up esp now
void connectToEspNowPeer(esp_now_peer_info &peerInfo);                                                    // connects too peer
void onDataTransmitted(const uint8_t *receiverMacAdress, esp_now_send_status_t transmissionStatus);       // callback on data transmitted
void onDataReceived(const uint8_t *senderMacAdress, const uint8_t *receivedData, int receivedDataLength); // callback on data received
void printReceivedJson(DynamicJsonDocument &jsonDoc);                                                     // prints received json doc to serial monitor
void printEspErrorCode(String message, esp_err_t errorCode);                                              // prints error message when transmission fails
void sendData(DynamicJsonDocument jsonDoc, esp_now_peer_info_t &peerInfo);

// MQTT
void setupWifi();                                               // Sets up wifi
void reconnect();                                               // Reconnects to mqtt broker
void callback(char *topic, byte *message, unsigned int length); // Callback function for mqtt
void publish(DynamicJsonDocument jsonDoc);                      // Publishes data to mqtt broker
void printReceivedNodeRedJson(DynamicJsonDocument &jsonDoc);    // Prints received json doc to serial monitor

//=========================================================================
//
//                                ESPNOW
//
//=========================================================================
#pragma region
// This section handles the to-way communication between the nodes.
// It uses the esp-now mesh/peer-to-peer network protocol, and json to send and recieve data.

//====== NODES ======

// Declares an instane of esp_now_peer_info_t
// that holds info about the node we want to communicate with
// in the peer-to-peer network.

esp_now_peer_info_t parking_a_node_info = {
    .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .channel = 0,
    .encrypt = false,
};

esp_now_peer_info_t parking_b_node_info = {
    .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .channel = 0,
    .encrypt = false,
};

esp_now_peer_info_t parking_c_node_info = {
    .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .channel = 0,
    .encrypt = false,
};


/**
 * @brief Configures esp now.
 *
 * @details Sets the device as a WiFi station and connects to the master node.
 * Registers callback functions for sending and receiving data.
 */
void setupEspNow()
{
  // Set device as a WiFi station
  WiFi.mode(WIFI_STA);

  // init esp-now
  esp_err_t espNowInitStatus = esp_now_init();
  if (espNowInitStatus != ESP_OK)
  {
    // If init fails, print a message and restart the esp32
    printEspErrorCode("Error initializing ESP-NOW", espNowInitStatus);
    ESP.restart();
  }

  // Connects to nodes
  connectToEspNowPeer(parking_a_node_info);
 

  // register callback function for successfully transmitting data
  esp_now_register_send_cb(onDataTransmitted);

  // register callback function for successfully receieveing data
  esp_now_register_recv_cb(onDataReceived);
}

/**
 * @brief Connects to a node in the peer-to-peer network.
 *
 * @param peerInfo The peer info of the node to connect to.
 */
void connectToEspNowPeer(esp_now_peer_info &peerInfo)
{
  esp_err_t peerConnectionStatus = esp_now_add_peer(&peerInfo);
  if (peerConnectionStatus != ESP_OK)
  {
    printEspErrorCode("Failed to add peer", peerConnectionStatus);
    ESP.restart();
  }
}

/**
 * @brief Callback funtion for when data is transmitted.
 *
 * @param receiverMacAdress The mac adress of the node that receives the data.
 * @param transmissionStatus The status of the transmission.
 */
void onDataTransmitted(const uint8_t *receiverMacAdress, esp_now_send_status_t transmissionStatus)
{
  if (transmissionStatus != ESP_NOW_SEND_SUCCESS)
  {
    Serial.print("Data transmission failed!");
    return;
  }
  Serial.println("Data transmission was succesful!");
}

/**
 * @brief Callback funtion for when data is received.
 *
 * @param senderMacAdress The mac adress of the node that sent the data.
 * @param receivedData The received data.
 * @param receivedDataLength The length of the received data.
 */
void onDataReceived(const uint8_t *senderMacAdress, const uint8_t *receivedData, int receivedDataLength)
{
  DynamicJsonDocument receivedJson(500);

  // Read the received data and store it as json in the receiveJson variable
  DeserializationError error = deserializeJson(receivedJson, receivedData, receivedDataLength);

  // If there was an error converting the recieved data to json, print the error message
  // and exit the function
  if (error != DeserializationError::Ok)
  {
    Serial.print(F("deserializeJson() failed with error message: "));
    Serial.println(error.c_str());
    return;
  }
  printReceivedJson(receivedJson);
  publish(receivedJson);
}

/**
 * @brief Prints the received json document to the serial monitor.
 *
 * @param jsonDoc The json document to print.
 */
void printReceivedJson(DynamicJsonDocument &jsonDoc)
{
  Serial.println("");
  Serial.println("NEW MESSAGE");

  // Converts the jsonDoc variable to text and sends it through
  // the serial port using Serial.print() behind the scenes
  serializeJsonPretty(jsonDoc, Serial);
  Serial.println("");
}

/**
 * @brief Prints an error message and the error code to the serial monitor.
 *
 * @param message The error message to print.
 * @param errorCode The error code to print.
 */
void printEspErrorCode(String message, esp_err_t errorCode)
{
  Serial.println(message);
  Serial.print("Error Code:");
  Serial.println(errorCode, HEX);
  Serial.println("The meaning of this error code can be found at:");
  Serial.println("https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/error-codes.html");
}

/**
 * @brief Sends data to a node in the peer-to-peer network.
 *
 * @details Serializes the json document and sends it to the node.
 *
 * @param jsonDoc The json document to send.
 * @param peerInfo The peer info of the node to send the data to.
 */
void sendData(DynamicJsonDocument jsonDoc, esp_now_peer_info_t &peerInfo)
{
  // Create string from jsonDoc
  // and store it in the payload variable
  String payload;
  int payloadSize = serializeJson(jsonDoc, payload);

  uint8_t *payloadBytes = (uint8_t *)payload.c_str();

  esp_err_t status = esp_now_send(peerInfo.peer_addr, payloadBytes, payloadSize);

  if (status != ESP_OK)
  {
    printEspErrorCode("Unable to start transmission!", status);
    return;
  }
}

#pragma endregion

//=========================================================================
//
//                                MQTT
//
//=========================================================================
#pragma region
// network
const char *ssid = "Borgiir";          // wifi name
const char *password = "Vildemor1999"; // wifi password
// MQTT broker adress
const char *mqtt_server = "10.0.0.27"; // mqtt broker adress

WiFiClient espClient;           // wifi client
PubSubClient client(espClient); // mqtt client

/**
 * @brief Connects to wifi.
 */
void setupWifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password); // connects to wifi

  while (WiFi.status() != WL_CONNECTED) // waits for connection
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WIFI connected");
  Serial.print("IP adress: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Connects to the mqtt broker and subscribes to topics.
 */
void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...: ");

    if (client.connect("ESP32Client"))
    {
      Serial.println("CONNECTED");

      //==== TOPIC SUBSCRIPTIONS ===




      // NB! FIX THIS



      client.subscribe("nodered/command");
      client.subscribe("nodered/request");
      client.subscribe("nodered/bank");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/**
 * @brief Callback function for when a message is received from the mqtt broker.
 *
 * @param topic The topic the message was received on.
 * @param message The message received.
 * @param length The length of the message.
 *
 * @details The function converts the message from Node-RED to a json document and prints it to the serial monitor.
 * It then gets the receiver from the json document and passes the message along to the receiver.
 */
void callback(char *topic, byte *message, unsigned int length)
{
  Serial.println("");
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.println("");
  Serial.println("MESSAGE: ");

  // Convert message to JSON
  DynamicJsonDocument receivedJson(500);
  DeserializationError error = deserializeJson(receivedJson, message, length);

  if (error != DeserializationError::Ok)
  {
    Serial.print(F("deserializeJson() failed with error message: "));
    Serial.println(error.c_str());
    return;
  }

  printReceivedNodeRedJson(receivedJson);

  // get receiver
  String receiver = received_json["receiver"].as<String>();
  Serial.println("");
  Serial.print("Receiver: ");
  Serial.println(receiver);
  Serial.println("");

  if (receiver == "parking_a_node"){
    Serial.println("Sending to parking node A...");
    sendData(received_json, parking_a_node_info);
  }
  else if(receiver == "parking_b_node"){
    Serial.println("Sending to parking node B...");
    sendData(received_json, parking_b_node_info);
  }
  else if(receiver == "parking_c_node"){
    Serial.println("Sending to parking node C...");
    sendData(receive_json, parking_c_node_info);
  }
  else{
    Serial.println("Receiver not recognized");
  }
}

// publishes JSON document to its topic
/**
 * @brief Publishes a json document to the mqtt broker.
 *
 * @param jsonDoc The json document to publish.
 *
 * @details The function serializes the json document and publishes it to the topic specified in the json document.
 */
void publish(DynamicJsonDocument jsonDoc)
{
  String topic = jsonDoc["topic"];
  String payload;
  int payloadSize = serializeJson(jsonDoc, payload);

  uint8_t *payloadBytes = (uint8_t *)payload.c_str();

  client.publish(topic.c_str(), payloadBytes, payloadSize);
}

/**
 * @brief Prints a json document received from NODE RED to the serial monitor.
 *
 * @param jsonDoc The json document to print.
 */
void printReceivedNodeRedJson(DynamicJsonDocument &jsonDoc)
{
  Serial.println("");
  Serial.println("NEW MESSAGE FROM NODERED");

  // Converts the jsonDoc variable to text and sends it through
  // the serial port using Serial.print() behind the scenes
  serializeJsonPretty(jsonDoc, Serial);
  Serial.println("");
}

#pragma endregion

//=========================================================================
//
//                              SETUP AND LOOP
//
//=========================================================================
/**
 * @brief Setup function. Runs once on startup.
 *
 * @details Starts serial monitor, sets up espnow and wifi.
 */
void setup()
{

  Serial.begin(115200);
  Serial.println("Hi! Im the Parking Master Node :)");

  // ESPNOW
  setupEspNow();

  // MQTT
  setupWifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

/**
 * @brief Loop function. Runs continuously.
 */
void loop()
{
  // connects to mqtt
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}