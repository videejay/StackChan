
## Build

### Fetch Dependencies

```bash
python3 ./fetch_repos.py
```

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Build

```bash
idf.py build
```

### Flash

```bash
idf.py flash
```

### Launcher / StackChan UI assets

UI icons and backgrounds are loaded from **embedded fallbacks** (generated at build time) and optionally overridden from a **LittleFS** partition managed by [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) (defaults: mount `/launcherfs`, partition label `userdata`). See `main/assets/assets_bin/README.md`.

Run `idf.py reconfigure` after pulling � the `joltwallet/littlefs` component is required for optional LittleFS mounting.
