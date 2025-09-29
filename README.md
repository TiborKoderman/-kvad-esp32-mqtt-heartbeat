# ESP32 MQTT Heartbeat

This project is designed for the ESP32-S3-WROOM-1 N16R8 and implements a simple MQTT client that connects to a Wi-Fi network and sends a heartbeat message every 30 seconds. The heartbeat message contains the following structure:

```
{ timestamp, bootId, seq, uptimeS }
```

## Project Structure

```
esp32-mqtt-heartbeat
├── src
│   └── main.cpp          # Main application code
├── include
│   └── config.h         # Configuration constants (Wi-Fi credentials, MQTT broker details)
├── lib                   # Additional libraries (if needed)
├── test                  # Unit tests or integration tests
├── platformio.ini       # PlatformIO configuration file
└── README.md             # Project documentation
```

## Setup Instructions

1. **Clone the repository** or download the project files.
2. **Install PlatformIO** if you haven't already.
3. Open the project in PlatformIO.
4. Update the `include/config.h` file with your Wi-Fi credentials and MQTT broker details.
5. Build and upload the project to your ESP32-S3 board.

## Usage

Once the project is uploaded to the ESP32-S3, it will connect to the specified Wi-Fi network and MQTT broker. The device will send a heartbeat message every 30 seconds, which can be monitored via the MQTT broker.

## Dependencies

This project requires the following libraries:

- Wi-Fi library for ESP32
- MQTT library (e.g., PubSubClient)

Make sure to include these libraries in your `platformio.ini` file.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.