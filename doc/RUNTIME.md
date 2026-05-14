# PipeWire Milan-AVB Runtime Guide

## Scope

This document describes how to start and operate the PipeWire Milan-AVB environment after installation.

---

## Recommended network topology

- Use a star topology as recommended for Ethernet networks.
- Make sure to use AVB capable switches. A variety of Milan-AVB certified switches can be found in the [Avnu Certified Product Registry](https://avnu.org/certified-product-registry/?type=Switch)

### Dedicated network interface recommendation

Please refer to the [Network Interfaces section.](../README.md#network-interfaces)

---

## Start time synchronization

The PTP instance needs to run in a
separate terminal as follows:

### Start LinuxPTP

> [!IMPORTANT]
> Keep this terminal running while using Milan-AVB.
> The gPTP synchronization services must remain active during operation.

```bash
cd ~/pipewire
sudo -E ./ptp-start.sh
sending: SET GRANDMASTER_SETTINGS_NP
phc2sys[2050.269]: Waiting for ptp4l...
phc2sys[2051.269]: Waiting for ptp4l...
phc2sys[2052.269]: Waiting for ptp4l...
phc2sys[2053.269]: Waiting for ptp4l...
phc2sys[2054.270]: CLOCK_REALTIME phc offset 37030500695 s0 freq      -0 delay   2174
phc2sys[2055.270]: CLOCK_REALTIME phc offset 37030515520 s1 freq  +14823 delay   2154
phc2sys[2056.270]: CLOCK_REALTIME phc offset       -19 s2 freq  +14804 delay   2164
phc2sys[2057.270]: CLOCK_REALTIME phc offset       -32 s2 freq  +14785 delay   2164
phc2sys[2058.270]: CLOCK_REALTIME phc offset       -33 s2 freq  +14775 delay   2164
phc2sys[2059.270]: CLOCK_REALTIME phc offset        51 s2 freq  +14849 delay   2144
phc2sys[2060.270]: CLOCK_REALTIME phc offset        -7 s2 freq  +14806 delay   2144
phc2sys[2061.270]: CLOCK_REALTIME phc offset         0 s2 freq  +14811 delay   2124
```

---

## Start PipeWire Milan-AVB
Once PipeWire is installed, it can be started as follows:

`cd ~/pipewire`
Then execute
`./start_pipewire.sh`

---

## Configure audio routing

### Install qpwgraph

1. Install qpwgraph

    `sudo pacman -S qpwgraph`

2. Run qpwgraph by typing `qpwgraph` into the terminal. A window with the available Milan-AVB sources and sinks should show up. You can route audio from other applications to pipewire-milan-avb.

---

## Configure Milan stream connections

### Install Hive

1. Download and install Hive from [https://github.com/christophe-calmejane/Hive/releases](https://github.com/christophe-calmejane/Hive/releases)
2. Run Hive and connect the Milan-AVB device to the Pipewire instance
