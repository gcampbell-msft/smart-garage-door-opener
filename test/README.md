# Unit Tests

This directory contains unit tests that run on the host machine (not on ESP8266).

## Building and Running Tests

### Windows (PowerShell)
```powershell
cd test
mkdir build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Or use the custom target:
```powershell
cmake --build . --target run_tests
```

### Linux/macOS
```bash
cd test
mkdir build && cd build
cmake ..
make
ctest --output-on-failure
```

## What's Tested

- **State Machine** (`test_state_machine.c`): Tests all garage door state transitions, event handling, and edge cases

## Architecture

The tests compile the pure C state machine code (`garage_state_machine.c`) using your native compiler (MSVC on Windows, GCC/Clang on Linux/macOS). This is separate from the ESP-IDF build which cross-compiles for ESP8266.

The state machine has zero hardware dependencies, making it fully testable on any platform.
