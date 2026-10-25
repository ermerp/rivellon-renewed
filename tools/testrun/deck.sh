#!/usr/bin/env bash
# deck.sh - Rivellon Renewed on the Steam Deck over SSH. Run from Git Bash on Windows.
#
#   tools/testrun/deck.sh <user@host> check
#       Read only: running div.exe, mod/dgVoodoo files, renderer (slashed.cfg), resolution
#       (config.div, slashed-d3d6.cfg), fog line, savegames, div.exe hash, earlier backups.
#   tools/testrun/deck.sh <user@host> install [--loglevel debug|info]
#       Backs up savegames and configs to divine_divinity/_backup_rr_<time>, then copies the ASI
#       loader as winmm.dll, RivellonRenewed.asi, RivellonRenewed/fonts and the repo default
#       RivellonRenewed.ini (an existing one is kept as RivellonRenewed.ini.previous) and writes the
#       launch options to ~/Desktop/rivellon-renewed-launch-options.txt. Refuses while div.exe runs
#       or when a winmm.dll exists that is not the ASI loader.
#   tools/testrun/deck.sh <user@host> fetch [<local dir>]
#       Copies the mod logs, the Proton log and the Steam screenshots made since the install to
#       <local dir> (default C:\dev\screenshots\deck\<time>).
#   tools/testrun/deck.sh <user@host> testsaves [<save> ...]
#       Copies test saves (default: all) from C:\dev\testsaves\de into the Deck's savegames;
#       a save whose name exists there already is skipped, never overwritten.
#   tools/testrun/deck.sh <user@host> start
#       Game Mode: starts the game through Steam and waits for the overlay's first frame.
#   tools/testrun/deck.sh <user@host> steps [--out <local dir>] <step> ...
#       Game Mode: drives the running game (input, guarded screenshots; steps in deck-steps.sh)
#       and copies the shots to <local dir> (default C:\dev\screenshots\deck\steps).
#   tools/testrun/deck.sh <user@host> uninstall
#       Removes winmm.dll (only if it is the loader), RivellonRenewed.asi, the fonts and all logs;
#       keeps RivellonRenewed.ini and the backups. The launch options must be cleared by hand.
#
# The host is never stored in the repo (CLAUDE.md). Build first: build/Release/RivellonRenewed.asi.

set -euo pipefail

