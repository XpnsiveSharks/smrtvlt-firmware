# Vault-Needed Backend Endpoints — Implementation Plan

> **Project:** `smartvault-backend`
> **Written for:** `smrtvlt-firmware` integration reference
> **Date:** 2026-03-01
> **Status:** Planning

---

## Overview

The firmware requires 4 device-facing backend endpoints and 1 mobile-facing endpoint to support the full provisioning and operation flow.

```
Mobile App ──► POST /vaults/{vault_id}/reset      (JWT auth, owner only)
                        │
                        ▼
Mobile App ──► POST /vaults/provisioning-token    (JWT auth)
                        │
                        ▼
              token displayed to user
                        │
                        ▼
Firmware ──────► Captive Portal (user enters token)
                        │
                        ▼
Firmware ──────► POST /devices/register           (no auth)
Firmware ──────► POST /devices/verify-pin         (no auth)
Firmware ──────► POST /devices/tamper             (no auth)
Firmware ──────► WS   /devices/ws                 (no auth, persistent)
```

---

## Token Format

`000000AR` — 6 random digits + 2 random uppercase letters
- Generated server-side
- Stored in `users.provisioning_token` (nullable, unique)
- Cleared immediately after successful device registration
- Single-use only

---

## 1. Mobile Endpoint — Generate Provisioning Token

### `POST /vaults/provisioning-token`

**Auth:** JWT (mobile app user)
**Router:** `app/api/v1/vaults.py`

**Response:**
```json
{ "provisioning_token": "482931KZ" }
```

**Logic:**
1. Get `current_user` from JWT
2. Call `GenerateProvisioningToken` use case
3. Generate token matching format `[0-9]{6}[A-Z]{2}`, ensure uniqueness (retry on collision)
4. Store token in `users.provisioning_token`
5. Return token

**Errors:**
- `401` — unauthenticated

---

## 2. Mobile Endpoint — Reset Vault

### `POST /vaults/{vault_id}/reset`

**Auth:** JWT (owner only)
**Router:** `app/api/v1/vaults.py`

**Response `200`:**
```json
{ "reset": true }
```

**Logic:**
1. Verify caller is vault owner → `403` if not
2. Wipe vault state in-place (same `vault_id`):
   - `pin_hash` → `NULL`
   - `pin_set_at` → `NULL`
   - `last_seen_at` → `NULL`
   - `status` → `PROVISIONING`
   - `vault_name` → keep (owner can rename later)
3. Delete all `access_logs` rows for this `vault_id`
4. If firmware is currently connected via WebSocket:
   - Send `{ "command": "reset" }` to the device via `manager.send_to_vault`
   - Firmware will wipe NVS (WiFi credentials + stored config) and reboot into provisioning mode (SoftAP)
   - Then disconnect: `manager.disconnect_vault`
5. If firmware is NOT connected (offline): no action — firmware will detect stale state on next boot (future enhancement)
6. Return `{ "reset": true }`

**Errors:**
- `403` — caller is not the vault owner
- `404` — vault not found

> **Note:** After reset, the firmware wipes NVS and reboots into SoftAP provisioning mode automatically (if online). Owner must then generate a new provisioning token and re-run the captive portal flow.

---

## 3. Firmware Endpoint — Register Device

### `POST /devices/register`

**Auth:** None (called by firmware over HTTPS)
**Router:** `app/api/v1/devices.py` *(new file)*

**Request body:**
```json
{
  "hardware_uuid": "AA:BB:CC:DD:EE:FF",
  "provisioning_token": "482931KZ"
}
```

**Response `200`:**
```json
{ "vault_id": "<uuid>", "vault_name": "My Vault" }
```

**Logic:**
1. Look up user by `provisioning_token`
2. If not found → `401 Invalid or expired provisioning token`
3. Check if `hardware_uuid` already exists in `vaults`
   - If yes → `409 Hardware already registered — reset the vault first`
