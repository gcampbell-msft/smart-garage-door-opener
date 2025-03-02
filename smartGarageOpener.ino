#include "wifi_credentials.h"
#include "mqtt_broker_credentials.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define REEDSWITCHPIN 4 // D2
#define RELAYCONTROLPIN 5 // D1
#define ONBOARDLED 2 // D4
const char* statusTopic = "garage_door/status";
const char* availabilityTopic = "garage_door/availability";
const String closeCommand = "CLOSE";
const String openCommand = "OPEN";
const int GARAGE_OPEN_CLOSE_TIME = 15;
const int DELAY_AFTER_BUTTON_PRESS = 2;
const int AVAILABILITY_TIMER = 10000; // 10 seconds, but in ms
const int WIFI_CHECK_DELAY = 20;

// define enum for states
enum GarageDoorState
{
  Closed,
  Open,
  Closing,
  Opening,
  Unknown
};
WiFiClient wifiClient;
PubSubClient client(wifiClient);

String command = "";
int TIMENOTSETFLAG = -1;
int timeMovementStarted = TIMENOTSETFLAG;
int availabilityTime = TIMENOTSETFLAG;
int wifiTime = 0;
bool initialized = false;
GarageDoorState garageDoorState = Closed;

void setupWifi()
{
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) 
  {
     delay(500);
     digitalWrite(ONBOARDLED, LOW);
     Serial.print("*");
  }
  
  Serial.println("");
  Serial.println("WiFi connection Successful");
  Serial.print("The IP Address of ESP8266 Module is: ");
  Serial.println(WiFi.localIP());// Print the IP address
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  payload[length] = '\0';
  Serial.print("Message received from topic: ");
  Serial.println(topic);

  Serial.print("Payload received: ");
  String payloadString = String((char*)payload);
  Serial.println(payloadString);

  command = String((char*)payload);
}