usage() { sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
[ $# -ge 2 ] || usage
host=$1; action=$2; shift 2

repo=$(cd "$(dirname "$0")/../.." && pwd)
loader=/c/dev/downloads/ultimate-asi-loader_9.7.4/dinput8.dll
asi=$repo/build/Release/RivellonRenewed.asi
gamedir='.local/share/Steam/steamapps/common/divine_divinity'   # relative to the Deck user's home
ssh_opts=(-o BatchMode=yes -o ConnectTimeout=15)

remote() { ssh "${ssh_opts[@]}" "$host" 'bash -s' -- "$@"; }

case $action in
check)
    remote "$gamedir" <<'EOF'
set -u
cd ~/"$1" || { echo "No game folder ~/$1"; exit 1; }
echo "== div.exe running: $(pgrep -x div.exe >/dev/null && echo YES - close it first || echo no)"
echo "== div.exe sha256: $(sha256sum div.exe | cut -d' ' -f1)"
echo "== mod and dgVoodoo files:"
ls -la winmm.dll RivellonRenewed.asi RivellonRenewed.ini RivellonRenewed.log RivellonRenewed \
       dgVoodoo.conf DDraw.dll ddraw.dll D3DImm.dll 2>/dev/null || true
[ -f winmm.dll ] && echo "   winmm.dll sha256: $(sha256sum winmm.dll | cut -d' ' -f1)"
echo "== renderer (slashed.cfg, first DWORD: 0 = Direct3D 6 / slash1.dll): $(od -An -tu4 -N4 slashed.cfg 2>/dev/null || echo missing)"
echo "== config.div:"; grep -aiE '^(width|height|no fog|startup logo|smooth)' config.div || true
echo "== slashed-d3d6.cfg width/height (0x0C/0x10): $(od -An -tu4 -j12 -N8 slashed-d3d6.cfg 2>/dev/null || echo missing)"
echo "== config.lcl:"; grep -aiE 'flat files|quickstart|use8bitfont|startup logo' config.lcl || true
echo "== savegames: $(ls savegames 2>/dev/null | wc -l) entries, $(du -sh savegames 2>/dev/null | cut -f1)"
echo "== backups:"; ls -d _backup* _ddmod* 2>/dev/null || true
echo "== free space: $(df -h . | awk 'NR==2 {print $4}')"
EOF
    ;;

install)
    level=debug
    while [ $# -gt 0 ]; do
        case $1 in --loglevel) level=$2; shift 2 ;; *) usage ;; esac
    done
    for f in "$loader" "$asi" "$repo/config/RivellonRenewed.ini"; do
        [ -f "$f" ] || { echo "Missing: $f"; exit 1; }
    done
    loader_sha=$(sha256sum "$loader" | cut -d' ' -f1)
    stage=$(mktemp -d)
    trap 'rm -rf "$stage"' EXIT
    cp "$loader" "$stage/winmm.dll"
    cp "$asi" "$stage/RivellonRenewed.asi"
    mkdir -p "$stage/RivellonRenewed/fonts"
    cp "$repo"/assets/fonts/* "$stage/RivellonRenewed/fonts/"
    sed "s/^LogLevel=[^\r]*/LogLevel=$level/" "$repo/config/RivellonRenewed.ini" > "$stage/RivellonRenewed.ini"
    echo "Installing $(sha256sum "$asi" | cut -d' ' -f1 | head -c 16)... (RivellonRenewed.asi, LogLevel=$level)"
    # One connection: the remote script reads the staged files as a tar stream after its checks.
    { cat <<'EOF'
set -eu
cd ~/"$1"
loader_sha=$2
if pgrep -x div.exe >/dev/null; then echo "div.exe is running - close the game first"; exit 1; fi
if [ -f winmm.dll ] && [ "$(sha256sum winmm.dll | cut -d' ' -f1)" != "$loader_sha" ]; then
    echo "winmm.dll exists and is not the ASI loader - not replacing it"; exit 1
fi
backup=_backup_rr_$(date +%Y%m%d-%H%M%S)
mkdir "$backup"
cp -a savegames "$backup/" 2>/dev/null || true
for f in config.div config.lcl slashed.cfg slashed-d3d6.cfg init.cfg sinfo.000 RivellonRenewed.ini; do
    if [ -f "$f" ]; then cp -a "$f" "$backup/"; fi
done
echo "Backup: $PWD/$backup ($(ls "$backup/savegames" 2>/dev/null | wc -l) savegames)"
if [ -f RivellonRenewed.ini ]; then
    mv RivellonRenewed.ini RivellonRenewed.ini.previous
    echo "Old RivellonRenewed.ini kept as RivellonRenewed.ini.previous"
fi
head -c "$3" | tar -xf -
touch RivellonRenewed/.installed
mkdir -p ~/Desktop
printf '%s\n' 'WINEDLLOVERRIDES="winmm=n,b" PROTON_LOG=1 %command%' > ~/Desktop/rivellon-renewed-launch-options.txt
echo "Installed in $PWD:"; ls -la winmm.dll RivellonRenewed.asi RivellonRenewed.ini RivellonRenewed/fonts
echo "Launch options: ~/Desktop/rivellon-renewed-launch-options.txt"
EOF
    } > "$stage.sh"
    tar -C "$stage" -cf "$stage.tar" .
    size=$(stat -c %s "$stage.tar")
    cat "$stage.sh" "$stage.tar" | ssh "${ssh_opts[@]}" "$host" \
        "f=\$(mktemp); head -c $(stat -c %s "$stage.sh") > \$f; bash \$f '$gamedir' '$loader_sha' '$size'; r=\$?; rm -f \$f; exit \$r"
    rm -f "$stage.sh" "$stage.tar"
    ;;

fetch)
    dest=${1:-/c/dev/screenshots/deck/$(date +%Y%m%d-%H%M%S)}
    mkdir -p "$dest"
    remote "$gamedir" <<'EOF' | tar -C "$dest" -xvf -
