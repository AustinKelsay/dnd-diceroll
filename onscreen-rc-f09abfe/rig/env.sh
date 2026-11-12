export R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-2052f717-f2729e40d44e35c5
export O=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-2052f717-f2729e40d44e35c5/.local/rc/rig
export HYPRLAND_INSTANCE_SIGNATURE=$(cat $O/sig)
export WAYLAND_DISPLAY=$(cat $O/sock)
unset DISPLAY; export SDL_VIDEODRIVER=wayland XDG_SESSION_TYPE=wayland
