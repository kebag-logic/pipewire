#!/bin/bash
# Restart pipewire
systemctl --user restart pipewire.service

# Ensure variable exists
if [ -z "$AVB_INTERFACE" ]; then
    echo "AVB_INTERFACE is not set"
    exit 1
fi

# Start with verbose logging and selected interface
/usr/bin/pipewire-avb -v