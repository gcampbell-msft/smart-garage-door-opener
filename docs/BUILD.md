# Build environment and setup instructions

The setup for this build environment is primarily located [here](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/windows-setup.html). However, I've re-documented the most essential steps for my workflow here. 

## Download the all-in-one toolchain & msys2

1. Unzip the toolchain, downloaded from [here](https://dl.espressif.com/dl/esp32_win32_msys2_environment_and_toolchain-20181001.zip), and extract it to a location of your choice. In my case, I extracted to C:\esp8266\xtensa-lx106-elf
2. **Optional** I also downloaded the standalone toolchain from [here](https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-win32.zip).

## Clone the ESP8266 RTOS repository 

1. Clone from [here](https://github.com/espressif/ESP8266_RTOS_SDK).

## Setup environment

1. From your extracted all-in-one toolchain + msys, locate the mingw32.exe, and open it.
2. Navigate to the location on your filesystem where you've cloned this repository. 
3. Setup your **IDF_PATH** variable. To do this permanently, find your **.bashrc** file and add`export IDF_PATH=<path to ESP8266RTOS repo>`
4. Ensure python is installed. [Install the required python packages](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html#install-the-required-python-packages). 
5. Add the PATH to your extracted toolchain to **PATH**. See note in **Step 3** to do this permanently. 
6. **Optional** Add VS Code to your PATH as well, so that you can use `code .` to open VS Code.
7. If using the CMake option below, add Ninja.exe to your PATH.

You will always need to do the following to open up your developer environment. 

1. Open mingw32.exe
2. Navigate to the project directory.
3. `code . &`

## Build instructions

### CMake Option [Recommended]

CMake presets are defined to control the configuration. For the main project, there are two Configure Presets. [CMakePresets.json](../CMakePresets.json)

1. PROD - This turns off test mode and sets up the reqiured compilers and PATh
2. TEST - This turns on the TEST_MODE preprocessor macro. 

Ensure that the `cmake.useVsDeveloperEnvironment` is set to `auto`. 

For the test project, there is only one [configure preset](../test/CMakePresets.json), and the `auto` setting from the `cmake.useVsDeveloperEnvironment` from the CMake Tools extension will set up the VS environment and support Windows building.

#### Tasks

There are also tasks set up to easily build and flash the code as needed, regardless of the configuration. See [tasks.json](../.vscode/tasks.json).

### Makefile option

Thanks to the default ESP8266_RTOS_SDK build system, you should be able to utilize the following two commands primarily. 

#### Build your code

Utilize `make` from the root folder of this project.

#### Flash your code

Utilize `make flash` to flash your project. 

#### Setup flash (if needed)

You may need to set up what port you are using and what baud rate to use, etc. Utilize `menu menuconfig`. 

### Serial Monitor

I recommend the [Serial Monitor Extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.vscode-serial-monitor). 