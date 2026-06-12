#!/bin/bash
# pipewire-avb is a CLIENT of a running PipeWire core, so make sure a core is up
# AND its socket is ready before starting the Milan-AVB daemon (module-avb is
# mandatory and the daemon exits if the core isn't reachable yet).
if [ -z "$AVB_INTERFACE" ]; then
    echo "AVB_INTERFACE is not set"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

# Realtime scheduling comes from RTKit, which polkit must authorize (or from
# RLIMIT_RTPRIO when configured). 
if [ "${MILAN_RT_CHECKED:-0}" != 1 ] && [ -x "$SCRIPT_DIR/check-polkit.sh" ]; then
    "$SCRIPT_DIR/check-polkit.sh" || \
        echo "WARNING: continuing without verified realtime privileges - AVB timing will be unreliable"
fi

# headless nodes may have no logind session -> create the runtime dir
if [ ! -d "$RUNTIME" ]; then
    sudo mkdir -p "$RUNTIME" && sudo chown "$(id -u):$(id -g)" "$RUNTIME" && sudo chmod 700 "$RUNTIME"
fi

# (re)start the core PipeWire the AVB daemon attaches to
systemctl --user restart pipewire.service 2>/dev/null
# wait for the core socket; if it never appears, start a plain core and wait
for i in $(seq 1 8); do [ -S "$RUNTIME/pipewire-0" ] && break; sleep 1; done
if [ ! -S "$RUNTIME/pipewire-0" ]; then
    setsid -f /usr/bin/pipewire >/tmp/pipewire-core.log 2>&1
    for i in $(seq 1 15); do [ -S "$RUNTIME/pipewire-0" ] && break; sleep 1; done
fi

# the core is up: prove it actually got realtime data threads (RTKit grants
# SCHED_RR, the RLIMIT_RTPRIO path SCHED_FIFO - accept both)
CORE_RT=0
for PID in $(pgrep -u "$(id -u)" -x pipewire); do
    CORE_RT=$((CORE_RT + $(ps -L -o cls= -p "$PID" 2>/dev/null | grep -c -E 'FF|RR')))
done
if [ "$CORE_RT" -gt 0 ]; then
    echo "RT-CHECK: pipewire core has $CORE_RT realtime thread(s)"
else
    echo "RT-CHECK: WARNING - pipewire core has NO realtime threads, run ./check-polkit.sh"
fi

# module-avb reads gPTP state from ptp4l's read-only management socket; make
# sure gPTP is up and the socket is connectable before the daemon needs it
MGMT_SOCK=$(sed -n 's/.*ptp\.management-socket *= *"\([^"]*\)".*/\1/p' \
        "$HOME/.config/pipewire/pipewire-avb.conf" 2>/dev/null | head -n 1)
MGMT_SOCK="${MGMT_SOCK:-/var/run/ptp4lro}"
if ! systemctl -q is-active milan-ptp4l 2>/dev/null && ! pgrep -x ptp4l >/dev/null; then
    echo "WARNING: ptp4l is not running - start it with ./ptp-start.sh"
elif [ ! -S "$MGMT_SOCK" ]; then
    echo "WARNING: $MGMT_SOCK missing - module-avb cannot read gPTP state (restart ./ptp-start.sh)"
elif [ ! -w "$MGMT_SOCK" ]; then
    echo "WARNING: $MGMT_SOCK not connectable by $(id -un) - restart ./ptp-start.sh (sets uds_ro_file_mode 0666)"
else
    echo "gPTP management socket $MGMT_SOCK is ready"
fi

# clear any stale gPTP management socket from a previous run
rm -f /tmp/pipewire-avb-gptp-* 2>/dev/null

# from a detached subshell; results on stderr and in
# /tmp/pipewire-avb-rt-check.log
(
    sleep 5
    PID=$(pgrep -n -u "$(id -u)" -x pipewire-avb)
    if [ -z "$PID" ]; then
        MSG="RT-CHECK: pipewire-avb not running after 5s"
    else
        N=$(ps -L -o cls= -p "$PID" 2>/dev/null | grep -c -E 'FF|RR')
        if [ "$N" -gt 0 ]; then
            MSG="RT-CHECK: pipewire-avb (pid $PID) has $N realtime thread(s)"
        else
            MSG="RT-CHECK: WARNING - pipewire-avb (pid $PID) has NO realtime threads, run ./check-polkit.sh"
        fi
    fi
    echo "$MSG" | tee /tmp/pipewire-avb-rt-check.log >&2
) &

# start the Milan-AVB daemon with verbose logging and the selected interface
exec /usr/bin/pipewire-avb -v
