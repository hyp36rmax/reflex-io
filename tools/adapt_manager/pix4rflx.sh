#!/bin/bash
# Reflex Core Art. SPDX-License-Identifier: GPL-3.0-or-later
# PIX assets remain in venice1200/MiSTer_i2c2oled_Pictures; never source them.
set -uo pipefail
export LC_ALL=C

ART_DIR=/media/fat/i2c2oled
CACHE_DIR=/media/fat/reflex-core-art
MAIN_CACHE_DIR=/media/fat/mister_serial/art/ssd1306-128x64
CORE_FILE=/tmp/CORENAME
TTY=""
ONECOLOR=yes
INVERTHEADER=no
SERIAL_LOCK=/tmp/reflex_adapt_serial.lock
CONFIG=${PIX4RFLX_CONFIG:-"${0%.sh}.ini"}
args=("$@")
for ((i=0; i<${#args[@]}; i++)); do
    if [[ ${args[i]} == --config ]]; then
        CONFIG=${args[i+1]:?Missing --config path}
    fi
done
if [[ -f $CONFIG ]]; then source "$CONFIG"; fi

MODE=once
CORE=""
INPUT=""
OUTPUT=""
declare -a CACHE_ALIASES=()
usage() {
    cat <<'HELP'
Reflex Core Art — pix4rflx
  --once                     Display the current core (default).
  --daemon                   Watch core changes and adapter reconnects.
  --update                   Download upstream PIX artwork; never opens serial.
  --encode INPUT OUTPUT      Convert a PIX file to 1024 raw bytes; no serial.
  --build-cache [DIR]        Export Main SSD1306 cache (default: /media/fat/mister_serial/art/ssd1306-128x64); no serial.
  --alias OUTPUT=SOURCE      Export OUTPUT from the explicitly named SOURCE PIX art.
  --core NAME                Display this core instead of reading CORENAME.
  --tty PATH                 Control CDC port; otherwise identify it via sysfs.
  --config FILE              User settings (default: pix4rflx.ini beside script).
  --art-dir DIR              Existing i2c2oled Pix/PRI folders.
  --cache-dir DIR            Downloaded artwork (default: /media/fat/reflex-core-art).
  --core-file PATH           Core name (default: /tmp/CORENAME).
  --lock-file PATH           Shared serial lock; all cooperating writers use it.
  --original                 Prefer original art instead of one-color art.
  --invert-header            Invert first 16 rows of original art.
HELP
}
while (($#)); do
    case $1 in
        --once) MODE=once; shift ;;
        --daemon) MODE=daemon; shift ;;
        --update) MODE=update; shift ;;
        --encode) MODE=encode; INPUT=${2:?Missing PIX path}; OUTPUT=${3:?Missing output path}; shift 3 ;;
        --build-cache)
            MODE=build-cache
            if (($# > 1)) && [[ $2 != --* ]]; then MAIN_CACHE_DIR=$2; shift 2; else shift; fi
            ;;
        --alias) CACHE_ALIASES+=("${2:?Missing OUTPUT=SOURCE alias}"); shift 2 ;;
        --core) CORE=${2:?Missing core name}; shift 2 ;;
        --tty) TTY=${2:?Missing TTY path}; shift 2 ;;
        --config) shift 2 ;;
        --art-dir) ART_DIR=${2:?Missing art directory}; shift 2 ;;
        --cache-dir) CACHE_DIR=${2:?Missing cache directory}; shift 2 ;;
        --core-file) CORE_FILE=${2:?Missing core file}; shift 2 ;;
        --lock-file) SERIAL_LOCK=${2:?Missing lock path}; shift 2 ;;
        --original) ONECOLOR=no; shift ;;
        --invert-header) INVERTHEADER=yes; shift ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done
log() { printf 'Core Art: %s\n' "$*" >&2; }
need() { command -v "$1" >/dev/null 2>&1 || { log "Missing dependency: $1"; exit 1; }; }
for tool in awk xxd mktemp; do need "$tool"; done
WORK=$(mktemp -d "${TMPDIR:-/tmp}/pix4rflx.XXXXXX") || exit 1
cleanup() { rm -rf -- "$WORK"; }
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