4. Call `ProvisionVault` use case with `owner_id=user.id`, `hardware_uuid`
5. Clear `users.provisioning_token` (set to `NULL`)
6. Return `vault_id` and `vault_name`

**Errors:**
- `401` — token not found or already used
- `409` — `hardware_uuid` already registered (must reset vault first via `POST /vaults/{vault_id}/reset`)

---

## 4. Firmware Endpoint — Verify PIN

### `POST /devices/verify-pin`

**Auth:** None (called by firmware on every keypad entry)
**Router:** `app/api/v1/devices.py`

**Request body:**
```json
{
  "hardware_uuid": "AA:BB:CC:DD:EE:FF",
  "pin": "1234"
}
```

**Response `200`:**
```json
{ "unlocked": true }
```

**Logic:**
1. Look up vault by `hardware_uuid` → `404` if not found
2. Use `vault.owner_id` as the user context
3. Call `UnlockVaultWithPIN` use case (adapt to accept `owner_id` directly, not from JWT)
4. Return `{ "unlocked": true }` on success

**Errors:**
- `404` — hardware_uuid not registered
- `401` — wrong PIN (`InvalidPINError`)
- `423` — PIN locked out (too many attempts)

---

## 5. Firmware Endpoint — Tamper Alert

### `POST /devices/tamper`

**Auth:** None (called by firmware on forced open detection)
**Router:** `app/api/v1/devices.py`

**Request body:**
```json
{
  "hardware_uuid": "AA:BB:CC:DD:EE:FF"
}
```

**Response `200`:**
```json
{ "received": true }
```

**Logic:**
1. Look up vault by `hardware_uuid` → `404` if not found
2. Call `LogActivity` use case:
   - `action`: `TAMPER_DETECTED`
   - `method`: `SYSTEM`
   - `user_id`: `None`
   - `metadata`: `{ "hardware_uuid": "..." }`
3. Return `{ "received": true }`

**Errors:**
- `404` — hardware_uuid not registered

> **Note:** `TAMPER_DETECTED` must be added to `ActivityAction` enum in `app/domain/models/access_log.py` if not already present.

---

## 6. Firmware WebSocket — Persistent Device Connection

### `WS /devices/ws?hardware_uuid={uuid}`

**Auth:** None (identified by hardware_uuid)
**Handler:** `app/websocket/vault_socket.py` — implement the existing placeholder `/ws/vault`

**Connection flow:**
1. Firmware connects with `?hardware_uuid=AA:BB:CC:DD:EE:FF`
2. Look up vault by `hardware_uuid` → close with `4004` if not found
3. Call `manager.connect_vault(websocket, vault_id)`
4. Enter receive loop

**Messages firmware → backend:**

| Type | Payload | Action |
|---|---|---|
| `heartbeat` | `{ "type": "heartbeat" }` | Update `vault.last_seen_at` |
| `state_update` | `{ "type": "state_update", "status": "LOCKED" }` | Call `ProcessVaultStateUpdate` |
| `ack` | `{ "type": "ack", "command": "remote_unlock" }` | Log acknowledgment |

**Messages backend → firmware (pushed by existing use cases):**

| Command | Payload | Triggered by |
|---|---|---|
| `remote_unlock` | `{ "command": "remote_unlock", ... }` | `SendUnlockCommand` use case |
| `buzzer_off` | `{ "command": "buzzer_off" }` | Future use case |
| `reset` | `{ "command": "reset" }` | `POST /vaults/{vault_id}/reset` |

**Disconnect:** call `manager.disconnect_vault(websocket, vault_id)` on any disconnect/error.

---

## Files to Create / Modify

