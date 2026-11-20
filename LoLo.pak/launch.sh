#!/bin/sh
APP_DIR="$(dirname "$0")"
APP_NAME="LoLo"
LOG_DIR="${LOGS_PATH:-/tmp}"
LOG_FILE="$LOG_DIR/$APP_NAME.log"

cd "$APP_DIR" || exit 1
killall minui-presenter >/dev/null 2>&1 || true
chmod +x launch.sh bin/* 2>/dev/null

log() { echo "[$(date '+%H:%M:%S')] $1" | tee -a "$LOG_FILE"; }
log "Starting $APP_NAME"

ensure_wifi() {
    if ! ip addr show wlan0 2>/dev/null | grep -q "inet "; then
        log "WiFi down, reconnecting..."
        rfkill.elf unblock wifi 2>/dev/null
        sleep 0.5
        /etc/init.d/wpa_supplicant start 2>/dev/null
        wpa_cli -i wlan0 reconnect 2>/dev/null
        for i in 1 2 3 4 5; do
            if ip addr show wlan0 2>/dev/null | grep -q "inet "; then log "WiFi reconnected"; break; fi
            sleep 1
        done
    else
        log "WiFi already connected"
    fi
}
ensure_wifi

export LD_LIBRARY_PATH="/usr/trimui/lib:$APP_DIR:$LD_LIBRARY_PATH"
./bin/lolo 2>&1 | while IFS= read -r line; do echo "[$(date '+%H:%M:%S')] $line" >> "$LOG_FILE"; done