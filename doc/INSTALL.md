# PipeWire-Milan-AVB Installation Guide

## Prerequisites

A reliable internet connection is required during installation.

## Automatic Installation

The current documentation is describing how to make PipeWire run on ArchLinux.

### Semi-automatic installation

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

## Manual installation

In case the computer already has Arch Linux installed or for
control and peace of mind, the following packages are necessary
for installation:

### Create a bootable Arch Linux USB drive

Steps:

1. Download the latest Archlinux version from [here](https://archlinux.org/download/)
2. Make a bootable USB stick with either Balena Etcher
(works well on macOS: [https://etcher.balena.io](https://etcher.balena.io))
or Rufus (works well on Windows: [https://rufus.ie/en/](https://rufus.ie/en/))
3. Configure the BIOS so that it can boot from the USB stick.
4. Boot from USB.s
5.  Use `archinstall`, and follow the steps, or follow
    [the official Installation Guide](https://wiki.archlinux.org/title/Installation_guide)

### Install Arch Linux

1. Install the desktop

    ``` sudo pacman -S plasma-desktop ```

2. Install terminal

    ``` sudo pacman -S gnome-terminal ```

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

### Verify that PipeWire is running

But make sure to have PipeWire as the Audio Server by running

```pw-top```

It should result in something like this

```bash
S   ID  QUANT   RATE    WAIT    BUSY   W/Q   B/Q  ERR FORMAT           NAME
S   30      0      0    ---     ---   ---   ---     0                  Dummy-Driver
S   31      0      0    ---     ---   ---   ---     0                  Freewheel-Driver
S   35      0      0    ---     ---   ---   ---     0                  auto_null
```

## Install LinuxPTP

LinuxPTP is a crucial element of synchronization in the network.

Install as follows:

```bash
 git clone http://git.code.sf.net/p/linuxptp/code ~/linuxptp
 cd ~/linuxptp/
 make
 sudo make install
```

Then modify the file located at ```~/linuxptp/configs/gPTP.conf```.
Replace the default LinuxPTP gPTP configuration with the provided repository configuration: [gPTP.cfg](../configs/gPTP.cfg)

## Retrieve the repository

Retrieve the source using git in your home Folder (`~`):

```bash
git clone --recurse-submodules https://github.com/kebag-logic/pipewire.git
```

## Prepare the system to Run PipeWire

Connect a Milan-AVB capable device to the network interface where Milan-AVB is going
to receive/send from/to (i210/i226) network card.

---

> [!IMPORTANT]
> This step only has to be performed once.

Identify the Intel i210/i226 network interface name with `ip a`.
Typically, the interface name is something like `enp2s0` but it can
differ on your system.

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

Restart the console or source the file again with ```source .bashrc```

It is crucial to ensure that the variable was set correctly. Therefore, run the
following command and make sure that the the interface name is displayed correctly.

```bash
echo $AVB_INTERFACE
enp2s0
```

---

### Compile PipeWire and install

Now, execute the following for compilation:

```bash
# Follow the PipeWire folder
cd ~/pipewire/
./build-and-install.sh
```

---

## Start time synchronization

Once the compilation is done, the PTP instance needs to run in a
separate terminal as follows:

```bash
cd ~/pipewire
./ptp-start.sh
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

> [!IMPORTANT]
> Keep this terminal running while using Milan-AVB.
> The gPTP synchronization services must remain active during operation.

## Run PipeWire

Once PipeWire is installed, the execution shall be done as below in a terminal:

```bash
cd ~/pipewire
./start_pipewire.sh
```

## Configure PipeWire inputs and outputs

1. Install qpwgraph

    ```sudo pacman -S qpwgraph```

2. Run qpwgraph by typing `qpwgraph` into the terminal. A window with the available Milan-AVB sources and sinks should show up. You can route audio from other applications to pipewire-milan-avb.

## Make stream connections in Hive

Use Hive to establish Milan-AVB stream connections between the device and the PipeWire Milan-AVB instance.

1. Download and install Hive from [https://github.com/christophe-calmejane/Hive/releases](https://github.com/christophe-calmejane/Hive/releases)
2. Run Hive and connect the Milan-AVB device to the Pipewire instance
