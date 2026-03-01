# SmartVault Firmware Context

## Overview
- ESP-IDF firmware for SmartVault on ESP32.
- Coordinates provisioning, networking, NFC reads, and backend registration.

## Hardware Summary
- MCU: ESP32
- NFC: 125 KHz UART reader
- Actuators/sensors: keypad, solenoid, buzzer, tamper

## Boot Flow
1. Initialize NVS and persistent settings.
2. Initialize peripheral modules.
3. Run provisioning when required.
4. Connect to Wi-Fi.
5. Register device and transition to normal mode.

## Core Modules
- `nvs_storage`
- `wifi_manager`
- `nfc`
- `provisioning`
- `api_client`

## TLS Policy
- Development may disable TLS verification by config.
- Production uses certificate validation.
