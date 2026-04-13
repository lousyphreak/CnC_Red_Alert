
# Command & Conquer Red Alert

This repository includes source code for Command & Conquer Red Alert. This release provides support to the [Steam Workshop](https://steamcommunity.com/workshop/browse/?appid=2229840) for the game.


## Dependencies

If you wish to rebuild the source code and tools successfully you will need to find or write new replacements (or remove the code using them entirely) for the following libraries;

- SDL3


## Compiling

The active port builds with CMake and SDL3. Use the top-level `CMakeLists.txt` together with the existing build trees (`build/`, `build-asan/`, `build-emscripten/`) or generate a fresh build directory for your target platform.


## Emscripten web container

The repository includes a multi-stage `Dockerfile` for the browser build.

Build the plain runtime image:

```sh
docker build --target web-runtime -t redalert-web:latest .
```

Build the runtime plus the filtered `GameData` layer:

```sh
docker build --target web-runtime-with-gamedata -t redalert-web-gamedata:latest .
```

The `web-runtime-with-gamedata` target copies the filtered `GameData` layer before the HTML/JS/wasm output so Docker can reuse the asset layer across binary rebuilds. It only carries browser-relevant game assets derived from `ra-assets-manifest.txt`; the Windows desktop-theme `GameData/RED ALERT/` extras and browser-local files such as savegames are intentionally excluded.


## Web container basic auth

The nginx image can enforce HTTP basic auth itself, independent of ingress behavior.

Enable it with a username and password:

```sh
docker run --rm -p 8080:80 \
  -e RA_BASIC_AUTH_USERNAME=redalert \
  -e RA_BASIC_AUTH_PASSWORD='change-me' \
  redalert-web-gamedata:latest
```

Or provide a precomputed htpasswd line directly:

```sh
docker run --rm -p 8080:80 \
  -e RA_BASIC_AUTH_HTPASSWD='redalert:$apr1$example$hashgoeshere' \
  redalert-web-gamedata:latest
```

If neither `RA_BASIC_AUTH_HTPASSWD` nor both `RA_BASIC_AUTH_USERNAME` and `RA_BASIC_AUTH_PASSWORD` are set, image-level auth stays disabled.

The Kubernetes manifests in `k8s/ea-games/` already pass the `redalert-web-basic-auth` secret into the container as `RA_BASIC_AUTH_HTPASSWD`. Replace the placeholder secret contents before any real deployment.

To use the compiled binaries, you must own the game. The C&C Ultimate Collection is available for purchase on [EA App](https://www.ea.com/en-gb/games/command-and-conquer/command-and-conquer-the-ultimate-collection/buy/pc) or [Steam](https://store.steampowered.com/bundle/39394/Command__Conquer_The_Ultimate_Collection/).


## Contributing

This repository will not be accepting contributions (pull requests, issues, etc). If you wish to create changes to the source code and encourage collaboration, please create a fork of the repository under your GitHub user/organization space.


## Support

This repository is for preservation purposes only and is archived without support. 


## License

This repository and its contents are licensed under the GPL v3 license, with additional terms applied. Please see [LICENSE.md](LICENSE.md) for details.
