# Development Setup

## Prerequisites
- ESP-IDF toolchain
- Python 3 and required ESP-IDF dependencies

## Build
```bash
idf.py set-target esp32
idf.py build
```

## Flash and Monitor
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## VS Code
- Install ESP-IDF extension.
- Configure ESP-IDF path and Python interpreter.
