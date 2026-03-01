# SmartVault Firmware Task Roadmap

1. ✓ Project setup and build scaffold.
2. ✓ NVS storage and Wi-Fi manager.
3. NFC reader integration.
4. Provisioning portal.
5. Device registration API.
6. Keypad and PIN authentication.
7. Solenoid, buzzer, and tamper behavior.
8. WebSocket/client session handling.
9. Lock state machine and error handling.
10. Integration testing on hardware.
11. Production hardening and release prep.

---

## Notes

### Hardware Availability
- Only ESP32 board is available at this time.
- NFC reader, keypad, solenoid, buzzer, and tamper sensor are not yet available.
- Tasks 3, 6, 7 will be implemented but can only be fully tested once hardware is provided.

### Temporary: Built-in Button for Provisioning Trigger
- The ESP32 built-in button (GPIO0 / BOOT button) is used to trigger provisioning during development.
- This replaces the NFC tap trigger until the NFC reader hardware is available.
- **Must be removed and replaced with NFC trigger once hardware is provided.**
