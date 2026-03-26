# ⚡ ESP32 Power Electronics Simulator

A comprehensive real-time Power Electronics simulation system built on ESP32. This project visualizes AC/DC rectifier circuits and waveforms on a TFT display, shows system status on an OLED screen, and offers full remote control via a Telegram Bot and a Local Web Server (IP Panel).

## ✨ Features
* **Multiple Rectifier Topologies:** Supports 1-Phase (Half/Full wave) and 3-Phase (Half/Full wave) rectifiers.
* **Dynamic Load Types:** Switch between Pure Resistive (R) and Inductive (R+L) loads to see real-time current waveform changes.
* **Live Waveform Visualization:** Real-time plotting of Input Voltage, Output Voltage, Input Current, and Output Current on a TFT display.
* **Dual Display System:** * **TFT Screen:** Draws the circuit diagrams, diodes, phases, and live waveforms.
  * **OLED Display (I2C):** Shows text-based system info (V_peak, V_dc, Load Type, and System Status).
* **IoT Remote Control:**
  * 📱 **Telegram Bot:** Send commands to change circuits, loads, and views with interactive table menus.
  * 🌐 **Web IP Panel:** A built-in web server with an HTML/JS dashboard for local network control and live Canvas drawing.

## 🛠️ Hardware Requirements
* ESP32 Development Board
* TFT Display (compatible with `TFT_eSPI` library)
* OLED Display 128x64 (I2C)
* Active/Passive Buzzer (connected to Pin 26)
* Jumper Wires & Breadboard

## 📚 Required Libraries
Make sure to install the following libraries in your Arduino IDE:
* `TFT_eSPI`
* `U8g2`
* `UniversalTelegramBot`
* `ArduinoJson`
* `WiFiClientSecure` & `WebServer` (Built-in)

## 🚀 How to Use
1. Open the `.ino` file in Arduino IDE.
2. Update your WiFi credentials (`ssid` and `password`).
3. Update your Telegram Bot Token (`BOTtoken`) and your Chat ID (`CHAT_ID`).
4. Upload the code to your ESP32.
5. Send the word `الاوامر` to your Telegram bot or access the ESP32 IP address via your browser to control the simulation!
