# DCRWNIIR
Door-close reminder when nobody is in room

# Server
## Current Structure

```
.
├── main.c                                 # Main program
├── fcm_token.h                            # Header for OAuth2 authentification
├── fcm_token.c                            # OAuth2 + JWT implementation
├── fcm_notification.h                     # Header for FCM notifications
├── fcm_notification.c                     # FCM notifications implementation
├── Makefile                               # Compilation file
└── door-close-reminder-79f888941429.json  # Service account JSON file
```

## Dependencies
```bash
sudo apt-get update
sudo apt-get install -y libcurl4-openssl-dev libjson-c-dev libssl-dev build-essential
```

# App
Automated Bluetooth L2CAP connection system that periodically scans for a specific device and sends a Firebase Cloud Messaging (FCM) token upon successful connection.

## Features
```
- Periodic Bluetooth device discovery (optimized for battery)
- Auto-pairing with a target device by name (DCRWNIIR)
- L2CAP socket connection (Android 10+)
- FCM token management (save, validate, refresh)
- Token is sent to the server over the Bluetooth channel
- Runtime permission handling for Bluetooth and notifications
```

## Current Structure
```
.
MainActivity
 ├── Initializes FCMManager & BluetoothAutoConnectService
 ├── Handles permissions & starts services
 └── Displays real-time status in UI

BluetoothAutoConnectService
 ├── Periodically scans for target Bluetooth device
 ├── Pairs if needed
 ├── Connects via L2CAP socket
 └── Sends FCM token over Bluetooth

TokenStorage
 ├── Caches token in SharedPreferences
 └── Handles token expiry (7 days)

FCMManager
 ├── Retrieves or refreshes token using Firebase
 └── Persists token using TokenStorage

FCMService
 ├── Handles onNewToken from Firebase
 └── Displays incoming FCM messages via system notifications
```

## Requirements
```
- Android 10+ (for L2CAP channel support)
- Bluetooth hardware
- Firebase project configured
- Internet access for FCM
- Target Bluetooth device listening on PSM 0x1003
```
