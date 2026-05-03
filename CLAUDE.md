# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

StackChan is an open-source AI desktop robot project by M5Stack, comprising four main components:

1. **App** (`app/`) - Flutter mobile app (iOS/Android) for remote control, AI conversation, dance choreography, and camera integration
2. **Server** (`server/`) - GoFrame-based Go backend providing RESTful APIs for device management, user auth, posts, dances, and XiaoZhi AI integration
3. **Firmware** (`firmware/`) - ESP-IDF firmware (C/C++) for the StackChan robot running on ESP32-S3 (CoreS3)
4. **Remote** (`remote/`) - ESP-IDF firmware for a separate ESP32 remote control device using ESP-NOW protocol

## Key Commands

### App (Flutter/Dart)
```bash
cd app
flutter pub get          # Install dependencies
flutter analyze          # Lint/static analysis
flutter test             # Run tests
flutter run              # Run on connected device
flutter build apk --release   # Build release APK
flutter build ios --release   # Build release iOS
```

### Server (Go/GoFrame)
```bash
cd server
go run main.go                      # Run in development mode
go build -o stackchan-server main.go # Build binary
make build                           # Build via Makefile
make run                             # Run via Makefile
```

Server architecture: GoFrame v2 with MySQL. Layered architecture: `api/` (request/response structs) -> `internal/controller/` (HTTP handlers) -> `internal/logic/` (business logic) -> `internal/dao/` (data access) -> MySQL. Config in `manifest/config/config.yaml`. Database init: `check_list/create_mysql_database.sql`.

### Firmware (ESP-IDF)
```bash
cd firmware
python3 ./fetch_repos.py   # Fetch dependencies
idf.py build               # Build firmware binary
idf.py flash               # Flash to device
```

Requires ESP-IDF v5.5.4.

### Remote (ESP-IDF)
```bash
cd remote/code
idf.py build
idf.py flash
```

## Server API Modules

The server exposes these API modules, each under `/api/<module>/`:

| Module | Path Prefix | Purpose |
|--------|-------------|---------|
| device | `/api/device/*` | Device binding, unbinding, info updates |
| user | `/api/user/*` | Login, registration, JWT auth |
| dance | `/api/dance/*` | Dance creation and motion data management |
| post | `/api/post/*` | Posts with text, images, comments |
| admin | `/api/admin/*` | Admin panel for app/user management |
| appstore | `/api/appstore/*` | App store management |
| friend | `/api/friend/*` | Friend/contact system |
| pano | `/api/pano/*` | Panorama photo management |
| xiaozhi | `/api/xiaozhi/*` | XiaoZhi AI agent integration (license, token, config) |
| stackchandevice | `/api/stackchandevice/*` | StackChan-specific device user info |

API versioning uses `v1/` and `v2/` subdirectories. Controllers follow `*_v<N>_action.go` naming (e.g., `device_v2_bind_device.go`).

## App Architecture

- `lib/main.dart` - Entry point
- `lib/app_state.dart` - Global state management (using `get` package)
- `lib/model/` - Data models (XiaoZhi AI, dance, device info, etc.)
- `lib/network/` - HTTP client (dio), WebSocket utility, URL config
- `lib/util/` - RSA encryption, Bluetooth utils, music/audio processing
- `lib/view/` - UI screens organized by feature (home, popup)
- Server URL configured in `lib/network/urls.dart`, RSA keys in `lib/util/value_constant.dart`

## Firmware Architecture

- `firmware/main/` - ESP-IDF project root
- `firmware/main/apps/` - App modules: `app_ai_agent`, `app_app_center`, `app_avatar`, `app_dance`, `app_espnow_ctrl`, `app_ezdata`, `app_launcher`, `app_setup`
- `firmware/main/hal/` - Hardware abstraction layer for CoreS3 board
- Uses LVGL for UI rendering, ESP-NOW for wireless remote control
- Assets in `firmware/main/assets/` (icons, fonts, sounds, 3D model)

## Architecture Notes

- Server uses GoFrame's code generation (`gf gen`) - generated DAO/model code lives in `internal/dao/` and `internal/model/`
- Both firmware and remote are ESP-IDF projects (CMake-based) requiring separate build toolchains
- App requires backend server for full functionality (device management, AI, social features)
- BLE connects the app to the robot; ESP-NOW connects the remote controller to the robot
- No Go tests exist in the server. No Cursor/Copilot rules are present.
