# Casio fx-CP400 Gemini AI Chat Client

A native AI chat client for the Casio ClassPad II (fx-CP400) graphing calculator, which communicates with the Google Gemini API via an ESP32 module (UART/Wi-Fi).

## Features
* **Hollyhock-2 SDK GUI:** User-friendly interface with text input, send button, and separate reply window.
* **Smooth Rendering:** The AI ​​response is cached in RAM and displayed in a separate pop-up dialog to prevent rendering errors when using a software keyboard.
* **ESP32 Bridge:** Handles the Wi-Fi connection and encrypted HTTPS communication with the Gemini API.
* **ASCII Sanitization:** Automatically cleans and formats responses for small computer displays.
