# ESP32 OTA update and send telemetry data to Thingsboard.

This is Tutorial of EPS32 OTA device and how to send telemetry data with mqtt to Thingsboard.

## Environnment

This project need a specific environment:

- Board: EPS32-S
- Platform: platformio
- Programming Language: C++
- Operating System: Windows 11

## Setup Thingsboard

This is How to setup Thingsborad for OTA update and recive data from one EPS32, the step is below:

1. Go to Thingsboard.
2. Click to "Entities -> Devices".
3. Add new device.

If you have many ESP32 device, the step is below:

1. Go to Thingsboard.
2. Click to "Profiles -> Device profiles".
3. Add new device profile.
4. Click to "Entities -> Devices".
5. Go to what device you want to add to profile.
6. Assign to your profile.

   
## Setup ESP32 code

Before you build or upload code to your ESP32 you need to config some value, the step is below:

1. Download the project.
2. Go to this file: "ESP32_OTA/src/main.cpp"
3. Config wifi:

   ```bash
   const char *ssid = "(Your_wifi_name)";
   const char *password = "(Your_wifi_password)";
   ```
    
4. Config mqtt:

   ```bash
   const char *mqtt_server = "(Your_mqtt_server)";
   const char *mqtt_token = "(Your_access_token)";
   ```

   > You can get access token in Thingsboard webside, go to your device and click the copy access token buttom.

## Update version

If you want to update firmware version to Thingsboard, you need to do this step:

1. Go to this file: "ESP32_OTA/src/main.cpp"
2. Edit this:

   ```bash
   #define SRC_ESP_VER (version)
   ```

   > The value (version) must be float and unique from other version.

3. Build or upload the file.
4. Go to Thingsboard.
5. Click Advanced features -> OTA updates.
6. Add new package. You can edit your title but Version must be as same as is step2.
   > The binary file (.bin) is "ESP32_OTA\.pio\build\esp32dev\firmware.bin"