| File | Action | Notes |
|---|---|---|
| `app/infrastructure/db/models/user_orm.py` | **Modify** | Add `provisioning_token: str \| None` column (nullable, unique) |
| `app/infrastructure/db/repositories/sqlalchemy_user_repository.py` | **Modify** | Add `get_by_provisioning_token`, `set_provisioning_token`, `clear_provisioning_token` |
| `app/application/ports/user_repository.py` | **Modify** | Add 3 new method signatures |
| `app/application/use_cases/generate_provisioning_token.py` | **Create** | New use case |
| `app/application/use_cases/register_device.py` | **Create** | New use case |
| `app/application/use_cases/reset_vault.py` | **Create** | New use case — wipe vault state, delete access logs |
| `app/api/v1/devices.py` | **Create** | New router: register, verify-pin, tamper |
| `app/api/v1/vaults.py` | **Modify** | Add `POST /vaults/provisioning-token` |
| `app/websocket/vault_socket.py` | **Modify** | Implement `/ws/vault` placeholder |
| `app/domain/models/access_log.py` | **Modify** | Add `TAMPER_DETECTED` to `ActivityAction` if missing |
| `app/main.py` (or router include) | **Modify** | Include `devices` router |
| Alembic migration | **Create** | Add `provisioning_token` column to `users` table |

---

## Implementation Order

1. DB migration — `provisioning_token` column on `users`
2. `UserORM` + repository methods
3. `GenerateProvisioningToken` use case
4. `RegisterDevice` use case
5. `ResetVault` use case
6. `POST /vaults/provisioning-token` endpoint
7. `POST /vaults/{vault_id}/reset` endpoint
8. `POST /devices/register` endpoint
9. `POST /devices/verify-pin` endpoint
10. `POST /devices/tamper` endpoint
11. `WS /devices/ws` — implement vault_socket.py placeholder
12. Wire `devices` router into `app/main.py`

---

## Open Questions

- [x] Alembic migrations are at `app/alembic/versions/` inside `smartvault-backend`
- [ ] Does `ActivityAction` enum already include `TAMPER_DETECTED`?
- [ ] Should `POST /devices/register` also return a `vault_secret` for future WebSocket auth hardening?
- [ ] Solenoid sensor GPIO pin (firmware side) — still TBD (placeholder GPIO34)

---

## Firmware Full Process Flow

