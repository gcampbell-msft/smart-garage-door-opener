#define GARAGE_LED 7

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(GARAGE_LED, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readString();
    if (input == "open" || input == "close") {
      openOrCloseGarage();
    }
  }
}

void openOrCloseGarage() {
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(GARAGE_LED, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(GARAGE_LED, LOW);
}
