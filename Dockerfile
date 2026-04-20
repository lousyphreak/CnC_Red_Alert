# syntax=docker/dockerfile:1.7

ARG EMSCRIPTEN_IMAGE=emscripten/emsdk:4.0.14
ARG DEBIAN_IMAGE=debian:bookworm-slim
ARG ZIG_VERSION=0.15.2

FROM ${EMSCRIPTEN_IMAGE} AS emscripten-build

WORKDIR /src

RUN python3 -m pip install --no-cache-dir "cmake>=3.24,<4" ninja

COPY . .

RUN emcmake cmake -S . -B /tmp/build-emscripten -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DRA_ENABLE_UDP=OFF \
        -DRA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA=ON \
        -DRA_EMSCRIPTEN_PACKAGE_GAMEDATA=OFF \
    && cmake --build /tmp/build-emscripten --target redalert -j"$(nproc)"


FROM emscripten-build AS emscripten-gamedata

RUN python3 docker/filter_emscripten_gamedata.py \
        --manifest /tmp/build-emscripten/ra-assets-manifest.txt \
        --source-root /src/GameData \
        --output-root /tmp/deploy-root/GameData


FROM ${DEBIAN_IMAGE} AS zig-build

ARG ZIG_VERSION

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

RUN curl -fsSL "https://ziglang.org/download/${ZIG_VERSION}/zig-x86_64-linux-${ZIG_VERSION}.tar.xz" -o zig.tar.xz \
    && mkdir -p /opt/zig \
    && tar -xJf zig.tar.xz --strip-components=1 -C /opt/zig

WORKDIR /src/server

COPY server/build.zig server/build.zig.zon ./
COPY server/src ./src

RUN /opt/zig/zig build -Doptimize=ReleaseSafe


FROM ${DEBIAN_IMAGE} AS web-runtime-base

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /srv

COPY --from=zig-build /src/server/zig-out/bin/ra-wol-server /usr/local/bin/ra-wol-server

EXPOSE 80

HEALTHCHECK CMD wget -q -O /dev/null http://127.0.0.1/healthz || exit 1

ENTRYPOINT ["/usr/local/bin/ra-wol-server"]
CMD ["--host", "0.0.0.0", "--port", "80", "--emscripten-dir", "/srv/web"]


FROM web-runtime-base AS web-runtime

COPY --from=emscripten-build /tmp/build-emscripten/redalert.html /srv/web/redalert.html
COPY --from=emscripten-build /tmp/build-emscripten/redalert.js /srv/web/redalert.js
COPY --from=emscripten-build /tmp/build-emscripten/redalert.wasm /srv/web/redalert.wasm


FROM web-runtime-base AS web-runtime-with-gamedata

COPY --from=emscripten-gamedata /tmp/deploy-root/GameData /srv/GameData
COPY --from=emscripten-build /tmp/build-emscripten/redalert.html /srv/web/redalert.html
COPY --from=emscripten-build /tmp/build-emscripten/redalert.js /srv/web/redalert.js
COPY --from=emscripten-build /tmp/build-emscripten/redalert.wasm /srv/web/redalert.wasm

CMD ["--host", "0.0.0.0", "--port", "80", "--gamedata", "/srv/GameData", "--emscripten-dir", "/srv/web"]
