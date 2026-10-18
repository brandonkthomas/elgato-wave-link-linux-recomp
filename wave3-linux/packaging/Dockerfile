# Reproducible Linux x86-64 build: compiles wave3ctl on Debian 12, runs the protocol tests and
# exports the binary.
#   docker buildx build --platform linux/amd64 -f packaging/Dockerfile \
#     --target export --output type=local,dest=dist/linux-x86_64 .
FROM --platform=linux/amd64 debian:bookworm AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake make pkg-config libusb-1.0-0-dev file \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j"$(nproc)" \
 && ctest --test-dir build --output-on-failure \
 && file build/wave3ctl && ldd build/wave3ctl

FROM scratch AS export
COPY --from=build /src/build/wave3ctl /wave3ctl
