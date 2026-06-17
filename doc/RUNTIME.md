# PipeWire Milan-AVB Runtime Guide

## Scope

This document describes how to start and operate the PipeWire Milan-AVB
environment after installation.

---

## Recommended network topology

- Use a star topology as recommended for Ethernet networks.
- Make sure to use AVB capable switches. A variety of Milan-AVB
certified switches can be found in the [Avnu Certified Product Registry](https://avnu.org/certified-product-registry/?type=Switch)

---

## Verify realtime privileges (polkit)

PipeWire's realtime threads are granted by `rtkit`, which asks
`polkit` for permission (`org.freedesktop.RealtimeKit1.*` actions). If
polkit denies — typical for processes outside a logind session on headless
nodes — PipeWire silently runs without realtime scheduling and AVB timing
falls apart.

```bash
cd ~/pipewire
./check-polkit.sh
```

The check is read-only. It verifies the polkit authority, the rtkit policy
and daemon, the live authorization verdict for your user, the
`RLIMIT_RTPRIO` fallback path, and — when PipeWire is already running —
whether its processes actually hold realtime threads. Exit code 0 means at
least one realtime path works; on failure the script prints the exact
remediation (the `realtime-privileges` group or a polkit rule).
`bring-up.sh` and `start_pipewire.sh` run it automatically.

---

## Start time synchronization

### Start LinuxPTP

ptp4l runs as the supervised systemd unit `milan-ptp4l`, so that nothing can
disturb it:

- every ptp4l thread runs `SCHED_FIFO 95`, above PipeWire's data threads —
  no other userspace can preempt it
- `oom_score_adj -1000` — the OOM killer always picks something else
- no controlling terminal — closing the shell, ssh disconnects or Ctrl-C
  cannot kill it
- `Restart=on-failure` — a crash self-heals within ~1 s

> [!NOTE]
> ptp4l disciplines the NIC PHC directly and PipeWire reads gPTP time from the PHC.
> phc2sys is not used, and NTP is left enabled on the system clock.
> The start script stops anything else that would fight over the PHC or the NIC
> timestamping configuration (stray ptp4l/phc2sys/timemaster, chronyd hwtimestamp).

The PTP daemon is brought during the `bring-up.sh` script. The milan-ptp4l logs
can be accessed via journalctl:

```bash
journalctl -fu milan-ptp4l   # follow the ptp4l log (rms lines)
```

Expected log:

```
ptp4l[2051.269]: rms       12 max       34 freq -26000 +/- 102 delay  2164 +/-  12
ptp4l[2052.269]: rms        4 max        9 freq -26010 +/-  45 delay  2160 +/-  10
```

---

## Start PipeWire Milan-AVB
Once PipeWire is installed, it can be started as follows:

`cd ~/pipewire`
Then execute
`./bring-up.sh`

This script takes care of everything: PTP, PipeWire Milan-AVB module, traffic
shaping and VLAN configuration.

---

## Configure audio routing

### Install qpwgraph

1. Install qpwgraph

    `sudo pacman -S qpwgraph`

2. Run qpwgraph by typing `qpwgraph` into the terminal. A window with the
available Milan-AVB sources and sinks should show up. You can route audio from
other applications to `AVB Source` and `AVB Sink`.

---

## Configure Milan-AVB stream connections

### Install Hive

1. Download and install Hive from [https://github.com/christophe-calmejane/Hive/releases](https://github.com/christophe-calmejane/Hive/releases)
2. Run Hive and connect the Milan-AVB device to the Pipewire instance

> [!CAUTION]
> Running Hive on the same Ethernet interface as the PipeWire Milan-AVB module can
> cause undefined behavior in Hive.
