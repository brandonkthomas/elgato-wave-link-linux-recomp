# Development image for the Wave Link Linux port: Qt 6/QML UI, PipeWire engine, plugin hosting.
# Debian 13 ships Qt 6.8 and PipeWire 1.4. Built natively for fast iteration; release builds use
# --platform linux/amd64.
FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build pkg-config git ca-certificates curl file xz-utils \
      qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-shadertools-dev qt6-5compat-dev \
      qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
      qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
      qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtquick-shapes \
      qml6-module-qtquick-effects qml6-module-qtqml-workerscript qml6-module-qtquick-dialogs \
      qml6-module-qt5compat-graphicaleffects qml6-module-qt-labs-platform qml6-module-qtcore \
      libpipewire-0.3-dev libspa-0.2-dev pipewire pipewire-bin wireplumber pulseaudio-utils \
      liblilv-dev lv2-dev libusb-1.0-0-dev \
      libdbus-1-dev libkf6statusnotifieritem-dev liblayershellqtinterface-dev qt6-websockets-dev \
      rustc cargo \
      fonts-dejavu-core \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