encode() {
    local source=$1 destination=$2 invert=$3
    [[ -f $source && $(wc -c < "$source") -le 32768 ]] || return 1
    # Rows run left-to-right; bit 0 is the top pixel of each eight-row page.
    # Parsing quoted data (rather than evaluating logo=) also rejects truncated
    # or oversized rows without accepting a silently cropped partial image.
    awk -F '"' -v invert="$invert" '
        /^[[:space:]]*#/ { next }
        /"/ {
            if (NF != 3 || $2 !~ /^[01]+$/ || length($2) != 128) bad = 1
            rows[n++] = $2
        }
        END {
            if (bad || n != 64) exit 1
            for (p = 0; p < 8; p++) for (x = 1; x <= 128; x++) {
                value = 0
                weight = 1
                for (b = 0; b < 8; b++) {
                    bit = substr(rows[p * 8 + b], x, 1) + 0
                    if (invert == "yes" && p < 2) bit = 1 - bit
                    value += bit * weight
                    weight *= 2
                }
                printf "%02x", value
            }
            printf "\n"
        }
    ' "$source" > "$WORK/frame.hex" || return 1
    xxd -r -p "$WORK/frame.hex" > "$WORK/frame.bin" || return 1
    [[ $(wc -c < "$WORK/frame.bin") -eq 1024 ]] || return 1
    # Destination replacement happens only after complete validation.
    cp -- "$WORK/frame.bin" "$destination"
}

update_artwork() {
    for tool in curl tar flock; do need "$tool"; done
    mkdir -p -- "$CACHE_DIR" || return 1
    exec 7>"$CACHE_DIR/update.lock" || return 1
    flock -n 7 || { log 'Another artwork update is running'; return 1; }
    local repo=MiSTer_i2c2oled_Pictures file relative count=0 rejected=0 old="" previous=""
    [[ ! -r $CACHE_DIR/current.txt ]] || IFS= read -r old < "$CACHE_DIR/current.txt" || true
    [[ ! -r $CACHE_DIR/previous.txt ]] || IFS= read -r previous < "$CACHE_DIR/previous.txt" || true
    log 'Downloading Venice1200 PIX artwork'
    curl --fail --location --silent --show-error --connect-timeout 15 --max-time 120 \
        "https://codeload.github.com/venice1200/$repo/tar.gz/refs/heads/main" \
        -o "$WORK/artwork.tar.gz" || return 1
    tar -xzf "$WORK/artwork.tar.gz" -C "$WORK" \
        "$repo-main/Pictures/Pix" "$repo-main/Pictures/Pix_Onecolor" || return 1
    for file in "$WORK/$repo-main/Pictures/"{Pix,Pix_Onecolor}/*.pix; do
        [[ -f $file ]] || continue
        if ! encode "$file" "$WORK/validated.bin" no; then
            ((rejected+=1))
            relative=${file#"$WORK/$repo-main/Pictures/"}
            log "Skipping invalid upstream artwork: $relative"
            if [[ $old =~ ^artwork\.[A-Za-z0-9]+$ ]] && encode "$CACHE_DIR/$old/$relative" "$WORK/validated.bin" no; then
                cp -- "$CACHE_DIR/$old/$relative" "$file" || return 1
            else
                rm -f -- "$file" || return 1
                continue
            fi
        fi
        ((count+=1))
    done
    ((count > 0)) || { log 'Archive contains no valid PIX artwork; keeping installed cache'; return 1; }
    # FAT cannot create symlinks. Publish a tiny pointer file by rename after
    # the entire generation is installed, retaining the previous generation.
    local generation
    generation=$(mktemp -d "$CACHE_DIR/artwork.XXXXXX") || return 1
    cp -R -- "$WORK/$repo-main/Pictures/." "$generation/" || return 1
    printf '%s\n' "${generation##*/}" > "$CACHE_DIR/.current.$$" || return 1
    mv -f -- "$CACHE_DIR/.current.$$" "$CACHE_DIR/current.txt" || return 1
    printf '%s\n' "$old" > "$CACHE_DIR/.previous.$$" || return 1
    mv -f -- "$CACHE_DIR/.previous.$$" "$CACHE_DIR/previous.txt" || return 1
    # Only retire the recorded, helper-owned generation; never touch PRI art.
    if [[ $previous =~ ^artwork\.[A-Za-z0-9]+$ && $previous != "$old" && $previous != "${generation##*/}" ]]; then
        rm -rf -- "$CACHE_DIR/$previous" || return 1
    fi
    log "Updated $count PIX files in $generation ($rejected invalid upstream files skipped or retained from cache)"
}

