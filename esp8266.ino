#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.
long serial_speed = 115200;
const char* ssid = "wifi-ssid";
const char* password = "password";
const char* mqtt_server = "192.168.1.x";
int mqtt_port = 1883;
const char* user_name = "mqtt-username";
const char* user_password = "mqtt-password";

const char* topic_subscribe = "charge";
const char* topic_publish = "charge_reply";
WiFiClient espClient;
PubSubClient client(espClient);

const byte ledPin = LED_BUILTIN; 
const byte toggle = 0;  // physical switch
bool isCharging = false;

long lastMsg = 0;
char msg[50];
int value = 0;

//non blocking timer
unsigned long timer1 = 0;
unsigned long timer2 = 0;
unsigned long timer3 = 0;
unsigned long timer4 = 0;
unsigned long timerForAutoMode2 = 0;
unsigned long currentMillis = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
 resetTimer(timerForAutoMode2);
 Serial.print("Message arrived [");
 Serial.print(topic);
 Serial.print("] ");
 for (int i=0;i<length;i++) {
  char receivedChar = (char)payload[i];
  Serial.print(receivedChar);
  // ESP8266 outputs are "reversed"
  if (receivedChar == '1'){
    digitalWrite(ledPin, LOW);
    digitalWrite(toggle, LOW);
    isCharging = true;
    client.publish(topic_publish, "1");
    }
  else if (receivedChar == '0'){
    digitalWrite(ledPin, HIGH);
    digitalWrite(toggle, HIGH);
    isCharging = false;
    client.publish(topic_publish, "0");
    }
  }
  Serial.println();
}

// well this function is blocking when mqtt host is not exist
bool reconnect() {
  Serial.println("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  //if (client.connect(clientId.c_str())) { // if you dont use password
  if (client.connect(clientId.c_str(),user_name,user_password)) {
    Serial.println("connected");
    client.subscribe(topic_subscribe);
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    return false;
  }
}

void setup() {
  Serial.begin(serial_speed);
  setup_wifi(); // blocking until wifi connected
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  pinMode(ledPin, OUTPUT);
  pinMode(toggle, OUTPUT);  // physical switch
}

bool afterMillis(const unsigned long & afterMillis, unsigned long & timer, bool (* callback)(unsigned long & timer)){
  if(currentMillis - timer > afterMillis){
    callback && callback(timer);
    return true;
  }
  return false;
}

bool resetTimer(unsigned long & timer){
  timer = currentMillis;
  return true;
}

void chargeMode(const unsigned int & blinkTime){
  afterMillis(blinkTime,timer1,nullptr) && digitalWriteWrapper(ledPin, HIGH);
  afterMillis(blinkTime * 2,timer1,resetTimer) && digitalWriteWrapper(ledPin, LOW);
}

bool delayWrapper(const unsigned int & delayTime){
  delay(delayTime);
  return true;
}

bool digitalWriteWrapper(const int & pin, const int & val){
  digitalWrite(pin, val);
  return true;
}

// we use block mode when reconnect function is blocking
void autoControlMode(bool blocking, const unsigned int & blinkTime,const unsigned int & retryTime,const unsigned int & chargeHours,const unsigned int & lastHours, bool (* callback)()){
  (blocking ? delayWrapper(blinkTime) : afterMillis(blinkTime,timer2,nullptr)) && digitalWriteWrapper(ledPin, HIGH);  // remember HIGH is down
  (blocking ? delayWrapper(blinkTime) : afterMillis(blinkTime * 2,timer2,resetTimer)) && digitalWriteWrapper(ledPin, LOW);
  (blocking ? delayWrapper(retryTime) : afterMillis(retryTime,timer3,resetTimer)) && callback && callback();
  afterMillis(chargeHours * 60 * 60 * 1000,timer4,nullptr) && digitalWriteWrapper(toggle, HIGH) && afterMillis(lastHours * 60 * 60 * 1000,timer4,resetTimer) && digitalWriteWrapper(toggle, LOW);
}

void loop()
{
  currentMillis = millis();

  if(WiFi.status() != WL_CONNECTED){
    Serial.println("no wifi");
    digitalWrite(ledPin, HIGH);
    digitalWrite(toggle, HIGH);
    setup_wifi(); // blocking until wifi connected
  }
  
  // auto control mode one
  // when mqtt broker is inaccessiable
  else if(!client.connected()) {
    Serial.println("no mqtt broker");
    Serial.println("auto control mode 1");   // if you use false here while reconnect may be blocking, be sure set timer bigger than the biggest time reconnect function might block. 
    //autoControlMode(true,300,5000,1,6,reconnect); // when host may be not exist ,use blocking mode, it means you cant ping to your host.
    autoControlMode(false,6000,5000,1,6,reconnect); // if you sure your host won't be offline, use non-blocking time, aka. false here, or like it said the very first line above.
  }                                                 // in my case, it could be set to 6 seconds

  // auto control mode two
  // when mqtt publisher is inaccessiable
  // after 12 minutes not receieve message from broker
  else if(afterMillis(12 * 60 * 1000,timerForAutoMode2, nullptr)){
    Serial.println("no publisher");
    Serial.println("auto control mode 2");
    autoControlMode(false,3000,0,1,6,nullptr);     
  }

  // blink when charging
  else if(isCharging){
    Serial.println("turn on charge mode");
    chargeMode(1000);
  }
 
  client.loop();
}