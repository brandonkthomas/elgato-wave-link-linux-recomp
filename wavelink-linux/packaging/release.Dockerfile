# Release build for any x86-64 Linux with glibc >= 2.36: Qt 6.8 LTS official binaries,
# app + engine bundled into a relocatable directory (Qt and its plugins included; PipeWire and libusb
# come from the system).
#   (from the repository root; -j4 keeps emulated x86-64 compiles inside a 4 GB Docker VM)
#   docker buildx build --platform linux/amd64 -f wavelink-linux/packaging/release.Dockerfile \
#     --target export --output type=local,dest=wavelink-linux/dist .
# PriDump reads Wave Link's UI strings out of the installer's .pri files during install; published as a
# self-contained single file so users need no .NET SDK.
FROM --platform=linux/amd64 mcr.microsoft.com/dotnet/sdk:10.0 AS pridump
WORKDIR /xe
COPY tools/xaml-extract/src/PriDump /xe/src/PriDump
RUN git clone -q https://github.com/chausner/PriTools.git vendor/PriTools \
 && git -C vendor/PriTools checkout -q 765d4544fd48aaaebf848b5b1b24ade2cf1770a0 \
 && dotnet publish src/PriDump -c Release -r linux-x64 --self-contained -p:PublishSingleFile=true \
      -p:InvariantGlobalization=true -p:EnableCompressionInSingleFile=true -p:DebugType=none -o /out \
 && mv /out/PriDump /out/pridump

FROM --platform=linux/amd64 debian:bookworm AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build pkg-config git ca-certificates curl unzip file patchelf python3-pip python3-venv \
      libglib2.0-0 libpipewire-0.3-dev libusb-1.0-0-dev libgl-dev libegl-dev libxkbcommon-dev libxkbcommon-x11-dev \
      libfontconfig1-dev libfreetype-dev libdbus-1-dev libwayland-dev libxcb1-dev libxcb-cursor0 libxcb-icccm4 \
      libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 \
 && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/aqt && /opt/aqt/bin/pip install --no-cache-dir aqtinstall \
 && /opt/aqt/bin/aqt install-qt linux desktop 6.8.3 linux_gcc_64 -O /opt/Qt -m qtwebsockets qtshadertools qt5compat qtlottie qtwaylandcompositor >/dev/null \
 && ls /opt/Qt/6.8.3/gcc_64/lib | grep -c libQt6
# Noise suppression plugins behind Voice Focus (DeepFilterNet, MIT/Apache-2.0) and Elgato Noise Removal
# (noise-suppression-for-voice's RNNoise LADSPA, GPL-3.0).
RUN mkdir -p /plugins \
 && curl -sSfL -o /plugins/libdeep_filter_ladspa.so \
      https://github.com/Rikorose/DeepFilterNet/releases/download/v0.5.6/libdeep_filter_ladspa-0.5.6-x86_64-unknown-linux-gnu.so \
 && curl -sSfL -o /tmp/rnnoise.zip https://github.com/werman/noise-suppression-for-voice/releases/download/v1.10/linux-rnnoise.zip \
 && unzip -j /tmp/rnnoise.zip '*ladspa/librnnoise_ladspa.so' -d /plugins && ls -la /plugins
WORKDIR /src
COPY wave3-linux /src/wave3-linux
COPY wavelink-linux /src/wavelink-linux
RUN cmake -S wavelink-linux -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64 \
      -DCMAKE_INSTALL_PREFIX=/ -DBUILD_TESTING=OFF \
 && cmake --build build -j4 \
 && DESTDIR=/stage cmake --install build \
 && python3 wavelink-linux/packaging/bundle.py /stage /opt/Qt/6.8.3/gcc_64 /out/wavelink-linux \
 && mkdir -p /out/wavelink-linux/lib/wavelink/plugins && cp /plugins/*.so /out/wavelink-linux/lib/wavelink/plugins/ \
 && cp wavelink-linux/README.md wavelink-linux/packaging/install.sh wavelink-linux/packaging/uninstall.sh wavelink-linux/packaging/wavelinkd.service wave3-linux/packaging/70-elgato-wave3.rules /out/wavelink-linux/ \
 && cp -r wavelink-linux/packaging/desktop /out/wavelink-linux/desktop \
 && cp -r wavelink-linux/packaging/licenses /out/wavelink-linux/licenses \
 && mkdir -p /out/wavelink-linux/tools && cp wavelink-linux/tools/import-elgato-assets.sh /out/wavelink-linux/tools/
COPY --from=pridump /out/pridump /out/wavelink-linux/tools/pridump
RUN tar -C /out -czf /out/wavelink-linux-x86_64.tar.gz wavelink-linux

FROM scratch AS export
COPY --from=build /out/wavelink-linux-x86_64.tar.gz /