valid_art_name() {
    [[ $1 =~ ^[A-Za-z0-9\ _+().-]+$ && $1 != . && $1 != .. ]]
}

cache_generation_sources() {
    local folder root cached="" generation="" source stem
    if [[ -r $CACHE_DIR/current.txt ]]; then
        IFS= read -r generation < "$CACHE_DIR/current.txt" || true
        [[ $generation =~ ^artwork\.[A-Za-z0-9]+$ ]] && cached="$CACHE_DIR/$generation"
    fi
    local folders=(PRI_Onecolor PRI Pix_Onecolor Pix)
    [[ $ONECOLOR == yes ]] || folders=(PRI Pix)
    for folder in "${folders[@]}"; do
        for root in "$ART_DIR" "$cached"; do
            [[ -d $root/$folder ]] || continue
            while IFS= read -r -d '' source; do
                stem=${source##*/}; stem=${stem%.pix}
                if ! valid_art_name "$stem"; then
                    log "Skipping artwork with unsafe cache name: ${source##*/}"
                else
                    # Keep every source in display priority order. A malformed
                    # local PRI must not hide a valid lower-priority PIX file.
                    CACHE_SOURCES["$stem"]="${CACHE_SOURCES[$stem]-}$source"$'\n'
                fi
            done < <(find "$root/$folder" -maxdepth 1 -type f -name '*.pix' -print0 | sort -z)
        done
    done
}

add_cache_target() {
    local target=$1 source=$2 lowered
    if ! valid_art_name "$target"; then
        log "Invalid cache target name: $target"
        return 1
    fi
    lowered=$(printf '%s' "$target" | tr '[:upper:]' '[:lower:]')
    if [[ -n ${CACHE_TARGETS[$target]+set} && ${CACHE_TARGETS[$target]} != "$source" ]]; then
        log "Conflicting artwork for cache target: $target"
        return 1
    fi
    if [[ -n ${CACHE_TARGETS[$lowered]+set} && ${CACHE_TARGETS[$lowered]} != "$source" ]]; then
        log "Conflicting artwork for lowercase cache target: $lowered"
        return 1
    fi
    CACHE_TARGETS["$target"]=$source
    CACHE_TARGETS["$lowered"]=$source
}

encode_cache_target() {
    local stem=$1 destination=$2 source invert
    while IFS= read -r source; do
        [[ -n $source ]] || continue
        invert=no
        [[ $INVERTHEADER != yes || $source == */*_Onecolor/* ]] || invert=yes
        if encode "$source" "$destination" "$invert"; then return 0; fi
        log "Skipping invalid artwork for cache target $stem: $source"
    done <<< "${CACHE_SOURCES[$stem]}"
    return 1
}

build_main_cache() {
    # Main owns the CDC writer. This path only reads PIX data and atomically
    # replaces completed 1024-byte files in Main's fixed artwork directory.
    [[ $ONECOLOR == yes || $ONECOLOR == no ]] || { log 'ONECOLOR must be yes or no'; return 2; }
    [[ $INVERTHEADER == yes || $INVERTHEADER == no ]] || { log 'INVERTHEADER must be yes or no'; return 2; }
    mkdir -p -- "$MAIN_CACHE_DIR" || return 1
    declare -gA CACHE_SOURCES=()
    declare -gA CACHE_TARGETS=()
    cache_generation_sources
    local name alias destination source staging staged count=0 rejected=0
    for name in "${!CACHE_SOURCES[@]}"; do
        add_cache_target "$name" "$name" || ((rejected+=1))
    done
    for alias in "${CACHE_ALIASES[@]}"; do
        if [[ $alias != *=* ]]; then
            log "Invalid alias (use OUTPUT=SOURCE): $alias"; ((rejected+=1)); continue
        fi
        destination=${alias%%=*}; source=${alias#*=}
        if [[ -z $destination || -z $source || -z ${CACHE_SOURCES[$source]+set} ]]; then
            log "Unknown alias source (aliases never guess arcade/MRA names): $alias"; ((rejected+=1)); continue
        fi
        add_cache_target "$destination" "$source" || ((rejected+=1))
    done
    ((${#CACHE_TARGETS[@]} > 0)) || { log 'No valid artwork candidates; keeping Main cache'; return 1; }
    staging=$(mktemp -d "$MAIN_CACHE_DIR/.pix4rflx.XXXXXX") || return 1
    for name in "${!CACHE_TARGETS[@]}"; do
        source=${CACHE_TARGETS[$name]}
        if encode_cache_target "$source" "$staging/$name.bin"; then
            ((count+=1))
        else
            rm -f -- "$staging/$name.bin"
            ((rejected+=1))
        fi
    done
    ((count > 0)) || { rm -rf -- "$staging"; log 'No valid artwork exported; keeping Main cache'; return 1; }
    # FAT has no durable directory rename semantics. Each completed file is
    # copied to a same-directory temporary then renamed; a failed generation
    # therefore leaves every old file or a complete new 1024-byte file.
    for staged in "$staging"/*.bin; do
        [[ -f $staged ]] || continue
        destination="$MAIN_CACHE_DIR/${staged##*/}"
        cp -- "$staged" "$destination.$$" || { rm -rf -- "$staging"; return 1; }
        mv -f -- "$destination.$$" "$destination" || { rm -f -- "$destination.$$"; rm -rf -- "$staging"; return 1; }
    done
    printf '%s\n' "$(date +%s) $count" > "$MAIN_CACHE_DIR/.pix4rflx-generation.$$" || { rm -rf -- "$staging"; return 1; }
    mv -f -- "$MAIN_CACHE_DIR/.pix4rflx-generation.$$" "$MAIN_CACHE_DIR/.pix4rflx-generation" || { rm -f -- "$MAIN_CACHE_DIR/.pix4rflx-generation.$$"; rm -rf -- "$staging"; return 1; }
    rm -rf -- "$staging"
    log "Exported $count Main artwork files ($rejected rejected; explicit aliases only)"
}

if [[ $MODE == encode ]]; then
    encode "$INPUT" "$OUTPUT" "$INVERTHEADER" || { log "Invalid PIX file: $INPUT"; exit 1; }
    exit 0
fi
if [[ $MODE == update ]]; then update_artwork; exit $?; fi
if [[ $MODE == build-cache ]]; then build_main_cache; exit $?; fi
for tool in flock stty timeout stat readlink; do need "$tool"; done
[[ $ONECOLOR == yes || $ONECOLOR == no ]] || { log 'ONECOLOR must be yes or no'; exit 2; }
[[ $INVERTHEADER == yes || $INVERTHEADER == no ]] || { log 'INVERTHEADER must be yes or no'; exit 2; }
exec 8>"$SERIAL_LOCK" || exit 1
flock -n 8 || { log 'Serial lock is busy; no data sent'; exit 1; }

find_tty() {
    if [[ -n $TTY ]]; then [[ -c $TTY ]] && printf '%s\n' "$TTY"; return; fi
    local entry path vendor control found="" matches=0
    for entry in /sys/class/tty/ttyACM*; do
        [[ -e $entry/device ]] || continue
        path=$(readlink -f "$entry/device")
        vendor=""; control=no
        while [[ $path != / && -n $path ]]; do
            if [[ -r $path/interface && $(<"$path/interface") == *"Reflex Adapt Control"* ]]; then control=yes; fi
            if [[ -r $path/idVendor ]]; then vendor=$(<"$path/idVendor"); break; fi
            path=${path%/*}
        done
        if [[ $control == yes && $vendor == 16d0 && -c /dev/${entry##*/} ]]; then
            found=/dev/${entry##*/}; ((matches+=1))
        fi
    done
    ((matches == 1)) && printf '%s\n' "$found"
}

port_in_use() {
    local descriptor
    # Main mister_serial does not currently honor our flock. Refuse an already
    # open port as well; never interrupt another writer to take ownership.
    for descriptor in /proc/[0-9]*/fd/*; do
        [[ $descriptor == /proc/$$/fd/* || $descriptor == /proc/$BASHPID/fd/* ]] && continue
        [[ $descriptor -ef $1 ]] && return 0
    done
    return 1
}

pick_art() {
    local folder root cached="" generation=""
    if [[ -r $CACHE_DIR/current.txt ]]; then
        IFS= read -r generation < "$CACHE_DIR/current.txt" || true
        [[ $generation =~ ^artwork\.[A-Za-z0-9]+$ ]] && cached="$CACHE_DIR/$generation"
    fi
    local folders=(PRI_Onecolor PRI Pix_Onecolor Pix)
    [[ $ONECOLOR == yes ]] || folders=(PRI Pix)
    for folder in "${folders[@]}"; do
        for root in "$ART_DIR" "$cached"; do
            [[ -n $root ]] || continue
            if [[ -f $root/$folder/$1.pix ]]; then
                printf '%s\n' "$root/$folder/$1.pix"
                return 0
            fi
        done
    done
    return 1
}

send_frame() {
    local tty=$1 name=$2 frame=$3 command="CMDCOR,$2,0"
    [[ -c $tty ]] || return 1
    if port_in_use "$tty"; then log "$tty is already open; stop its other serial sender first"; return 1; fi
    [[ -z $frame ]] || command="CMDPIX,$name"
    # Bound a stalled/disconnected write, and wait for the receiver's command
    # ACK before binary data. The firmware's initial frame timeout is 500 ms.
    timeout 2 bash -c '
        exec 3<> "$1" || exit 1
        stty -F "$1" 115200 raw -echo -hupcl -crtscts clocal min 0 time 1 || exit 1
        printf "%s\n" "$2" >&3 || exit 1
        IFS= read -r -t 0.2 -d ";" reply <&3 || exit 1
        [[ $reply == *ttyack ]] || exit 1
        [[ -z $3 ]] || cat -- "$3" >&3
    ' pix4rflx-send "$tty" "$command" "$frame"
}

last=""
last_error=""
while :; do
    name=$CORE
    if [[ -z $name ]]; then
        if [[ -r $CORE_FILE ]]; then IFS= read -r name < "$CORE_FILE" || true; else name=Menu; fi
    fi
    name=${name//$'\r'/}
    # Core names are also filenames and command fields. Preserve printable core
    # labels while removing separators, path traversal and command line breaks.
    name=$(printf '%s' "$name" | tr -cd 'A-Za-z0-9 _+().-')
    if [[ -z $name || $name == . || $name == .. ]]; then
        [[ $MODE == daemon ]] || { log 'Empty/invalid core name'; exit 1; }
        sleep 1; continue
    fi
    tty=$(find_tty) || tty=""
    if [[ -z $tty ]]; then
        last=""
        if [[ $last_error != no-port ]]; then log 'Waiting for a single Adapt control CDC port (use --tty if needed)'; last_error=no-port; fi
        [[ $MODE == daemon ]] || exit 1
        sleep 1; continue
    fi
    artwork=$(pick_art "$name") || artwork=""
    signature="$name|$tty|$(stat -Lc '%i:%Z:%t:%T' "$tty")|$artwork"
    [[ -z $artwork ]] || signature+="|$(stat -Lc '%i:%s:%Y' "$artwork")"
    if [[ $signature != "$last" ]]; then
        frame=""
        invert=no
        [[ $INVERTHEADER != yes || $artwork == */*_Onecolor/* ]] || invert=yes
        if [[ -n $artwork ]]; then
            if encode "$artwork" "$WORK/current.bin" "$invert"; then frame=$WORK/current.bin
            else log "Invalid artwork for $name; using text"; fi
        fi
        if send_frame "$tty" "${name:0:31}" "$frame" 2>"$WORK/send.err"; then
            last=$signature; last_error=""
            kind=text; [[ -z $frame ]] || kind=PIX
            log "Displayed $name ($kind)"
        else
            if [[ $last_error != "$signature" ]]; then
                cat "$WORK/send.err" >&2
                log "Send failed for $name; will retry in daemon mode"
            fi
            last_error=$signature
            [[ $MODE == daemon ]] || exit 1
        fi
    fi
    [[ $MODE == daemon ]] || exit 0
    sleep 1
done
