# Launcher UI assets (optional overrides)

PNG / prebuilt LVGL `.bin` files placed here replace the compile-time placeholders in the embedded fallback.

Build copies them into `stackchan_launcher_assets.gen.c` via `gen_launcher_embedded_assets.py`. If a file is missing, a minimal placeholder is generated so the firmware still links and boots under Launcher.

Expected names (see `assets/tools/gen_launcher_embedded_assets.py`):

- `icon_*.bin`, `app_center_bg.png`, `setup_stackchan_front_view.bin`, etc.
- `index.html` — mBot / WEB.REMOTE UI (embedded in flash as fallback; SD/LittleFS copy overrides at runtime if present)

Place `index.html` on SD/LittleFS to override the flash copy, e.g. `/sdcard/assets/index.html`. Source: `assets/assets_bin/index.html`.

### PNG ↔ LVGL `.bin`

From this directory (`pip install pillow` once):

```bash
python convert.py icon_dance.png              # -> icon_dance.bin
python convert.py icon_dance.bin              # -> icon_dance.png
python convert.py icon_dance.png -o out.bin   # custom output path
python convert.py icon_dance.bin --to png -o preview.png
```

Format: LVGL v9 header + **RGB565** or **RGB565A8** (LVGL image converter output). PNGs with transparency become RGB565A8; opaque PNGs become RGB565.

## LittleFS (Launcher)

When `CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS` is enabled, the device also looks for the same filenames under:

`CONFIG_STACKCHAN_LITTLEFS_MOUNT_PATH` + `/` + `CONFIG_STACKCHAN_LITTLEFS_ASSETS_SUBPATH` + `/` + `filename`

Default: `/launcherfs/assets/icon_home.bin`

The partition table uses a `userdata` **littlefs** slot (see root `partitions.csv`). Adjust `CONFIG_STACKCHAN_LITTLEFS_PARTITION_LABEL` if your Launcher image uses a different label.
