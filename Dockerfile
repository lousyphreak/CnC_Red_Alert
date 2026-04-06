# syntax=docker/dockerfile:1.7

ARG EMSCRIPTEN_IMAGE=emscripten/emsdk:4.0.14
ARG NGINX_IMAGE=nginx:1.27-alpine

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


FROM ${NGINX_IMAGE} AS web-runtime

RUN apk add --no-cache openssl

COPY docker/nginx/redalert-web.conf /etc/nginx/conf.d/default.conf
COPY docker/nginx/10-configure-basic-auth.sh /docker-entrypoint.d/10-configure-basic-auth.sh
RUN chmod +x /docker-entrypoint.d/10-configure-basic-auth.sh
COPY --from=emscripten-build /tmp/build-emscripten/redalert.html /usr/share/nginx/html/redalert.html
COPY --from=emscripten-build /tmp/build-emscripten/redalert.js /usr/share/nginx/html/redalert.js
COPY --from=emscripten-build /tmp/build-emscripten/redalert.wasm /usr/share/nginx/html/redalert.wasm

EXPOSE 80

HEALTHCHECK CMD wget -q -O /dev/null http://127.0.0.1/healthz || exit 1


FROM web-runtime AS web-runtime-with-gamedata

COPY --from=emscripten-gamedata /tmp/deploy-root/GameData /usr/share/nginx/html/GameData
