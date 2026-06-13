#!/bin/bash

NIC=${1}

sudo modprobe 8021q
# bring the parent NIC up: prepare-traffic-shaper.sh reloads the igb driver, which
# leaves the interface administratively down; module-avb fails to bind it otherwise.
sudo ip link set dev ${NIC} up
sudo ip link add link ${NIC} name ${NIC}.2 type vlan id 2
# MVRP declares VID-2 membership to the AVB switch so it forwards the SR stream.
sudo ip link set dev ${NIC}.2 type vlan mvrp on
sudo ip link set dev ${NIC}.2 up
