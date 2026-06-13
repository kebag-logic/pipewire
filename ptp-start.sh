#!/bin/bash

# SPDX-FileCopyrightText: Copyright © 2022 Kebag-Logic
# SPDX-FileCopyrightText: Copyright © 2025 Alexandre Malki <alexandre.malki@kebag-logic.com>
# SPDX-FileCopyrightText: Copyright © 2025 Simon Gapp <simon.gapp@kebag-logic.com>
# SPDX-License-Identifier: MIT

# Part of the code taken from https://tsn.readthedocs.io/
#
# Start gPTP (ptp4l) so that nothing can disturb it:
#   - transient systemd *system* unit "milan-ptp4l": no controlling terminal,
#     so a closing shell, ssh disconnect or Ctrl-C cannot kill it, and logind
#     session cleanup never touches it
#   - every ptp4l thread runs SCHED_FIFO at RT_PRIO (default 95, above
#     PipeWire's data threads at rt.prio 83 and RTKit's RR grants): no other
#     userspace can preempt it off the CPU
#   - OOMScoreAdjust=-1000: the OOM killer always picks something else
#   - Restart=on-failure: a crash (or an igb reload yanking the PHC away, see
#     prepare-traffic-shaper.sh) self-heals within ~1 s
#   - optional: PTP_CPU=<n> pins ptp4l to one CPU, away from busy cores
#
# The pre-flight stops everything that would fight ptp4l for the PHC or the
# NIC hardware-timestamping config: stray ptp4l/phc2sys/timemaster processes
# and units, chronyd with hwtimestamp on this interface. NTP keeps owning
# CLOCK_REALTIME; ptp4l owns the NIC PHC. phc2sys would fight NTP and must
# not run.
#
# Usage:
#   ptp-start.sh [<network-interface>]    start (arg or $AVB_INTERFACE)
#   ptp-start.sh stop                     stop the supervised unit
#   ptp-start.sh status                   unit, per-thread RT prio, sockets
# Logs: journalctl -fu milan-ptp4l

UNIT="milan-ptp4l"
RT_PRIO="${RT_PRIO:-95}"                # rtprio for ptp4l (above PipeWire's 83)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GPTP_CFG="${GPTP_CFG:-$SCRIPT_DIR/gPTP.cfg}"
# management sockets ptp4l serves (gPTP.cfg makes the ro one world-connectable
# because module-avb runs as a regular user and reads gPTP state through it)
UDS_RW="/var/run/ptp4l"
UDS_RO="/var/run/ptp4lro"

# print scheduling class and rtprio of every thread of a pid
show_threads() {
    ps -L -o tid=,cls=,rtprio=,comm= -p "$1" 2>/dev/null | sed 's/^/    /'
}

case "${1:-}" in
stop)
    sudo systemctl stop "$UNIT" 2>/dev/null
    sudo systemctl reset-failed "$UNIT" 2>/dev/null
    echo "$UNIT stopped"
    exit 0
    ;;
status)
    systemctl status "$UNIT" --no-pager -l 2>/dev/null
    PID=$(systemctl show -p MainPID --value "$UNIT" 2>/dev/null)
    if [ -n "$PID" ] && [ "$PID" != 0 ]; then
        echo "threads (tid, class, rtprio, name):"
        show_threads "$PID"
        echo "oom_score_adj: $(cat /proc/"$PID"/oom_score_adj 2>/dev/null)"
        ls -l "$UDS_RW" "$UDS_RO" 2>/dev/null
    fi
    exit 0
    ;;
esac

# Use AVB_INTERFACE from environment if no argument is passed
IFACE="${1:-$AVB_INTERFACE}"

if [ -z "$IFACE" ]; then
    echo "Usage: $0 [<network-interface>] | stop | status"
    echo "Selected interface: (none)"
    echo "If no interface is printed, make sure that AVB_INTERFACE is set in your environment or .bashrc"
    exit 1
fi

echo "Selected interface: $IFACE"

PTP4L_BIN="$(command -v ptp4l)"
if [ -z "$PTP4L_BIN" ]; then
    echo "ERROR: ptp4l not found in PATH (install LinuxPTP, see doc/INSTALL.md)"
    exit 1
fi
GPTP_CFG="$(readlink -f "$GPTP_CFG")"
if [ ! -r "$GPTP_CFG" ]; then
    echo "ERROR: gPTP config not found: $GPTP_CFG"
    exit 1
