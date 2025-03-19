#!/bin/bash

# install.sh for SlimWM - Installs dependencies, clones repo and builds/installs

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if running as root (discouraged)
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}[!] This script should not be run as root. Use a regular user with sudo privileges.${NC}"
    exit 1
fi

# Detect distribution
DISTRO=""
if [ -f /etc/debian_version ]; then
    DISTRO="debian"
elif [ -f /etc/arch-release ]; then
    DISTRO="arch"
elif [ -f /etc/fedora-release ]; then
    DISTRO="fedora"
elif [ -f /etc/gentoo-release ]; then
    DISTRO="gentoo"
elif [ -f /etc/lfs-release ]; then
    DISTRO="lfs"
else
    echo -e "${RED}[!] Unsupported distribution detected. Supported: Debian/Ubuntu, Arch, Fedora, Gentoo, LFS (manual).${NC}"
    exit 1
fi

# Function to install dependencies
install_deps() {
    echo -e "${BLUE}[*] Handling dependencies for $DISTRO...${NC}"
    case $DISTRO in
        "debian")
            sudo apt update
            sudo apt install -y git gcc make libx11-dev libxext-dev
            ;;
        "arch")
            sudo pacman -Sy --noconfirm git base-devel libx11 libxext
            ;;
        "fedora")
            sudo dnf install -y git gcc make libX11-devel libXext-devel
            ;;
        "gentoo")
            sudo emerge --sync
            sudo emerge -av dev-vcs/git sys-devel/gcc sys-devel/make x11-libs/libX11 x11-libs/libXext
            ;;
        "lfs")
            echo -e "${BLUE}[*] Linux From Scratch detected. Please manually install these dependencies:${NC}"
            echo -e "${GREEN}  - git${NC}"
            echo -e "${GREEN}  - gcc${NC}"
            echo -e "${GREEN}  - make${NC}"
            echo -e "${GREEN}  - libX11 (X11 library)${NC}"
            echo -e "${GREEN}  - libXext (X11 extensions library)${NC}"
            echo -e "${BLUE}[*] After installing these, re-run the script to continue.${NC}"
            exit 0
            ;;
    esac
    if [ $? -ne 0 ] && [ "$DISTRO" != "lfs" ]; then
        echo -e "${RED}[-] Failed to install dependencies. Check your package manager or internet connection.${NC}"
        exit 1
    elif [ "$DISTRO" != "lfs" ]; then
        echo -e "${GREEN}[+] Dependencies installed successfully.${NC}"
    fi
}

# Function to clone and install SlimWM
install_slimwm() {
    # Check if git is installed
    if ! command -v git &> /dev/null; then
        echo -e "${RED}[-] Git is not installed. Please install it manually and re-run the script.${NC}"
        exit 1
    fi

    echo -e "${BLUE}[*] Cloning SlimWM repository...${NC}"
    git clone https://github.com/user7210unix/SlimWM.git ~/SlimWM
    if [ ! -d "~/SlimWM" ]; then
        echo -e "${RED}[-] Failed to clone SlimWM repository. Check your internet or the URL.${NC}"
        exit 1
    fi

    cd ~/SlimWM

    # Check if sxhkd folder exists
    if [ ! -d "sxhkd" ]; then
        echo -e "${RED}[-] sxhkd folder not found in SlimWM repository. Skipping copy.${NC}"
    else
        echo -e "${BLUE}[*] Copying sxhkd configuration to ~/.config/...${NC}"
        mkdir -p ~/.config
        cp -r sxhkd ~/.config/
        if [ $? -ne 0 ]; then
            echo -e "${RED}[-] Failed to copy sxhkd folder to ~/.config/.${NC}"
            exit 1
        else
            echo -e "${GREEN}[+] sxhkd configuration copied successfully.${NC}"
        fi
    fi

    # Build and install SlimWM
    echo -e "${BLUE}[*] Building and installing SlimWM...${NC}"
    make
    if [ $? -ne 0 ]; then
        echo -e "${RED}[-] Make failed. Check the Makefile or dependencies.${NC}"
        exit 1
    fi

    sudo make install
    if [ $? -ne 0 ]; then
        echo -e "${RED}[-] Installation failed. Check sudo privileges or Makefile.${NC}"
        exit 1
    else
        echo -e "${GREEN}[+] SlimWM installed successfully.${NC}"
    fi
}

# Main execution
echo -e "${BLUE}[*] Starting SlimWM installation script...${NC}"
install_deps
install_slimwm
echo -e "${GREEN}[+] Installation complete! You can now run 'slimwm' or configure your session to use it.${NC}"
echo -e "${BLUE}[*] Ensure sxhkd is running (e.g., 'sxhkd &') for keybindings.${NC}"
