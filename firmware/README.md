
## Build

### Fetch Dependencies

```bash
python3 ./fetch_repos.py
```

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Compile

```bash
idf.py build
```

### Flash

```bash
idf.py flash
```

### Launcher / StackChan UI assets

Asset lookup order is fixed across the StackChan ecosystem and should be kept as:
**Embedded -> LittleFS -> SD card**.

Embedded assets are generated at build time. Optional runtime overrides can come from:

- LittleFS (Launcher-managed; defaults: mount `/launcherfs`, label `userdata`)
- SD card (defaults: `/sdcard/assets`)

See `main/assets/assets_bin/README.md` for expected filenames and override behavior.

Run `idf.py reconfigure` after pulling; the `joltwallet/littlefs` component is required for optional LittleFS mounting.
