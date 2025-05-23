#!/bin/bash
##############################################################################
# Copyright (c) 2022-2025
#
# Author(s):
#  Christian Hoffmann
#  The Jamulus Development Team
#
##############################################################################
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
#
##############################################################################

set -eu

if [[ ! ${JAMULUS_BUILD_VERSION:-} =~ [0-9]+\.[0-9]+\.[0-9]+ ]]; then
    echo "Environment variable JAMULUS_BUILD_VERSION has to be set to a valid version string"
    exit 1
fi

TARGET_ARCH="${TARGET_ARCH:-amd64}"
case "${TARGET_ARCH}" in
    amd64)
        ABI_NAME=""
        ;;
    armhf)
        ABI_NAME="arm-linux-gnueabihf"
        ;;
    arm64)
        ABI_NAME="aarch64-linux-gnu"
        ;;
    *)
        echo "Unsupported TARGET_ARCH ${TARGET_ARCH}"
        exit 1
        ;;
esac

setup() {
    export DEBIAN_FRONTEND="noninteractive"

    APT_ARCH="$(dpkg --print-architecture)"
    export APT_ARCH

    setup_cross_compilation_apt_sources

    echo "Installing dependencies..."
    sudo apt-get -qq update
    sudo apt-get -qq --no-install-recommends -y install devscripts build-essential debhelper fakeroot libjack-jackd2-dev qtbase5-dev qttools5-dev-tools qtmultimedia5-dev

    setup_cross_compiler
}

setup_cross_compilation_apt_sources() {
    if [[ "${TARGET_ARCH}" == "${APT_ARCH}" ]]; then
        return
    fi
    echo "Building on ${APT_ARCH} for ${TARGET_ARCH}. Thus modifying apt..."
    sudo dpkg --add-architecture "${TARGET_ARCH}"

    if [[ "${APT_ARCH}" == "amd64"  ]]; then
        # Duplicate the original Ubuntu sources and modify them to refer to the TARGET_ARCH:
        sed -rne "s|^deb.*/ ([^ -]+(-updates)?) main.*|deb [arch=${TARGET_ARCH}] http://ports.ubuntu.com/ubuntu-ports \1 main universe multiverse restricted|p" /etc/apt/sources.list | sudo dd of=/etc/apt/sources.list.d/"${TARGET_ARCH}".list
        # Now take the original Ubuntu sources and limit those to the build host (i.e. non-TARGET_ARCH) architectures:
        sudo sed -re 's/^deb /deb [arch=amd64,i386] /' -i /etc/apt/sources.list
    fi
}

setup_cross_compiler() {
    if [[ "${TARGET_ARCH}" == "${APT_ARCH}" ]]; then
        return
    fi
    local GCC_VERSION=9  # 9 is the default on 20.04, there is no reason not to update once 20.04 is out of support
    sudo apt install -qq -y --no-install-recommends "g++-${GCC_VERSION}-${ABI_NAME}" "qt5-qmake:${TARGET_ARCH}" "qtbase5-dev:${TARGET_ARCH}" "libjack-jackd2-dev:${TARGET_ARCH}" "qtmultimedia5-dev:${TARGET_ARCH}"
    sudo update-alternatives --install "/usr/bin/${ABI_NAME}-g++" g++ "/usr/bin/${ABI_NAME}-g++-${GCC_VERSION}" 10
    sudo update-alternatives --install "/usr/bin/${ABI_NAME}-gcc" gcc "/usr/bin/${ABI_NAME}-gcc-${GCC_VERSION}" 10

    # Ubuntu's Qt version only ships a profile for gnueabi, but not for gnueabihf or aarch64. Therefore, build a custom one:
    if [[ $ABI_NAME ]]; then
        sudo cp -R "/usr/lib/${ABI_NAME}/qt5/mkspecs/linux-arm-gnueabi-g++/" "/usr/lib/${ABI_NAME}/qt5/mkspecs/${ABI_NAME}-g++/"
        sudo sed -re "s/arm-linux-gnueabi/${ABI_NAME}/" -i "/usr/lib/${ABI_NAME}/qt5/mkspecs/${ABI_NAME}-g++/qmake.conf"
    fi
}

build_app_as_deb() {
    TARGET_ARCH="${TARGET_ARCH}" ./linux/deploy_deb.sh
}

pass_artifacts_to_job() {
    mkdir deploy

    # rename headless first, so wildcard pattern matches only one file each
    local artifact_1="jamulus-headless_${JAMULUS_BUILD_VERSION}_ubuntu_${TARGET_ARCH}.deb"
    echo "Moving headless build artifact to deploy/${artifact_1}"
    mv ../jamulus-headless*"_${TARGET_ARCH}.deb" "./deploy/${artifact_1}"
    echo "artifact_1=${artifact_1}" >> "$GITHUB_OUTPUT"

    local artifact_2="jamulus_${JAMULUS_BUILD_VERSION}_ubuntu_${TARGET_ARCH}.deb"
    echo "Moving regular build artifact to deploy/${artifact_2}"
    mv ../jamulus*_"${TARGET_ARCH}.deb" "./deploy/${artifact_2}"
    echo "artifact_2=${artifact_2}" >> "$GITHUB_OUTPUT"
}

case "${1:-}" in
    setup)
        setup
        ;;
    build)
        build_app_as_deb
        ;;
    get-artifacts)
        pass_artifacts_to_job
        ;;
    *)
        echo "Unknown stage '${1:-}'"
        exit 1
        ;;
esac
