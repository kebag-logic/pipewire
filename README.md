# PipeWire Milan-AVB

> [!CAUTION]
> Milan-AVB support is currently experimental.
> The implementation is still under development and does not yet fully comply with all IEEE AVB and Milan-AVB specification requirements.
> Interoperability with certified Milan devices is not guaranteed.

This repository provides an integration and deployment framework for Milan-AVB on Linux using PipeWire.

The repository contains:
- setup and deployment scripts
- Milan-specific configuration
- system integration helpers
- a pinned upstream PipeWire submodule

The actual PipeWire source code is included as a Git submodule to maintain a clear dependency on a known upstream version while keeping Milan integration scripts separate from PipeWire development itself.

A brief overview of the history of this project can be found in the [History Channel](doc/HISTORY.md).

## Current status
Development of Milan-AVB support in PipeWire is tracked in the upstream PipeWire project: [AVB: Integrate Milan](https://gitlab.freedesktop.org/pipewire/pipewire/-/work_items/4973)

## Repository layout

| Path | Purpose |
|---|---|
| `configs/` | Milan-AVB and gPTP configuration files |
| `doc` | History, installation and runtime documentation |
| `pipewire/` | Upstream PipeWire submodule |
| `build-and-install.sh` | Build helper |
| `prepare-traffic-shaper.sh` | Traffic shaping and hardware TX/RX queue configuration |
| `ptp-start.sh` | LinuxPTP startup helper |
| `setup-vlan.sh` | VLAN configuration |
| `start_pipewire.sh` | PipeWire startup helper |

## Hardware Requirements

Milan-AVB requires hardware capable of deterministic low-latency packet scheduling and precise time synchronization.

### Network interfaces

> [!IMPORTANT]
> The network interface must support:
> - IEEE 1588 hardware timestamping
> - multiple hardware TX/RX queues
> - traffic prioritization for AVB Stream Reservation classes

  * Intel i210 (validated)
  * Intel i226 (expected to work, not yet validated)

### Hardware platform

Recommended minimum system configuration:

- Modern x86_64 CPU with multiple physical cores
- SMT/Hyper-Threading supported and enabled
- 8 GB RAM minimum
- 60 GB available SSD storage

### Operating system

Officially supported distribution:

- Arch Linux (tested regularly)

Community testing status:

- Ubuntu 24.04 LTS (not officially supported)

## Installation guide

1. For setting up a dedicated Arch Linux machine, follow the steps in the [Arch Linux Guide](doc/INSTALL.md)
2. To build and install the Milan-AVB enabled PipeWire environment, the following components are required:

    1. LinuxPTP: [LinuxPTP Guide](doc/INSTALL.md#install-linuxptp)
    2. PipeWire from this repository: [PipeWire Installation Guide](doc/INSTALL.md#build-and-install-pipewire-milan-avb)

3. For running PipeWire with the Milan-AVB implementation, follow the steps in the [PipeWire Install Guide](doc/INSTALL.md#configure-the-avb-network-interface)

## Runtime guide

Once the installation and configuration of the system has been done. PipeWire with the Milan-AVB functionality can be started as described in the [Runtime Guide](doc/RUNTIME.md)

## Contributions

Contributions are welcome.

### PipeWire core functionality

Changes to PipeWire core infrastructure and generic functionality should be submitted upstream to the PipeWire project:

- [PipeWire GitLab](https://gitlab.freedesktop.org/pipewire/pipewire?utm_source=chatgpt.com)

### Milan integration framework

Changes related to:
- setup scripts
- Milan configuration
- deployment tooling
- system integration

can be contributed directly to this repository.

## Introduction to AVB and Milan-AVB

For background information about AVB, Milan, gPTP, SRP, and related technologies, visit the [AVB Academy](https://avb-academy.com).