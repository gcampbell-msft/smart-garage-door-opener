#define MagneticSwitchPin 52
#define LedDrivenBySwitchPin 53


int switchStatus;

void setup() {
  // LED Driven by Magnetic Switch
  pinMode(LedDrivenBySwitchPin, OUTPUT);

  // Magnetic Switch Pin
  // 1 = disconnected, 0 = connected
  pinMode(MagneticSwitchPin, INPUT_PULLUP);

  pinMode(RelayControlPin, OUTPUT);
  
  // Output from relay, input to the board
  // 1 = disconnected, 0 = connected 
  pinMode(RelayOutputPin, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  switchStatus = digitalRead(MagneticSwitchPin);

  int ledStatus = digitalRead(LedDrivenBySwitchPin);

  switch (ledStatus) {
    case 1:
      if (switchStatus == 1) {
        digitalWrite(LedDrivenBySwitchPin, LOW);
        Serial.println("switch isn't triggered");
      }
      break;
    case 0:
      if (switchStatus == 0) {
        digitalWrite(LedDrivenBySwitchPin, HIGH);
        Serial.println("switch is triggered");
      }
      break;
  }

}
