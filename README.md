# DCRWNIIR
Door-close reminder when nobody is in room

NEEDED :
For fcm_oauth.c :
gcc -Wall -Wextra -std=c99 -g -o fcm_oauth fcm_oauth.c -lcurl -ljson-c -lssl -lcrypto
Compilation needs these options, remember it.

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
