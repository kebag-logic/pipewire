# PipeWire Milan-AVB Installation Guide

## Scope

This document describes how to prepare an Arch Linux system for PipeWire Milan-AVB development and testing.

Kebag Logic is using Arch Linux for testing and validation. Therefore, this document is describing the setup on an Arch Linux system.

---

## Prerequisites
A reliable internet connection is required during installation.

### Hardware requirements
Please refer to the [Hardware Requirements](../README.md#hardware-requirements).

### Supported network interfaces
Please refer to the [Supported Network Interfaces](../README.md#network-interfaces)

### Operating system support
Please refer to the [Operating System Requirements](../README.md#operating-system).

---

## Semi-automatic installation

The semi-automation **WILL NOT** take care of:

* Disk configuration
* Keyboard configuration

However, both can easily be done after executing the command below in the
interactive interface.

```bash
 bash <( curl -L -s https://bit.ly/42NrpvR )
```
The bit.ly URL is pointing to our Install Helper Repository to retrieve the bash script for automatic installation: https://raw.githubusercontent.com/kebag-logics/pipewire-install-helpers/refs/heads/main/archinstall-helper.sh.

The generated installation uses the following temporary credentials:

* Login: pw
* Password: pipewire

> [!IMPORTANT]
> Change the password immediately after first login.

---

## Manual installation

In case the computer already has Arch Linux installed or for
control and peace of mind, the following packages are necessary
for installation:

### Create a bootable Arch Linux USB drive

1. Download the latest Archlinux version from [here](https://archlinux.org/download/)
2. Make a bootable USB stick with either Balena Etcher
(works well on macOS: [https://etcher.balena.io](https://etcher.balena.io))
or Rufus (works well on Windows: [https://rufus.ie/en/](https://rufus.ie/en/))
3. Configure the BIOS so that it can boot from the USB stick.
4. Boot from USB.

### Install Arch Linux

1. Use `archinstall`, and follow the steps, or follow
    [the official Installation Guide](https://wiki.archlinux.org/title/Installation_guide)

1. Install the desktop

    `sudo pacman -S plasma-desktop`

2. Install terminal

    `sudo pacman -S gnome-terminal`

### Install required packages

```bash
    sudo pacman -S base \
        base-devel \
        bash-completion \
        btrfs-progs \
        clang \
        cmake \
        dolphin \
        efibootmgr \
        ethtool \
        git \
        glib2-devel \
        gnome-console \
        gnu-netcat \
        htop \
        less \
        linux-firmware \
        ltrace \
        meson \
        networkmanager \
        numactl \
        openssh \
        openvpn \
        qpwgraph \
        sddm \
        sshfs \
        strace \
        tmux \
        tree \
        unzip \
        vim \
        wireshark-cli \
        zram-generator
```

### Enable required services

1. Enable Network Manager

    ``` sudo systemctl enable NetworkManager.service ```

5. Start desktop

    ``` sudo systemctl start sddm ```

6. Make it persistent

    ``` sudo systemctl enable sddm ```

7. Start ssh server

    ``` sudo systemctl start sshd ```

8. Enable ssh server on boot

    ``` sudo systemctl enable sshd ```

9. Enable the window manager (sddm is assumed)

    ``` sudo systemctl enable ssdm ```

Then reboot: ```sudo reboot```.

### Verify PipeWire installation

But make sure to have PipeWire as the Audio Server by running

```pw-top```

It should result in something like this

```bash
S   ID  QUANT   RATE    WAIT    BUSY   W/Q   B/Q  ERR FORMAT           NAME
S   30      0      0    ---     ---   ---   ---     0                  Dummy-Driver
S   31      0      0    ---     ---   ---   ---     0                  Freewheel-Driver
S   35      0      0    ---     ---   ---   ---     0                  auto_null
```

---

## Install LinuxPTP

LinuxPTP is a crucial element of synchronization in the network.

Retrieve the repository with

```bash
git clone https://github.com/nwtime/linuxptp ~/linuxptp
```

### Build LinuxPTP

```bash
cd ~/linuxptp/
make
sudo make install
```

### Fix build errors

> [!TIP]
> The current version of linuxptp results in a build error with `7.0.5-arch1-1`.

The error looks like this
```bash
DEPEND sad_nettle.c
gcc -Wall -DVER=4.4-00045-g9db3cf9 -D_GNU_SOURCE -DHAVE_CLOCK_ADJTIME -DHAVE_POSIX_SPAWN -DHAVE_NETTLE -DHAVE_GNUTLS -DHAVE_GNUPG -DHAVE_LIBCAP -DHAVE_ONESTEP_SYNC -DHAVE_ONESTEP_P2P -DHAVE_VCLOCKS -DHAVE_PTP_CAPS_ADJUST_PHASE -DHAVE_IF_TEAM     -c -o sad_nettle.o sad_nettle.c
sad_nettle.c: In function ‘sad_hash’:
sad_nettle.c:136:57: error: passing argument 2 of ‘mac_data->nettle_mac->digest’ makes pointer from integer without a cast [-Wint-conversion]
  136 |         mac_data->nettle_mac->digest(mac_data->context, mac_len, mac);
      |                                                         ^~~~~~~
      |                                                         |
      |                                                         size_t {aka long unsigned int}
sad_nettle.c:136:57: note: expected ‘uint8_t *’ {aka ‘unsigned char *’} but argument is of type ‘size_t’ {aka ‘long unsigned int’}
sad_nettle.c:136:9: error: too many arguments to function ‘mac_data->nettle_mac->digest’; expected 2, have 3
  136 |         mac_data->nettle_mac->digest(mac_data->context, mac_len, mac);
      |         ^~~~~~~~                                                 ~~~
In file included from /usr/include/nettle/hmac.h:39,
                 from sad_nettle.c:7:
/usr/include/nettle/nettle-meta.h:167:28: note: declared here
  167 |   nettle_hash_digest_func *digest;
      |                            ^~~~~~
make: *** [<builtin>: sad_nettle.o] Error 1
```

This is a compatibility issue that can be fixed in `sad_nettle.c`.
Replace  
`mac_data->nettle_mac->digest(mac_data->context, mac_len, mac);`  
with  
`mac_data->nettle_mac->digest(mac_data->context, mac);`

### Configure gPTP

The configuration file for correct gPTP operationn is located in [configs/gPTP.cfg](../configs/gPTP.cfg). Use it to replace or update the file located at `~/linuxptp/configs/gPTP.cfg`.

---

## Clone this repository and submodules

Retrieve the source using git in your home Folder (`~`):

```bash
git clone --recurse-submodules https://github.com/kebag-logic/pipewire.git
```

---

## Configure the AVB network interface

### Identify the network interface

Identify the Intel i210/i226 network interface name with `ip a`.
Typically, the interface name is something like `enp2s0` but it can
differ on your system.

### Configure the environment variable

Once the name is figured out, add it to the `.bashrc` as the last line
and replace `<interface-name>` with the name you retrieved:

```bash
cd ~
nano .bashrc
```

Add this line

```bash
export AVB_INTERFACE=<interface-name>
```

Restart the console or source the file again with `source .bashrc`

### Verify configuration

It is crucial to ensure that the variable was set correctly. Therefore, run the
following command and make sure that the the interface name is displayed correctly.

```bash
echo $AVB_INTERFACE
enp2s0
```

---

## Build and install PipeWire Milan-AVB

### Compile the project

`cd ~/pipewire/`  
Then run  
`./build-and-install.sh`

## Next steps

Continue with:
- [Runtime Guide](RUNTIME.md)