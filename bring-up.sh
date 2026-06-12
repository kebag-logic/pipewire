#!/bin/bash

# SPDX-FileCopyrightText: Copyright © 2025 Alexandre Malki <alexandre.malki@kebag-logic.com>
# SPDX-License-Identifier: MIT

# Generic Milan-AVB bring-up. Run once per boot to configure the network
# interface (traffic shaper + VLAN), start gPTP, then start PipeWire Milan-AVB.
# Build/install once first with build-and-install.sh (see doc/INSTALL.md).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Interface from the first argument or the AVB_INTERFACE environment variable
IFACE="${1:-${AVB_INTERFACE:-}}"

if [ -z "$IFACE" ]; then
    echo "Usage: $0 [<network-interface>]"
    echo "Or set AVB_INTERFACE in your environment (see doc/INSTALL.md)."
    exit 1
fi
export AVB_INTERFACE="$IFACE"

echo "Bringing up Milan-AVB on $IFACE"

# 0. Realtime privilege stack: PipeWire's realtime threads come from RTKit,
"$SCRIPT_DIR/check-polkit.sh" || \
    echo "WARNING: continuing without verified realtime privileges - AVB timing will be unreliable"
export MILAN_RT_CHECKED=1

# 1. Traffic shaper (mqprio + CBS) and VLAN id 2
sudo "$SCRIPT_DIR/prepare-traffic-shaper.sh" "$IFACE"
sudo "$SCRIPT_DIR/setup-vlan.sh" "$IFACE"

# 2. gPTP as the supervised system unit milan-ptp4l (SCHED_FIFO 95 on every
# thread, OOM-protected, auto-restart, no controlling terminal).
"$SCRIPT_DIR/ptp-start.sh" "$IFACE" 2>&1 | tee /tmp/ptp-start.log

# 3. PipeWire Milan-AVB (foreground; Ctrl-C stops PipeWire only - gPTP stays up)
"$SCRIPT_DIR/start_pipewire.sh"