set -u
cd ~
g=$1
marker=$g/RivellonRenewed/.installed
[ -f "$marker" ] || { echo "Mod not installed (no $marker)" >&2; exit 1; }
files=()
for f in "$g/RivellonRenewed.log" "$g/RivellonRenewed.prev.log" "$g/RivellonRenewed.ini"; do [ -f "$f" ] && files+=("$f"); done
[ -d "$g/RivellonRenewed/logs" ] && files+=("$g/RivellonRenewed/logs")
[ steam-214170.log -nt "$marker" ] && files+=(steam-214170.log)
while IFS= read -r f; do files+=("$f"); done < <(find .local/share/Steam/userdata/*/760/remote/214170/screenshots \
    -maxdepth 1 -type f -newer "$marker" 2>/dev/null)
tar -cf - "${files[@]}"
EOF
    echo "Fetched to $dest"
    ;;

testsaves)
    saves=/c/dev/testsaves/de
    [ $# -gt 0 ] || set -- $(ls "$saves")
    for s in "$@"; do [ -d "$saves/$s" ] || { echo "No test save $saves/$s"; exit 1; }; done
    # a fresh install has no savegames folder until the first save
    existing=$(ssh "${ssh_opts[@]}" "$host" "mkdir -p ~/'$gamedir'/savegames && ls ~/'$gamedir'/savegames")
    copy=()
    for s in "$@"; do
        if grep -qxF "$s" <<<"$existing"; then echo "Skipped $s (exists on the Deck)"; else copy+=("$s"); fi
    done
    [ ${#copy[@]} -gt 0 ] || exit 0
    tar -C "$saves" -cf - "${copy[@]}" | ssh "${ssh_opts[@]}" "$host" "tar -C ~/'$gamedir'/savegames -xf -"
    printf 'Copied %s\n' "${copy[@]}"
    ;;

start)
    remote "$gamedir" <<'EOF'
cd ~/"$1"
if pgrep -x div.exe >/dev/null; then echo "div.exe is already running"; exit 1; fi
log=RivellonRenewed.log
before=$(stat -c %Y "$log" 2>/dev/null || echo 0)
(steam -ifrunning steam://rungameid/214170 >/dev/null 2>&1 &)
for i in $(seq 1 120); do
    if pgrep -x div.exe >/dev/null && [ "$(stat -c %Y "$log" 2>/dev/null || echo 0)" != "$before" ] \
        && grep -q 'Overlay: first frame' "$log"; then
        echo "First frame after ~$((i / 2)) s; $(grep -cE '\[(WARN|ERROR)' "$log") warning(s)/error(s) in the log"; exit 0
    fi
    sleep 0.5
done
echo "No first frame within 60 s (div.exe: $(pgrep -x div.exe || echo not running))"; exit 1
EOF
    ;;

steps)
    out=/c/dev/screenshots/deck/steps
    if [ "${1:-}" = --out ]; then out=$2; shift 2; fi
    [ $# -gt 0 ] || usage
    ssh "${ssh_opts[@]}" "$host" 'rm -rf /tmp/rr-shots'
    ssh "${ssh_opts[@]}" "$host" 'bash -s' -- "$@" < "$repo/tools/testrun/deck-steps.sh"
    if ssh "${ssh_opts[@]}" "$host" 'ls /tmp/rr-shots/*.png' >/dev/null 2>&1; then
        mkdir -p "$out"
        scp -q "${ssh_opts[@]}" "$host:/tmp/rr-shots/*.png" "$out/"
        ssh "${ssh_opts[@]}" "$host" 'rm -rf /tmp/rr-shots'
        echo "Shots in $out"
    fi
    ;;

uninstall)
    remote "$gamedir" "$(sha256sum "$loader" | cut -d' ' -f1)" <<'EOF'
set -eu
cd ~/"$1"
if pgrep -x div.exe >/dev/null; then echo "div.exe is running - close the game first"; exit 1; fi
if [ -f winmm.dll ]; then
    if [ "$(sha256sum winmm.dll | cut -d' ' -f1)" = "$2" ]; then rm winmm.dll
    else echo "winmm.dll is not the ASI loader - left in place"; fi
fi
rm -rf RivellonRenewed.asi RivellonRenewed.log RivellonRenewed.prev.log RivellonRenewed/fonts \
       RivellonRenewed/logs RivellonRenewed/.installed
rmdir RivellonRenewed 2>/dev/null || true
echo "Rivellon Renewed removed from $PWD (RivellonRenewed.ini and _backup_rr_* kept) - clear the launch options in Steam"
EOF
    ;;

*) usage ;;
esac
