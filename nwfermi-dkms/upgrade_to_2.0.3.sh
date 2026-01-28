#!/bin/bash
# Upgrade nwfermi

set -e

if [ "$(id -u)" != "0" ]; then
   echo "Must run as root" 1>&2
   exit 1
fi

echo "Upgrading nwfermi..."

# Unload and remove old
rmmod nwfermi 2>/dev/null || true
dkms remove -m nwfermi -v 2.0.4 --all 2>/dev/null || true

# Install new
dkms add -m nwfermi -v 2.0.5
dkms build -m nwfermi -v 2.0.5
dkms install -m nwfermi -v 2.0.5
modprobe nwfermi

echo "Done! Check: journalctl -b | grep nwfermi | tail"