fi

# Make SELinux permissive for ptp4l
sudo semanage permissive -a ptp4l_t

# ------------------------------------------------------------------ pre-flight
# The interface must expose a PHC; this also gives the /dev/ptpN guarded below.
# Older ethtool prints "PTP Hardware Clock: N", newer ones
# "Hardware timestamp provider index: N".
PHC_IDX=$(ethtool -T "$IFACE" 2>/dev/null \
        | awk '/PTP Hardware Clock:|Hardware timestamp provider index:/ {print $NF; exit}')
if [ -z "$PHC_IDX" ] || [ "$PHC_IDX" = "none" ]; then
    echo "ERROR: $IFACE has no PTP hardware clock (check: ethtool -T $IFACE)"
    exit 1
fi

# Stop competing linuxptp system units: a second ptp4l fights over the PHC,
# phc2sys drags the PHC/CLOCK_REALTIME against NTP, timemaster runs both.
ACTIVE_UNITS=$(systemctl list-units --type=service --state=running --no-legend \
        'ptp4l*' 'phc2sys*' 'timemaster*' 2>/dev/null | awk '{print $1}')
for U in $ACTIVE_UNITS; do
    echo "Stopping conflicting unit: $U"
    sudo systemctl stop "$U"
done

# chronyd configured for hardware timestamping reprograms the NIC timestamping
# filters underneath ptp4l -> broken RX timestamps. Refuse to continue.
if systemctl -q is-active chronyd 2>/dev/null; then
    for F in /etc/chrony.conf /etc/chrony/chrony.conf; do
        [ -r "$F" ] || continue
        if grep -Eq "^[[:space:]]*hwtimestamp[[:space:]]+(\*|$IFACE)" "$F"; then
            echo "ERROR: chronyd uses 'hwtimestamp' on $IFACE (in $F) and would fight ptp4l"
            echo "Remove that line (plain NTP on CLOCK_REALTIME is fine) and retry,"
            echo "or set MILAN_PTP_FORCE=1 to override"
            [ "${MILAN_PTP_FORCE:-0}" = 1 ] || exit 1
        fi
    done
fi

# Stop our own previous instance first so the stray scan below only ever sees
# foreign processes.
sudo systemctl stop "$UNIT" 2>/dev/null
sudo systemctl reset-failed "$UNIT" 2>/dev/null

# Stray (non-unit) ptp4l/phc2sys processes, e.g. from a manual run.
STRAYS="$(pgrep -x -d ' ' ptp4l; pgrep -x -d ' ' phc2sys)"
if [ -n "$STRAYS" ]; then
    echo "Killing stray PTP processes: $STRAYS"
    # shellcheck disable=SC2086 # pid list is intentionally word-split
    sudo kill $STRAYS 2>/dev/null
    sleep 1
    LEFT="$(pgrep -x -d ' ' ptp4l; pgrep -x -d ' ' phc2sys)"
    # shellcheck disable=SC2086
    [ -n "$LEFT" ] && sudo kill -9 $LEFT 2>/dev/null
fi

# After the cleanup nothing may still hold the PHC.
HOLDERS=$(sudo fuser "/dev/ptp$PHC_IDX" 2>/dev/null)
if [ -n "$HOLDERS" ]; then
    echo "ERROR: /dev/ptp$PHC_IDX is still in use by:$HOLDERS"
    sudo fuser -v "/dev/ptp$PHC_IDX"
    exit 1
fi

# NetworkManager bouncing the link (DHCP renegotiation, MAC randomization,
# reconfiguration) interrupts synchronization -> warn loudly.
if command -v nmcli >/dev/null 2>&1 && systemctl -q is-active NetworkManager 2>/dev/null; then
    NM_STATE=$(nmcli -t -f DEVICE,STATE device status 2>/dev/null | awk -F: -v i="$IFACE" '$1 == i {print $2}')
    if [ -n "$NM_STATE" ] && [ "$NM_STATE" != "unmanaged" ]; then
        echo "WARNING: NetworkManager manages $IFACE (state: $NM_STATE) and may bounce the link"
        echo "         consider: sudo nmcli device set $IFACE managed no"
    fi
fi

