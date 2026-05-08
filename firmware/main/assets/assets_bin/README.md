# Launcher UI assets (optional overrides)

PNG / prebuilt LVGL `.bin` files placed here replace the compile-time placeholders in the embedded fallback.

Lookup order is fixed and should be kept ecosystem-wide:
**Embedded -> LittleFS -> SD card**.

Build copies them into `stackchan_launcher_assets.gen.c` via `gen_launcher_embedded_assets.py`. If a file is missing, a minimal placeholder is generated so the firmware still links and boots under Launcher.

Expected names (see `assets/tools/gen_launcher_embedded_assets.py`):

- `icon_*.bin`, `app_center_bg.png`, `setup_stackchan_front_view.bin`, etc.

## LittleFS (Launcher override layer)

When `CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS` is enabled, the device also looks for the same filenames under:

`CONFIG_STACKCHAN_LITTLEFS_MOUNT_PATH` + `/` + `CONFIG_STACKCHAN_LITTLEFS_ASSETS_SUBPATH` + `/` + `filename`

Default: `/launcherfs/assets/icon_home.bin`

The partition table uses a `userdata` **littlefs** slot (see root `partitions.csv`). Adjust `CONFIG_STACKCHAN_LITTLEFS_PARTITION_LABEL` if your Launcher image uses a different label.

## SD card (final override layer)

When `CONFIG_STACKCHAN_SD_UI_ASSETS` is enabled, the device also checks:

`CONFIG_STACKCHAN_SD_MOUNT_PATH` + `/` + `CONFIG_STACKCHAN_SD_ASSETS_SUBPATH` + `/` + `filename`

Default: `/sdcard/assets/icon_home.bin`
