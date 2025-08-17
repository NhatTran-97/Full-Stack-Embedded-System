# Ethernet_W5500 Project

This project explores how to interface **microcontrollers (MCUs)** with the **W5500 Ethernet module** for embedded systems and IoT applications.  
It is designed to be expandable — starting with **ESP32** and later supporting other MCUs such as **STM32**.

## Structure
Ethernet_W5500/
├── esp_ethernet_arduino/ # Example using ESP32 with Arduino framework
├── esp_ethernet_idf/ # Example using ESP32 with ESP-IDF framework
├── log/ # Debug / runtime logs (ignored by git)
└── README.md # Project documentation

## Purpose
- Learn how to connect MCUs with the W5500 Ethernet module.
- Compare and evaluate different development frameworks (Arduino, ESP-IDF, STM32 HAL, etc.).
- Provide reusable examples for networking (TCP/UDP, MQTT, HTTP) over Ethernet.

## Current Status
- ✅ ESP32 + W5500 (Arduino framework)  
- ✅ ESP32 + W5500 (ESP-IDF framework)  
- ⏳ STM32 + W5500 (planned)