# --------------------------------------------------------------------- launch
AFFINITY_PROPS=()
[ -n "${PTP_CPU:-}" ] && AFFINITY_PROPS=("--property=CPUAffinity=$PTP_CPU")

echo "Starting $UNIT: ptp4l on $IFACE (SCHED_FIFO $RT_PRIO, oom_score_adj -1000, auto-restart)"
sudo systemd-run \
    --unit="$UNIT" \
    --description="gPTP daemon (Milan-AVB) on $IFACE" \
    --property=CPUSchedulingPolicy=fifo \
    --property=CPUSchedulingPriority="$RT_PRIO" \
    --property=OOMScoreAdjust=-1000 \
    --property=Restart=on-failure \
    --property=RestartSec=1 \
    "${AFFINITY_PROPS[@]}" \
    "$PTP4L_BIN" -f "$GPTP_CFG" -i "$IFACE" --step_threshold=1 -m \
    || exit 1

# --------------------------------------------------------------------- verify
PID=0
for _ in $(seq 1 16); do
    if [ "$(systemctl is-active "$UNIT" 2>/dev/null)" = "active" ]; then
        PID=$(systemctl show -p MainPID --value "$UNIT")
        [ -n "$PID" ] && [ "$PID" != 0 ] && break
    fi
    sleep 0.5
done
if [ "$PID" = 0 ]; then
    echo "ERROR: $UNIT did not come up; last log lines:"
    journalctl -u "$UNIT" -n 20 --no-pager 2>/dev/null
    sudo systemctl stop "$UNIT" 2>/dev/null
    exit 1
fi

# Every ptp4l thread must run SCHED_FIFO at RT_PRIO - nothing may disturb it.
THREADS=$(ps -L -o cls=,rtprio= -p "$PID" 2>/dev/null)
NON_RT=$(echo "$THREADS" | grep -c -E -v "FF[[:space:]]+$RT_PRIO[[:space:]]*$")
echo "ptp4l threads (tid, class, rtprio, name):"
show_threads "$PID"
if [ -z "$THREADS" ]; then
    echo "WARNING: ptp4l (pid $PID) vanished while checking its threads"
elif [ "$NON_RT" -gt 0 ]; then
    echo "WARNING: $NON_RT ptp4l thread(s) NOT at SCHED_FIFO $RT_PRIO"
else
    echo "ptp4l: all threads at SCHED_FIFO $RT_PRIO, oom_score_adj $(cat /proc/"$PID"/oom_score_adj 2>/dev/null)"
fi

# Management sockets: module-avb (a regular user) connects to the read-only
# one; gPTP.cfg sets uds_ro_file_mode 0666 to make that possible.
for _ in $(seq 1 10); do [ -S "$UDS_RO" ] && break; sleep 0.5; done
if [ ! -S "$UDS_RO" ]; then
    echo "WARNING: $UDS_RO did not appear - module-avb cannot read gPTP state"
elif [ ! -w "$UDS_RO" ]; then
    echo "WARNING: $UDS_RO not connectable by $(id -un) - check uds_ro_file_mode 0666 in $GPTP_CFG"
else
    echo "gPTP management socket ready for module-avb: $(stat -c '%a %U:%G' "$UDS_RO") $UDS_RO"
fi

# Optional end-to-end probe of the management interface (the same query
# module-avb makes); pmc stays optional on purpose. -s is required: this
# linuxptp build defaults to /var/run/ptp/ptp4l, not the pinned path.
PMC_BIN="$(command -v pmc)"
if [ -n "$PMC_BIN" ]; then
    if sudo timeout 5 "$PMC_BIN" -u -b 0 -t 1 -s "$UDS_RW" 'GET PARENT_DATA_SET' 2>/dev/null \
            | grep -q grandmasterIdentity; then
        echo "gPTP management interface answers (GET PARENT_DATA_SET)"
    else
        echo "WARNING: no reply on $UDS_RW management socket yet"
    fi
fi

STATE=$(journalctl -u "$UNIT" -n 50 --no-pager 2>/dev/null \
        | grep -oE 'to (SLAVE|MASTER|GRAND_MASTER|UNCALIBRATED|LISTENING)' | tail -n 1)
echo "ptp4l port state: ${STATE:-(none yet - selecting master)}"
echo "$UNIT is up: follow with 'journalctl -fu $UNIT', stop with '$0 stop'"
