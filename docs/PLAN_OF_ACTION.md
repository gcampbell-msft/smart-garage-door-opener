# Plan of action

In order to best understand what hardware I should buy (WiFi or BlueTooth), it would be wise to test out integrating with Home Assistant in both ways. 

1. Create minimal projects that integrate with Home Assistant over:
    1. WiFi
    1. Bluetooth

2. Create a non-smart Garage Door Opener using the Arduino Mega 2560 that I already have. 
    This can be completed simultaneously with #1, because it does not require the "smart" component, it can simply interact via a button press or over serial when connected to a computer via USB.
    This will require various hardware and plans documented in GARAGE_DOOR_OPENER.md

3. Assess and decide which "smart" integration mechanism I will use, either WiFi or Bluetooth. This decision will be based on several factors:
    1. Cost
    1. Ease of integration with Home Assistant
    1. Range. I estimate that a Range of at least 30 feet, maybe more for the Arduino to connect to the Raspberry Pi with Home Assistant on it. 

4. Update non-smart garage door opener from #2 by implementing the "smart" technology based on the decision made in #3. 