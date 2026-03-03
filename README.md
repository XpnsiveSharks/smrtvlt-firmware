# SmartVault Firmware

ESP-IDF firmware for the SmartVault lock controller.

## Features
- Device provisioning via SoftAP portal
- Wi-Fi credential and runtime configuration storage
- NFC reader integration
- Backend device registration over HTTPS

## Hardware
- ESP32 development board
- 125 KHz UART NFC reader
- Keypad, buzzer, solenoid lock, tamper switch

## Configuration

| Setting | Default | Where |
|---|---|---|
| `CONFIG_API_BASE_URL` | `http://localhost:8000` | `sdkconfig.defaults` |

`CONFIG_API_BASE_URL` is the backend URL the device will call after provisioning.
Change this value in `sdkconfig.defaults` before flashing for any non-local deployment.

During captive-portal provisioning the user can override it via the optional **API URL** field.
If left blank, the firmware falls back to `CONFIG_API_BASE_URL`.

> Backend Swagger UI: `http://localhost:8000/docs`

## Quick Start
1. Install ESP-IDF and export environment.
2. Configure target and project settings.
3. Build, flash, and monitor firmware.

## Docs
- [`docs/DEV_SETUP.md`](docs/DEV_SETUP.md)
- [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md)
- [`docs/HARDWARE_TESTS.md`](docs/HARDWARE_TESTS.md)
