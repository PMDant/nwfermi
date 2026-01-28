#!/bin/bash
# Upgrade from 2.0.2 to 2.0.3 - fixes start/stop flapping

set -e

if [ "$(id -u)" != "0" ]; then
   echo "Must run as root" 1>&2
   exit 1
fi

echo "Upgrading to 2.0.3 (fixes flapping issue)..."

# Unload and remove old
rmmod nwfermi 2>/dev/null || true
dkms remove -m nwfermi -v 2.0.2 --all 2>/dev/null || true

# Install new
dkms add -m nwfermi -v 2.0.3
dkms build -m nwfermi -v 2.0.3
dkms install -m nwfermi -v 2.0.3
modprobe nwfermi

echo "Done! Check: journalctl -b | grep nwfermi | tail"