```
                          ┌─────────────────────────────┐
                          │         POWER ON / BOOT      │
                          │  nvs_storage_init()          │
                          │  esp_netif_init()            │
                          │  nfc_init()                  │
                          └──────────────┬──────────────┘
                                         │
                          ┌──────────────▼──────────────┐
                          │   Is device provisioned?     │
                          │   nvs_storage_is_provisioned │
                          └──────┬───────────────┬───────┘
                                 │ NO             │ YES
                                 │                │
          ┌──────────────────────▼──┐    ┌────────▼────────────────────┐
          │   PROVISIONING MODE     │    │   NORMAL MODE BOOT          │
          │                         │    │                             │
          │  Wait for NFC tap       │    │  Load from NVS:             │
          │  nfc_start_listener()   │    │  - WiFi SSID + password     │
          └──────────┬──────────────┘    │  - API URL                  │
                     │                   │  - hardware_uuid            │
          ┌──────────▼──────────────┐    └────────┬────────────────────┘
          │   NFC card tapped       │             │
          │   on_nfc_card_tapped()  │    ┌────────▼────────────────────┐
          └──────────┬──────────────┘    │  wifi_manager_connect()     │
                     │                   │  api_client_init()          │
          ┌──────────▼──────────────┐    └────────┬────────────────────┘
          │  UID stored in NVS?     │             │
          └──┬──────────────────┬───┘             │
             │ NO               │ YES             │
             │ (first tap)      │ (owner re-tap)  │
             │                  │                 │
  ┌──────────▼──────┐  ┌────────▼──────────┐     │
  │ Save UID to NVS │  │ nvs_set_provisioned│     │
  └──────────┬──────┘  │ (false)            │     │
             │         └────────┬───────────┘     │
             └────────┬─────────┘                 │
                      │                           │
          ┌───────────▼─────────────────┐         │
          │   SoftAP starts             │         │
          │   provisioning_start()      │         │
          │   SSID: SmartVault-XXXXXX   │         │
          │   Captive portal: 192.168.4.1│        │
          └───────────┬─────────────────┘         │
                      │                           │
          ┌───────────▼─────────────────┐         │
          │  User connects phone to AP  │         │
          │  Opens captive portal       │         │
          │  Enters:                    │         │
          │  - WiFi SSID                │         │
          │  - WiFi password            │         │
          │  - Provisioning token       │         │
          │    (from mobile app)        │         │
          └───────────┬─────────────────┘         │
                      │                           │
          ┌───────────▼─────────────────┐         │
          │  on_provisioning_done()     │         │
          │  Save to NVS:               │         │
          │  - WiFi credentials         │         │
          │  - provisioning token       │         │
          │  - API URL (if provided)    │         │
          └───────────┬─────────────────┘         │
                      │                           │
          ┌───────────▼─────────────────┐         │
          │  wifi_manager_connect()     │         │
          │  Connect to home WiFi       │         │
          └───────────┬─────────────────┘         │
                      │                           │
          ┌───────────▼─────────────────┐         │
          │  api_client_register_device │         │
          │  POST /devices/register     │         │
          │  { hardware_uuid, token }   │         │
          └───────────┬─────────────────┘         │
                      │                           │
          ┌───────────▼─────────────────┐         │
          │  nvs_set_provisioned(true)  │         │
          │  Token cleared on backend   │         │
          └───────────┬─────────────────┘         │
                      │                           │
                      └──────────────┬────────────┘
                                     │
                          ┌──────────▼──────────────────┐
                          │       NORMAL MODE            │
                          │                              │
                          │  solenoid_init()             │
                          │  buzzer_init()               │
                          │  keypad_init()               │
                          │  pin_auth_init()             │
                          │  keypad_start()              │
                          │  tamper_init()               │
                          │  tamper_start()              │
                          │  ws_client_init()            │
                          │  ws_client_start()           │
                          └──────────┬───────────────────┘
                                     │
                    ┌────────────────┼────────────────────┐
                    │                │                     │
       ┌────────────▼──────┐  ┌──────▼──────────┐  ┌──────▼──────────────┐
       │  KEYPAD INPUT     │  │  TAMPER DETECT  │  │  WEBSOCKET          │
       │                   │  │                 │  │                     │
       │  User types PIN   │  │  Sensor GPIO34  │  │  Connected to       │
       │  on_pin_entered() │  │  (TBD) triggers │  │  WS /devices/ws     │
       └────────┬──────────┘  └──────┬──────────┘  └──────┬──────────────┘
                │                    │                     │
       ┌────────▼──────────┐  ┌──────▼──────────┐  ┌──────▼──────────────┐
       │  pin_auth_verify()│  │  POST /devices/ │  │  Receive commands   │
       │  POST /devices/   │  │  tamper         │  │  from backend       │
       │  verify-pin       │  │  { hardware_uuid│  └──────┬──────────────┘
       │  { hw_uuid, pin } │  │  }              │         │
       └────────┬──────────┘  └──────┬──────────┘         │
                │                    │            ┌────────┴──────────────┐
       ┌────────▼──────────┐  ┌──────▼──────────┐│                       │
       │  200 → accepted   │  │  200 → received ││  "remote_unlock"      │  "reset"
       │  solenoid_unlock()│  │  buzzer alarm   │└──────┬────────────────┘──────┐
       │  buzzer long beep │  │  continues      │       │                       │
       │                   │  │                 │┌──────▼────────────┐  ┌───────▼──────────┐
       │  401 → denied     │  └─────────────────┘│  solenoid_unlock()│  │  nvs_erase_all() │
       │  buzzer short beep│                     │  buzzer confirm   │  │  reboot →        │
       └───────────────────┘                     └───────────────────┘  │  PROVISIONING    │
                                                                         │  MODE            │
                                                                         └──────────────────┘
```

> **GPIO placeholders:** Solenoid sensor / tamper input is GPIO34 — update in `components/tamper/tamper.c` and `components/solenoid/include/solenoid.h` once wiring is finalized.
