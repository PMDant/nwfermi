#!/bin/bash
set -e

PACKAGE_NAME="nwfermi"
PACKAGE_VERSION="2.0.2"

echo "Uninstalling NextWindow Fermi touchscreen driver..."

# Check if running as root
if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Unload module if loaded
echo "Unloading module..."
if lsmod | grep -q nwfermi; then
    rmmod nwfermi || true
fi

# Uninstall from DKMS
echo "Removing from DKMS..."
dkms remove -m $PACKAGE_NAME -v $PACKAGE_VERSION --all || true

# Remove modprobe configuration
echo "Removing configuration..."
rm -f /etc/modprobe.d/nwfermi.conf

# Update initramfs
echo "Updating initramfs..."
update-initramfs -u

echo ""
echo "Uninstallation complete!"
echo ""
