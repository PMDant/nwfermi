#!/bin/bash
set -e

PACKAGE_NAME="nwfermi"
PACKAGE_VERSION="2.0.3"

echo "Installing NextWindow Fermi touchscreen driver..."

# Check if running as root
if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Install to DKMS
echo "Adding to DKMS..."
dkms add -m $PACKAGE_NAME -v $PACKAGE_VERSION

# Build the module
echo "Building module..."
dkms build -m $PACKAGE_NAME -v $PACKAGE_VERSION

# Install the module
echo "Installing module..."
dkms install -m $PACKAGE_NAME -v $PACKAGE_VERSION

# Create modprobe configuration to blacklist old driver if it exists
echo "Configuring module loading..."
cat > /etc/modprobe.d/nwfermi.conf << EOF
# Blacklist any old NextWindow drivers
blacklist nwfermi_old

# Load new nwfermi driver
alias usb:v1926p1846d*dc*dsc*dp*ic*isc*ip* nwfermi
alias usb:v1926p1875d*dc*dsc*dp*ic*isc*ip* nwfermi
alias usb:v1926p1878d*dc*dsc*dp*ic*isc*ip* nwfermi
EOF

# Update initramfs
echo "Updating initramfs..."
update-initramfs -u

echo ""
echo "Installation complete!"
echo ""
echo "To load the driver now:"
echo "  sudo modprobe nwfermi"
echo ""
echo "To enable debug messages:"
echo "  sudo modprobe nwfermi"
echo "  sudo bash -c 'echo 8 > /proc/sys/kernel/printk'"
echo "  sudo bash -c 'echo \"module nwfermi +p\" > /sys/kernel/debug/dynamic_debug/control'"
echo ""
echo "To view debug output:"
echo "  sudo dmesg -w | grep -i fermi"
echo ""
