#!/bin/bash

# SPDX-FileCopyrightText: Copyright © 2025 Alexandre Malki <alexandre.malki@kebag-logic.com>
# SPDX-License-Identifier: MIT

# Verify the privilege stack PipeWire Milan-AVB relies on for realtime
# scheduling. PipeWire's module-rt tries, in order:
#   1. sched_setscheduler() directly -- needs RLIMIT_RTPRIO >= rt.prio
#   2. RTKit over D-Bus              -- rtkit-daemon asks polkit to authorize
#      org.freedesktop.RealtimeKit1.acquire-real-time / acquire-high-priority
# On a stock Arch install RLIMIT_RTPRIO is 0, so the RTKit/polkit path is the
# only one; if polkit denies (typical for processes outside a logind session
# on headless nodes), PipeWire runs without realtime threads and AVB timing
# falls apart silently. This script makes that failure loud and explains the
# fix.
#
# Read-only: performs authorization checks only, never changes system state.
# Exit 0: at least one realtime path verified working.
# Exit 1: PipeWire will NOT get realtime scheduling on this node.

# rt.prio module-rt asks for (see configs/pipewire-avb.conf.in)
RT_PRIO_WANTED="${RT_PRIO_WANTED:-83}"

RTKIT_OK=1
RLIMIT_OK=1

echo "== Milan-AVB realtime privilege check (user $(id -un), uid $(id -u)) =="

# --- 1. polkit authority answers and the rtkit policy is installed ----------
if ! command -v pkcheck >/dev/null 2>&1; then
    echo "[FAIL] pkcheck not found - polkit is not installed (pacman -S polkit)"
    RTKIT_OK=0
else
    POLICY=$(pkaction --action-id org.freedesktop.RealtimeKit1.acquire-real-time --verbose 2>&1)
    if [ $? -ne 0 ]; then
        echo "[FAIL] polkit authority not answering or rtkit policy missing:"
        echo "$POLICY" | head -n 2 | sed 's/^/       /'
        RTKIT_OK=0
    else
        echo "[ OK ] polkit authority is answering (polkit.service: $(systemctl is-active polkit 2>/dev/null))"
        echo "$POLICY" | sed -n 's/^[[:space:]]*implicit/       implicit/p'
    fi

    # --- 2. the actual authorization verdict for THIS process ---------------
    for ACTION in acquire-real-time acquire-high-priority; do
        OUT=$(pkcheck --action-id "org.freedesktop.RealtimeKit1.$ACTION" --process $$ 2>&1)
        if [ $? -eq 0 ]; then
            echo "[ OK ] polkit authorizes $ACTION for this process"
        elif [ "$ACTION" = "acquire-real-time" ]; then
            echo "[FAIL] polkit DENIES $ACTION: ${OUT:-no detail}"
            RTKIT_OK=0
        else
            # nice-level boost only; not fatal for AVB timing
            echo "[WARN] polkit denies $ACTION (nice level boost unavailable): ${OUT:-no detail}"
        fi
    done
fi

# --- 3. rtkit-daemon is there to forward the request (chk daemon) -------------------------
if systemctl -q is-active rtkit-daemon.service 2>/dev/null; then
    echo "[ OK ] rtkit-daemon is running"
elif systemctl list-unit-files rtkit-daemon.service --no-legend 2>/dev/null | grep -q rtkit; then
    echo "[ OK ] rtkit-daemon installed (D-Bus activated on first request)"
else
    echo "[FAIL] rtkit-daemon not installed (pacman -S rtkit)"
    RTKIT_OK=0
fi

# --- 4. session context (what polkit bases its decision on) ------------------
SESSIONS=$(loginctl list-sessions --no-legend 2>/dev/null | awk -v uid="$(id -u)" '$2 == uid {print $1}')
if [ -n "$SESSIONS" ]; then
    for S in $SESSIONS; do
        echo "[INFO] logind session $S: active=$(loginctl show-session "$S" -p Active --value 2>/dev/null)"
    done
else
    echo "[INFO] no logind session (cron/setsid start?) - polkit applies its 'implicit any' policy"
fi

# --- 5. RLIMIT_RTPRIO fallback (works without polkit/rtkit) ------------------
HARD=$(ulimit -Hr)
if [ "$HARD" = "unlimited" ] || [ "$HARD" -ge "$RT_PRIO_WANTED" ] 2>/dev/null; then
    echo "[ OK ] RLIMIT_RTPRIO hard limit is $HARD (>= rt.prio $RT_PRIO_WANTED) - direct path works without polkit"
else
    echo "[INFO] RLIMIT_RTPRIO hard limit is $HARD (< rt.prio $RT_PRIO_WANTED) - no direct path, RTKit/polkit is required"
    RLIMIT_OK=0
fi

# --- 6. live outcome: do running PipeWire processes hold realtime threads? ---
for NAME in pipewire pipewire-avb; do
    for PID in $(pgrep -u "$(id -u)" -x "$NAME" 2>/dev/null); do
        RT_THREADS=$(ps -L -o cls= -p "$PID" 2>/dev/null | grep -c -E 'FF|RR')
        if [ "$RT_THREADS" -gt 0 ]; then
            echo "[ OK ] $NAME (pid $PID) holds $RT_THREADS realtime thread(s) right now"
        else
            echo "[WARN] $NAME (pid $PID) is running WITHOUT realtime threads"
        fi
    done
done

# --- verdict ------------------------------------------------------------------
echo
if [ "$RTKIT_OK" = 1 ]; then
    echo "VERDICT: PASS - RTKit/polkit path is working; PipeWire will get realtime scheduling"
    exit 0
fi
if [ "$RLIMIT_OK" = 1 ]; then
    echo "VERDICT: PASS - polkit path unavailable, but RLIMIT_RTPRIO covers rt.prio $RT_PRIO_WANTED"
    exit 0
fi
echo "VERDICT: FAIL - PipeWire will NOT get realtime scheduling. Fix one of:"
echo "  A) rlimit path (recommended for headless nodes, no polkit involved):"
echo "       sudo pacman -S realtime-privileges && sudo gpasswd -a $(id -un) realtime"
echo "     then log out and back in"
echo "  B) polkit rule for RTKit, /etc/polkit-1/rules.d/40-rtkit-$(id -un).rules:"
echo "       polkit.addRule(function(action, subject) {"
echo "           if ((action.id == \"org.freedesktop.RealtimeKit1.acquire-real-time\" ||"
echo "                action.id == \"org.freedesktop.RealtimeKit1.acquire-high-priority\") &&"
echo "               subject.user == \"$(id -un)\")"
echo "               return polkit.Result.YES;"
echo "       });"
echo "  C) install/enable the missing pieces: sudo pacman -S polkit rtkit &&"
echo "     sudo systemctl enable --now polkit rtkit-daemon"
exit 1
