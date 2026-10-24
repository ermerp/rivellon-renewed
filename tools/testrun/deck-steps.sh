# deck-steps.sh - runs on the Steam Deck (sent by deck.sh steps through ssh 'bash -s'); not run locally.
# Drives the running game with xdotool on its Xwayland display and takes screenshots with gamescopectl.
# Game Mode only (gamescope). Steps, given as arguments:
#   wait <s> | key <name>[*<n>] (xdotool key names, e.g. Return, Up*9, ctrl+shift+F2) | type <text>
#   hold <key> <ms> | mouse <fx> <fy> | click <fx> <fy> | rclick <fx> <fy> | dclick <fx> <fy>
#   press <fx> <fy> <ms> (left button held) | shot <name> (to /tmp/rr-shots/<name>.png)
# Coordinates are fractions of the game window. A shot is taken only while gamescope reports the
# game (app 214170) as the focused app - never the Steam UI (CLAUDE.md rule 11).
set -u
pid=$(pgrep -x div.exe | head -1)
[ -n "$pid" ] || { echo "div.exe is not running"; exit 1; }
gamedisplay=$(tr '\0' '\n' < /proc/$pid/environ | sed -n 's/^DISPLAY=//p')
XDO="env DISPLAY=$gamedisplay xdotool"
win=$($XDO search --name '^Divinity$' | head -1)
[ -n "$win" ] || { echo "no game window on $gamedisplay"; exit 1; }
eval "$($XDO getwindowgeometry --shell "$win" | grep -E '^(WIDTH|HEIGHT)=')"
pos() { awk -v x="$1" -v y="$2" -v w="$WIDTH" -v h="$HEIGHT" 'BEGIN { printf "%d %d", x * w, y * h }'; }
sec() { awk -v m="$1" 'BEGIN { print m / 1000 }'; }
mkdir -p /tmp/rr-shots
while [ $# -gt 0 ]; do
    s=$1; shift
    case $s in
        wait) sleep "$1"; shift ;;
        key) k=${1%\**}; n=${1#*\*}; [ "$n" = "$1" ] && n=1
             $XDO key --window "$win" --repeat "$n" --delay 150 "$k"; shift ;;
        type) $XDO type --window "$win" --delay 120 "$1"; shift ;;
        hold) $XDO keydown --window "$win" "$1" sleep "$(sec "$2")" keyup --window "$win" "$1"; shift 2 ;;
        mouse) $XDO mousemove --window "$win" $(pos "$1" "$2"); shift 2 ;;
        # The game misses a click whose button goes up at once - hold it briefly.
        click) $XDO mousemove --window "$win" $(pos "$1" "$2") sleep 0.2 mousedown 1 sleep 0.12 mouseup 1; shift 2 ;;
        rclick) $XDO mousemove --window "$win" $(pos "$1" "$2") sleep 0.2 mousedown 3 sleep 0.12 mouseup 3; shift 2 ;;
        dclick) $XDO mousemove --window "$win" $(pos "$1" "$2") sleep 0.2 click --repeat 2 --delay 120 1; shift 2 ;;
        press) $XDO mousemove --window "$win" $(pos "$1" "$2") sleep 0.2 mousedown 1 sleep "$(sec "$3")" mouseup 1; shift 3 ;;
        shot)
            app=$(DISPLAY=:0 xprop -root GAMESCOPE_FOCUSED_APP 2>/dev/null | awk '{ print $3 }')
            f=/tmp/rr-shots/$1.png; rm -f "$f"
            if pgrep -x div.exe >/dev/null && [ "$app" = 214170 ]; then
                GAMESCOPE_WAYLAND_DISPLAY=gamescope-0 gamescopectl screenshot "$f" >/dev/null 2>&1
                for i in 1 2 3 4 5 6; do [ -s "$f" ] && break; sleep 0.5; done
                echo "shot $1: $(stat -c %s "$f" 2>/dev/null || echo FAILED)"
            else
                echo "shot $1: skipped (focused app ${app:-none})"
            fi
            shift ;;
        *) echo "unknown step $s"; exit 1 ;;
    esac
done
