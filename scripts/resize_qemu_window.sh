#!/bin/sh

if [ "$(uname -s)" != "Darwin" ] || ! command -v osascript >/dev/null 2>&1; then
    exit 0
fi

width=${QEMU_WINDOW_WIDTH:-1200}
height=${QEMU_WINDOW_HEIGHT:-800}
attempt=0

while [ "$attempt" -lt 50 ]; do
    if osascript -e "tell application \"System Events\" to tell process \"qemu-system-x86_64\" to set size of window 1 to {$width, $height}" >/dev/null 2>&1; then
        exit 0
    fi

    attempt=$((attempt + 1))
    sleep 0.1
done

echo "WARNING: Could not resize QEMU. Allow Terminal or osascript under System Settings > Privacy & Security > Accessibility." >&2
exit 0