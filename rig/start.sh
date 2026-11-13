#!/usr/bin/env bash
# Start a headless nested Hyprland. Prints its WAYLAND socket and instance signature.
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
before=$(ls /run/user/1000 | grep -E '^wayland-[0-9]+$' | sort)
env -u WAYLAND_DISPLAY -u HYPRLAND_INSTANCE_SIGNATURE -u HYPRLAND_CMD \
  HYPRLAND_HEADLESS_ONLY=1 AQ_BACKENDS=headless LIBSEAT_BACKEND=noop XDG_SESSION_TYPE=wayland \
  setsid Hyprland -c "$DIR/hyprland.conf" > "$DIR/hyprland.log" 2>&1 &
echo $! > "$DIR/hyprland.pid"
for i in $(seq 1 40); do
  sleep 0.5
  after=$(ls /run/user/1000 | grep -E '^wayland-[0-9]+$' | sort)
  new=$(comm -13 <(echo "$before") <(echo "$after") | head -1)
  [ -n "$new" ] && break
done
echo "socket=$new pid=$(cat "$DIR/hyprland.pid")"
