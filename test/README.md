# Unit Tests

Unit tests for garage door controller components using Google Test. Tests run on host machine (not ESP8266).

## Tests Covered

- **State Machine**: Garage door state transitions and event handling
- **WiFi Retry Manager**: Connection retry logic and backoff behavior  
- **MQTT Retry Manager**: MQTT connection retry and reconnection handling

## Running Tests

Use CMake Tools extension in VS Code. Steps would include using the CMake Project Outline in the VS Code sidebar, selecting the `tests` folder as the active folder, configuring, and building the code. 
Then, you can utilize the Test Explorer to run tests.

