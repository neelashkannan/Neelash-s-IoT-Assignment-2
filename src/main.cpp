#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Firebase_ESP_Client.h>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"
// Wi-Fi credentials
const char* ssid = "NEELASH-LOQ 5535";
const char* password = "12345678";

// MQTT broker details
const char* mqtt_server = "10.37.98.216";
const char* ledTopic = "assignment2/led";             // Topic for LED ON/OFF control
const char* ledColorTopic = "assignment2/ledcolor";   // New topic for RGB LED color control
const char* temperatureTopic = "assignment2/temperature"; // Topic for temperature data
const char* humidityTopic = "assignment2/humidity";       // Topic for humidity data
const char* statusTopic = "assignment2/status";           // Topic for status messages
const char* ledbrightness = "assignment2/brightness";
const char* assignmentclock = "assignment2/time";
// LED and DHT11 settings
const int redPin = 23;    
const int greenPin = 22;  
const int bluePin = 21;
const int redChannel = 2;
const int greenChannel = 1;
const int blueChannel = 0;
const int ledPin = 2;               // Built-in LED pin for ON/OFF control
const int dhtPin = 4;               // DHT11 data pin

#define API_KEY "AIzaSyD9hOm88DWHxQE6rw197X2Gdnr9vIKlf7U"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://assignment2-3f9c2-default-rtdb.europe-west1.firebasedatabase.app/" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

#define DHTTYPE DHT11               // DHT11 sensor type
DHT dht(dhtPin, DHTTYPE);
int humidityIndex = 1;
int temperatureIndex = 1;

const int pwmFreq = 5000;     
const int pwmResolution = 8; 
String currenttime;
String ledControl;                  // Variable to store LED ON/OFF state
String ledColor;                    // Variable to store RGB LED color state
int brightness;
int temp;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;
// Callback function to handle incoming MQTT messages
void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
    
  }

  // Check for LED ON/OFF control messages
  if (String(topic) == ledTopic) {
    ledControl = messageTemp;  // Store the received message for LED ON/OFF control
  }
  if (String(topic) == assignmentclock) {
    int colonIndex = messageTemp.indexOf(":");
    currenttime =messageTemp.substring(0, colonIndex) + "h" + messageTemp.substring(colonIndex + 1, messageTemp.length());;    // Store the received message for RGB LED color control
  }
  // Check for LED color control messages
  if (String(topic) == ledColorTopic) {
    ledColor = messageTemp;    // Store the received message for RGB LED color control
  }
  if (String(topic) == ledbrightness) {
    brightness = messageTemp.toInt();    // Store the received message for RGB LED color control
    Serial.println("brightness:"+brightness);
  }
}

// Connect to Wi-Fi
void setupWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
}

// Connect to MQTT broker
void setupMQTT() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_Client")) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(ledTopic);       // Subscribe to LED ON/OFF control topic
      client.subscribe(ledColorTopic);  // Subscribe to RGB LED color control topic
      client.subscribe(ledbrightness);
      client.subscribe(assignmentclock);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// Functions to control RGB LED color
void setRed(int i) {
  ledcWrite(redChannel, i);   // Set Red channel to max
  ledcWrite(greenChannel, 255);   // Green off
  ledcWrite(blueChannel, 255);    // Blue off
}

void setGreen(int j) {
  
  ledcWrite(redChannel, 255);     // Red off
  ledcWrite(greenChannel, j); // Set Green channel to max
  ledcWrite(blueChannel, 255);    // Blue off
  
}

void setBlue(int k) {
  ledcWrite(redChannel, 255);     // Red off
  ledcWrite(greenChannel, 255);   // Green off
  ledcWrite(blueChannel, k);  // Set Blue channel to max
}

void setRGBColor(String color) {
  if (color == "RED") {
    setRed(brightness);
  } else if (color == "GREEN") {
    setGreen(brightness);
  } else if (color == "BLUE") {
    setBlue(brightness);
  } else {
    // Turn off LED or set a default color if color is unrecognized
    ledcWrite(redChannel, 0);
    ledcWrite(greenChannel, 0);
    ledcWrite(blueChannel, 0);
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize PWM channels for RGB control
  ledcSetup(redChannel, pwmFreq, pwmResolution);
  ledcSetup(greenChannel, pwmFreq, pwmResolution);
  ledcSetup(blueChannel, pwmFreq, pwmResolution);

  ledcAttachPin(redPin, redChannel);
  ledcAttachPin(greenPin, greenChannel);
  ledcAttachPin(bluePin, blueChannel);

  pinMode(ledPin, OUTPUT);    // Initialize built-in LED pin
  dht.begin();                // Initialize DHT11 sensor
  
  setupWiFi();                // Connect to Wi-Fi
  setupMQTT();                // Connect to MQTT broker
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Reconnect if disconnected from MQTT broker
  if (!client.connected()) {
    setupMQTT();
  }
  client.loop();

  // Control RGB LED based on ledColor variable
  if (ledControl == "ON" && temperature < 30) {
    setRGBColor(ledColor);
  } else {
    if (temperature > 25) {
      setRed(brightness);
    } else {
      setGreen(brightness);
    }
  }

  // Check if reading is successful
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Publish temperature and humidity to their respective topics
  String temperatureStr = String(temperature);
  String humidityStr = String(humidity);

  Serial.print("Publishing temperature: ");
  Serial.println(temperatureStr);
  client.publish(temperatureTopic, temperatureStr.c_str());

  Serial.print("Publishing humidity: ");
  Serial.println(humidityStr);
  client.publish(humidityTopic, humidityStr.c_str());

  // Publish status message every 10 seconds
  static unsigned long lastMsg = 0;
  unsigned long now = millis();
  if (now - lastMsg > 10000) { // 10-second interval
    lastMsg = now;
    String statusMessage = "ESP32 is online";
    client.publish(statusTopic, statusMessage.c_str());
  }

  // Check if Firebase is ready and if we have a valid timestamp from assignment2/time
  if (Firebase.ready() && signupOK && !currenttime.isEmpty() && 
      (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Structure for humidity data
    String humidityPath = "sensor_data/humidity/" + String(humidityIndex);
    if (Firebase.RTDB.setString(&fbdo, humidityPath + "/time", currenttime) &&
        Firebase.RTDB.setFloat(&fbdo, humidityPath + "/humidityvalue", humidity)) {
      Serial.println("Humidity data sent to Firebase at index " + String(humidityIndex));
      humidityIndex++; // Increment index for next reading
    } else {
      Serial.println("FAILED to send humidity data");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    // Structure for temperature data
    String temperaturePath = "sensor_data/temperature/" + String(temperatureIndex);
    if (Firebase.RTDB.setString(&fbdo, temperaturePath + "/time", currenttime) &&
        Firebase.RTDB.setFloat(&fbdo, temperaturePath + "/temperaturevalue", temperature)) {
      Serial.println("Temperature data sent to Firebase at index " + String(temperatureIndex));
      temperatureIndex++; // Increment index for next reading
    } else {
      Serial.println("FAILED to send temperature data");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }

  delay(1000); // Delay before next loop iteration
}

