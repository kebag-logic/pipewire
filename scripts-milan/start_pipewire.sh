#!/bin/bash
# Restart pipewire
systemctl --user restart pipewire.service

# Start with verbose logging
/usr/bin/pipewire-milan -v
