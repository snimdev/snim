#!/bin/bash

# Installation script for Wayland-compatible screenshot tools

echo "Installing Wayland-compatible screenshot tools for Fedora KDE..."

# Check if we're on Fedora
if ! command -v dnf &> /dev/null; then
    echo "This script is designed for Fedora. Please install screenshot tools manually."
    exit 1
fi

# Install common screenshot tools that work with Wayland
echo "Installing screenshot tools..."

# Spectacle (KDE's native screenshot tool - best for KDE)
sudo dnf install -y spectacle

# Alternative tools
sudo dnf install -y gnome-screenshot

# Optional: Install grim and slurp for advanced Wayland support
echo "Installing optional Wayland-specific tools..."
sudo dnf install -y grim slurp

echo "Installation complete!"
echo ""
echo "Your screenshot app will automatically detect and use:"
echo "1. Spectacle (recommended for KDE)"
echo "2. GNOME Screenshot"
echo "3. Grim + Slurp (for sway/wlroots compositors)"
echo ""
echo "The app will work on both X11 and Wayland sessions."
