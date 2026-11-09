# Source me. Everything targets the nested headless Hyprland only.
O=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956/.local/onscreen
export HYPRLAND_INSTANCE_SIGNATURE=$(cat $O/nested/sig)
export WAYLAND_DISPLAY=wayland-2
BIN=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956/Builds/20261109T225039957106Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
PTR=$O/ptr/ptr2
SHOTS=$O/shots; mkdir -p $SHOTS
# launch NAME W H [extra args...]; fresh save folder unless SAVEDIR is exported
launch() { local n=$1 w=$2 h=$3; shift 3
  hyprctl keyword monitor HEADLESS-1,${w}x${h}@60,0x0,1 >/dev/null; sleep 1
  mkdir -p $O/$n/runtime ${SAVEDIR:-$O/$n/saves}
  echo "$w $h" > $O/dims
  setsid "$BIN" -windowed -ResX=$w -ResY=$h -ForceRes -corelimit=2 -UserDir=$O/$n/runtime -ExpeditionSaveRoot=${SAVEDIR:-$O/$n/saves} -abslog=$O/$n/game.log -stdout -FullStdOutLogOutput "$@" > $O/$n/stdout.log 2>&1 &
  sleep 4; pgrep -f "ExpeditionSaveRoot=${SAVEDIR:-$O/$n/saves}" | tail -1 > $O/$n/pid; echo launched $n pid $(cat $O/$n/pid) ${w}x${h}; }
gpid() { cat $O/$1/pid; }
focused() { hyprctl activewindow -j | python3 -c "import json,sys;d=json.load(sys.stdin);print(d.get('pid'),d.get('class'),d.get('size'))"; }
shot() { sleep ${2:-0.6}; hyprctl dismissnotify >/dev/null 2>&1; grim "$SHOTS/$1.png" && echo "shot $1"; }
key() { wtype -s 250 "$@"; }
hold() { # hold MS key... : press keys, wait, release (real key presses)
  local ms=$1; shift; local a=(); local r=(); for k in "$@"; do a+=(-P $k); r=(-p $k "${r[@]}"); done; wtype -s 250 "${a[@]}" -s $ms "${r[@]}"; }
ptr() { read RW RH < $O/dims; $PTR $RW $RH s 1400 "$@"; }
# hshot NAME x y : hover (x,y) and take a screenshot while the virtual pointer is still alive
hshot() { local n=$1 x=$2 y=$3; shift 3; ptr m $((x-6)) $((y-4)) s 150 m $x $y s 900 sh "hyprctl dismissnotify >/dev/null 2>&1; grim $SHOTS/$n.png" "$@"; echo "hshot $n"; }
closegame () 
{ 
    local pid=$(cat $O/$1/pid);
    local a=$(hyprctl clients -j | python3 -c "import json,sys;p=int('$pid');print(([c['address'] for c in json.load(sys.stdin) if c['pid']==p] or [''])[0])");
    hyprctl dispatch closewindow address:$a;
    sleep 8;
    ps -p $pid > /dev/null && echo STILL-RUNNING || echo closed;
    grep -c "Error:" $O/$1/game.log
}
