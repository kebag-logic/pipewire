#!/bin/bash

# SPDX-FileCopyrightText: Copyright © 2022 Kebag-Logic */
# SPDX-FileCopyrightText: Copyright © 2025 Alexandre Malki <alexandre.malki@kebag-logic.com>
# SPDX-FileCopyrightText: Copyright © 2025 Nils Tonnaett <ntonnatt@ccrma.stanford.edu>
# SPDX-License-Identifier: MIT

# Some of hte code is taken from https://tsn.readthedocs.io/
NIC=${1}

# Clean Everything first
sudo tc qdisc del dev ${NIC} parent root handle 6666 mqprio \
	num_tc 3 \
	map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
	queues 1@0 1@1 2@2 \
	hw 0 > /dev/null  2>&1

# Tune up the system
sudo sysctl -w net.core.default_qdisc=pfifo_fast
sudo sysctl -w net.core.wmem_max=90299200
sudo sysctl -w net.core.wmem_default=90299200

# check if AQCxxx or Intel I210/I226 is used and set CBS HW offloading accordingly
export IS_ATLANTIC=$(ethtool -i $NIC|grep "driver: atlantic"|wc -l)
export IS_INTEL=$(ethtool -i $NIC|grep "driver: ig"|wc -l)

if [ $IS_INTEL -eq 1 ]; then
	export CBS_OFFLOAD=1
	sudo modprobe -r igb
	sudo modprobe igb
	sudo modprobe -r igc
	sudo modprobe igc
else
	export CBS_OFFLOAD=0
	sudo modprobe -r atlantic
	sudo modprobe atlantic
fi

echo "CBS HW offloading: ${CBS_OFFLOAD}"

# Increase the number of descriptor to be used
sudo ethtool -G ${NIC} rx 64
sudo ethtool -G ${NIC} tx 64

# EEE low-power idle adds microsecond-scale wake latency on the link, which
# disturbs gPTP timestamps and AVB latency; some NICs/firmwares ship it on.
sudo ethtool --set-eee ${NIC} eee off 2>/dev/null || true

# Create the MQPrio mapping to Traffic class to Queue
sudo tc qdisc add dev ${NIC} parent root handle 6666 mqprio \
	num_tc 3 \
	map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
	queues 1@0 1@1 2@2 \
	hw 0

# Setup the CBS QDisc for the traffic shaper (CBS-only, no ETF)
# The qdisc value here is set to transmit ONLY 1 Stream
# Calculation are done accordingly to https://tsn.readthedocs.io/qdiscs.html#configuring-cbs-qdisc
sudo tc qdisc replace dev ${NIC} parent 6666:1 cbs \
	idleslope 98688 sendslope -901312 hicredit 153 locredit -1389 \
	offload ${CBS_OFFLOAD}

tc qdisc show dev ${NIC}
