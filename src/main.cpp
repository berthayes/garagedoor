#include <Arduino.h>
#include <Wire.h>
#include <SparkFunADXL313.h> 
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

ADXL313 myAdxl;

// This is your Apache Kafka REST Proxy
#define SERVER_IP "192.168.1.9:8082"

#ifndef STASSID
#define STASSID "YOUR_WIFI_SSID"
#define STAPSK  "YOUR_WIFI_WPA_KEY"
#endif

const int POST_PIN = 15;
const int WAKE_PIN = 10;
const int RED_PIN = 12;
const int YELLOW_PIN = 13;

void postxyz();
void print_stats();
void chill();

void setup()
{
  //delay(1000);
  Serial.begin(9600);

  pinMode(POST_PIN, OUTPUT); 
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(WAKE_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);


  Wire.begin();

  if (myAdxl.begin() == false) //Begin communication over I2C
  {
    Serial.println("The sensor did not respond. Please check wiring.");
    while(1); //Freeze
  }
  else {
    Serial.print("Sensor is connected properly.");
  }
  
  myAdxl.measureModeOn(); // wakes up the sensor from standby and puts it into measurement mode

  if(WiFi.begin(STASSID, STAPSK)) {
    Serial.println("We've got wifi!");
    digitalWrite(YELLOW_PIN, HIGH);
  } // connect to wifi
  else {
    digitalWrite(YELLOW_PIN, LOW);
    Serial.println("No wifi!");
  }

}

void loop()
{
  digitalWrite(LED_BUILTIN, HIGH);
  //Serial.println("We got to loop!");
  //turn off the built in bright blue LED

  chill(); // 10 second delay while we wait for garage door to open/close fully
  if(myAdxl.dataReady()) // check data ready interrupt, note, this clears all other int bits in INT_SOURCE reg
  {
    //print_stats();
    //Serial.println("Adxl is dataReady");
    postxyz();
  }
  else
  {
    Serial.println("Waiting for dataReady.");
  }  
  delay(50);
}

void postxyz() {
  digitalWrite(WAKE_PIN, HIGH);

  // make a json object from XYZ position values
  myAdxl.readAccel(); // read all 3 axis, they are stored in class variables: myAdxl.x, myAdxl.y and myAdxl.z

  StaticJsonDocument<300> doc;
  JsonObject root = doc.createNestedObject();
  JsonArray records = root.createNestedArray("records");
  JsonObject records_0 = records.createNestedObject();
  records_0["key"] = "esp8266_ADXL313";
  JsonObject records_0_value = records_0.createNestedObject("value");
  records_0_value["X value"] = myAdxl.x;
  records_0_value["Y value"] = myAdxl.y;
  records_0_value["Z value"] = myAdxl.z;
  
  char buffer[300];
  serializeJson(root, buffer);
  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {
    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    // configure traged server and url
    // TODO: make the topic a variable somewhere
    http.begin(client, "http://" SERVER_IP "/topics/garage"); //HTTP
    http.addHeader("Content-Type", "application/vnd.kafka.json.v2+json");
    http.addHeader("Accept", "application/vnd.kafka.v2+json");

    Serial.print("[HTTP] POST...\n");
    // start connection and send HTTP header and body
    //int httpCode = http.POST("{\"hello\":\"world\"}");
    int httpCode = http.POST(buffer);

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        digitalWrite(POST_PIN, HIGH); 
        const String& payload = http.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
        //delay(10000); // HERE is a better place
      }
    } else {
      digitalWrite(RED_PIN, HIGH);
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
  delay(2000); // leave the status LEDs on for 2 seconds, then turn them off
  digitalWrite(POST_PIN, LOW);
  digitalWrite(WAKE_PIN, LOW);
  digitalWrite(RED_PIN, LOW);
  

}

void print_stats() {

    myAdxl.readAccel(); // read all 3 axis, they are stored in class variables: myAdxl.x, myAdxl.y and myAdxl.z
    Serial.print("x: ");
    Serial.print(myAdxl.x);
    Serial.print("\ty: ");
    Serial.print(myAdxl.y);
    Serial.print("\tz: ");
    Serial.print(myAdxl.z);
    Serial.println();
}


void chill() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(10000);
  digitalWrite(LED_BUILTIN, HIGH);
}
