#!/usr/bin/env bash
#
# vcan_bridge_demo.sh â€” Virtual SocketCAN Bridging Demonstration
#
set -euo pipefail

echo "================================================================"
echo " CAN Logger Decoder â€” Virtual Socket Bridging Demonstration     "
echo "================================================================"

if [ "$(uname -s)" != "Linux" ]; then
    echo "[INFO] Linux SocketCAN is required for virtual interfaces."
    echo "[INFO] On Windows/macOS, can_decoder automatically uses the mock engine."
    exit 0
fi

echo "Setting up virtual CAN interfaces vcan0 and vcan1..."
sudo modprobe vcan || true
sudo ip link add dev vcan0 type vcan || true
sudo ip link set up vcan0 || true

echo "vcan0 is UP and listening."