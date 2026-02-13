## Flashing when building with CMake.

python <path>/ESP8266_RTOS_SDK/components/esptool_py/esptool/esptool.py --chip esp8266 --port COM3 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 2MB 0x0 /c/users/gdcam/Projects/smartGarageOpener/build/bootloader/bootloader.bin 0x10000 /c/users/gdcam/Projects/smartGarageOpener/build/smart_garage_door.bin 0x8000 /c/users/gdcam/Projects/smartGarageOpener/build/partitions_singleapp.bin


python <path>/ESP8266_RTOS_SDK/components/esptool_py/esptool/esptool.py --chip esp8266 --port COM3 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 2MB 0x0 C:/users/gdcam/Projects/smartGarageOpener/build/bootloader/bootloader.bin 0x10000 C:/users/gdcam/Projects/smartGarageOpener/build/smart_garage_door.bin 0x8000 C:/users/gdcam/Projects/smartGarageOpener/build/partitions_singleapp.bin