void setupMQTT()
{
  client.setServer(MQTT_BROKER_ADDRESS, 1883);
  client.setCallback(mqttCallback);
  client.setKeepAlive(20);
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USER_NAME, MQTT_USER_PASSWORD, availabilityTopic, 0, true, "unavailable")) {
      Serial.println("connected");
      client.subscribe("garage_door/buttonpress");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

char* getGarageDoorStateString(int state)
{
  switch (state)
  {
    case Closed:
      return (char*)"closed";
    case Closing:
      return (char*)"closing";
    case Opening:
      return (char*)"opening";
    case Open:
      return (char*)"open";
    default:
      return (char*)"None";
  }
}

void pressGarageDoorButton()
{
  Serial.println("Pressing garage door button...");
  digitalWrite(RELAYCONTROLPIN, HIGH);
  delay(500);
  digitalWrite(RELAYCONTROLPIN, LOW);
  delay(2000);
  Serial.println("Finished pressing garage door button.");
}

void publishStatusWithRetain(char* message)
{
  client.publish(statusTopic, message, true);
}

void setGarageDoorClosed()
{
  garageDoorState = Closed;
  publishStatusWithRetain(getGarageDoorStateString(garageDoorState));
  timeMovementStarted = TIMENOTSETFLAG;
  Serial.println("Garage door state is now closed");
}

void setGarageDoorOpen()
{
  garageDoorState = Open;
  publishStatusWithRetain(getGarageDoorStateString(garageDoorState));
  timeMovementStarted = TIMENOTSETFLAG;
  Serial.println("Garage door state is now open");
}

void setGarageDoorOpening()
{
  garageDoorState = Opening;
  publishStatusWithRetain(getGarageDoorStateString(garageDoorState));
  timeMovementStarted = millis();
  Serial.println("Garage door state is now opening");
}

void setGarageDoorClosing()
{
  garageDoorState = Closing;
  publishStatusWithRetain(getGarageDoorStateString(garageDoorState));
  timeMovementStarted = millis();
  Serial.println("Garage door state is now closing");
}

void setup(void)
{
  pinMode(ONBOARDLED, OUTPUT);
  digitalWrite(ONBOARDLED, HIGH);

  Serial.begin(9600);
  setupWifi();
  setupMQTT();

  // 1 = disconnected, 0 = disconnected
  pinMode(REEDSWITCHPIN, INPUT_PULLUP);

  pinMode(RELAYCONTROLPIN, OUTPUT);

  garageDoorState = Unknown;
  availabilityTime = millis();
}

void loop() 
{
  int checkWifiTime = millis();

  // checking for WIFI connection
  if ((WiFi.status() != WL_CONNECTED))
  {
    if (checkWifiTime - wifiTime >= WIFI_CHECK_DELAY) {
      digitalWrite(ONBOARDLED, LOW);
      Serial.print(millis());
      Serial.println("Reconnecting to WIFI network");
      WiFi.disconnect();
      WiFi.reconnect();
      wifiTime = checkWifiTime;
      digitalWrite(ONBOARDLED, HIGH);
    } else {
      // If we're not connected, we don't want to do anything else.
      return;
    }
  }

  if (!client.connected())
  {
    digitalWrite(ONBOARDLED, LOW);
    reconnect();
    digitalWrite(ONBOARDLED, HIGH);
  }
  client.loop();

  const int reedSwitchInput = digitalRead(REEDSWITCHPIN);
  int currentTime = TIMENOTSETFLAG;

  int checkAvailabilityTime = millis();

  if (checkAvailabilityTime - availabilityTime > AVAILABILITY_TIMER || !initialized)
  {
    availabilityTime = checkAvailabilityTime;
    client.publish(availabilityTopic, "available", true);
  }

  if (!initialized)
  {
    if (reedSwitchInput == 1)
    {
      setGarageDoorOpen();
    }
    else if (reedSwitchInput == 0)
    {
      setGarageDoorClosed();
    }

    initialized = true;
  }

  switch (garageDoorState)
  {
    case Open:
      // If the reed switch shows it is closed, then we are closed.
      if (reedSwitchInput == 0)
      {
        setGarageDoorClosed();
        break;
      }

      // If the button is pressed from Home Assistant, we go to closing
      if (command != "")
      {
        if (command == closeCommand)
        {
          pressGarageDoorButton();
          setGarageDoorClosing();
          // When we get button press, press the button and go to closing state, break early.
        }

        command = "";
      }
      break;
    case Closed:
      // In closed, we first want to check sensor because it will move to opening
      // where we ignore commands.
      if (reedSwitchInput == 1)
      {
        setGarageDoorOpening();
        break;
      }

      if (command != "")
      {
        if (command == openCommand)
        {
          pressGarageDoorButton();
          setGarageDoorOpening();
        }

        command = "";
      }
      break;
    case Opening:
      // Ignore commands in opening.
      command = "";

      // If we detect it's closed, report and break.
      if (reedSwitchInput == 0)
      {
        setGarageDoorClosed();
        break;
      }

      currentTime = millis();
      // If it's been 15 seconds, assume we're fully open.
      if (timeMovementStarted != TIMENOTSETFLAG && currentTime - timeMovementStarted > 15000)
      {
        setGarageDoorOpen();
      }
      break;
    case Closing:
      // Ignore commands in closing.
      command = "";

      // If we detect it's closed, report and break;
      if (reedSwitchInput == 0)
      {
        setGarageDoorClosed();
        break;
      }

      currentTime = millis();
      // If it's been 15 seconds, this means we should've been closed by now, if we haven't 
      // detected it, mark it unknown.
      if (timeMovementStarted != TIMENOTSETFLAG && currentTime - timeMovementStarted > 15000)
      {
        garageDoorState = Unknown;
        publishStatusWithRetain(getGarageDoorStateString(garageDoorState));
        timeMovementStarted = TIMENOTSETFLAG;
        Serial.println("Garage door state is now unknown");
      }
      break;
    case Unknown:
      if (reedSwitchInput == 0)
      {
        setGarageDoorClosed();
        break;
      }

      if (command != "")
      {
        if (command == openCommand)
        {
          pressGarageDoorButton();
          setGarageDoorOpening();
        }
        else if (command == closeCommand)
        {
          pressGarageDoorButton();
          setGarageDoorClosing();
        }

        command = "";
      }
      break;
  }

  delay(50);
}