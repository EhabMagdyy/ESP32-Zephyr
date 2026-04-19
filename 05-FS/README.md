## ESP32 Zephyr LittleFS

Quick reference for configuring a LittleFS mount in a Zephyr `.overlay` file & Functions used in the firmware.

---

## LittleFS Overlay Reference

### Full Example

```dts
/ {
    fstab {
        compatible = "zephyr,fstab";
        littlefs: littlefs {
            compatible     = "zephyr,fstab,littlefs";
            mount-point    = "/lfs";
            partition      = <&storage_partition>;
            automount;
            read-size      = <16>;
            prog-size      = <16>;
            cache-size     = <64>;
            lookahead-size = <32>;
            block-cycles   = <512>;
        };
    };
};
```

---

### Structure

#### `/  { }` — Root node
The Device Tree root. The overlay merges into the existing board DTS rather than replacing it.

#### `fstab { }` — Mount table container
**Name is fixed.** Zephyr's boot code specifically searches for a node named `fstab` under root to discover filesystems. Renaming it breaks auto-mounting entirely.

```dts
compatible = "zephyr,fstab";
```
Binds the node to Zephyr's fstab driver (`dts/bindings/fs/zephyr,fstab.yaml`). **Fixed string, do not change.**

---

### Mount Entry Node

```dts
lfs: lfs { ... }
```

| Part | What it is | Changeable? |
|------|-----------|-------------|
| `lfs:` | DTS **label** — used to reference this node elsewhere as `&lfs` | ✅ Yes |
| `lfs` | DTS **node name** — identifier in the tree | ✅ Yes |

Both are purely organisational and have no effect at runtime.

---

### Mount Entry Properties

#### `compatible = "zephyr,fstab,littlefs";`
**Fixed string.** Binds this entry to the LittleFS driver. Changing it selects a different filesystem (e.g. `"zephyr,fstab,fatfs"` for FAT).

---

#### `mount-point = "/lfs";`
The path prefix used in C code to access this filesystem.

```c
fs_open(&file, "/lfs/stm32.bin", FS_O_READ);
//             ^^^^^ must match mount-point
```

- ✅ Freely changeable (`"/storage"`, `"/data"`, etc.)
- Must start with `/`
- C code paths must use the same prefix

---

#### `partition = <&storage_partition>;`
Points to the flash partition used as storage backend. The label `storage_partition` must match a partition label defined in the flash layout:

```dts
storage_partition: partition@3D0000 {
    label = "storage";
    reg   = <0x3D0000 0x30000>;
};
```

- ✅ Label name is changeable — just keep it consistent between both places
- ❌ Must point to a valid, existing flash partition or mount fails at boot

---

#### `automount;`
Boolean flag (no value). Tells Zephyr to call `fs_mount()` automatically before `main()` runs.

**Without it** you must mount manually in C:

```c
static FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t mp = {
    .type      = FS_LITTLEFS,
    .fs_data   = &storage,
    .mnt_point = "/lfs",
};
fs_mount(&mp);
```

---

### LittleFS Tuning Parameters

All four values must be **powers of 2** and must reflect the actual flash hardware characteristics.

#### `read-size = <16>;`
Minimum bytes read from flash in a single operation.
- Set to match your flash chip's read granularity (typically 1–16 bytes)
- Too large → wastes RAM; too small → may not be supported by hardware

#### `prog-size = <16>;`
Minimum bytes written (programmed) to flash in a single operation.
- **Must match your flash chip's minimum write size** (commonly 4 or 16 bytes for NOR flash)
- Setting this smaller than hardware supports causes silent write failures or data corruption

#### `cache-size = <64>;`
Size of LittleFS's internal read/write cache in bytes.
- Must be a multiple of both `read-size` and `prog-size`
- Larger → better performance, more RAM consumed
- This RAM is **statically allocated** at boot

#### `lookahead-size = <32>;`
Size of the block allocator's lookahead buffer.
- Each byte represents 8 blocks → 32 bytes = 256 blocks lookahead
- Must be a multiple of 8
- Larger → faster free-block allocation, more RAM consumed

#### `block-cycles = <512>;`
Erase cycle threshold that triggers **wear leveling** on a block.
- Typical ESP32 internal flash endurance: 10,000–100,000 cycles
- Lower value → more aggressive wear leveling → longer flash lifetime
- Set to `-1` to disable wear leveling (not recommended for production)

---

### Quick-Reference Table

| Property | Fixed or changeable? | Effect if wrong |
|----------|---------------------|-----------------|
| `fstab` node name | **Fixed** | Zephyr won't find mount entries at boot |
| `lfs:` label | ✅ Freely changeable | None at runtime |
| `lfs` node name | ✅ Freely changeable | None at runtime |
| `compatible = "zephyr,fstab,littlefs"` | **Fixed** | Wrong or no driver bound |
| `mount-point` value | ✅ Freely changeable | C code paths must match |
| `partition` label ref | Must match flash partition label | Mount fails at boot |
| `automount` | Optional | Must mount manually in C |
| `read-size` / `prog-size` | Must match flash hardware | Data corruption |
| `cache-size` | Tunable | Performance / RAM trade-off |
| `lookahead-size` | Tunable | Allocation speed / RAM trade-off |
| `block-cycles` | Tunable | Flash wear leveling aggressiveness |

---

### Flash Partition (if your board has none)

Add to the same overlay file, before the `fstab` node:

```dts
&flash0 {
    partitions {
        compatible    = "fixed-partitions";
        #address-cells = <1>;
        #size-cells    = <1>;

        storage_partition: partition@3D0000 {
            label = "storage";
            reg   = <0x3D0000 0x30000>; /* 192 KB — adjust to not overlap your app */
        };
    };
};
```

---

## Zephyr LittleFS API Reference

### `fs_file_t_init`

```c
void fs_file_t_init(struct fs_file_t *zfp);
```

Initializes a file object to a known-zero state. **Must be called before every `fs_open`** — skipping it causes hard faults because internal pointers are garbage.

| Parameter | Description |
|-----------|-------------|
| `zfp` | Pointer to the `fs_file_t` object to initialize |

---

### `fs_open`

```c
int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags);
```

Opens or creates a file. Returns `0` on success, negative errno on failure.

| Parameter | Description |
|-----------|-------------|
| `zfp` | Pointer to an initialized `fs_file_t` object |
| `file_name` | Full path including mount prefix, e.g. `"/update/stm32.bin"` |
| `flags` | One or more of the flags below combined with `\|` |

**Flags:**

| Flag | Meaning |
|------|---------|
| `FS_O_READ` | Open for reading |
| `FS_O_WRITE` | Open for writing |
| `FS_O_RDWR` | Open for reading and writing |
| `FS_O_CREATE` | Create the file if it does not exist |
| `FS_O_TRUNC` | Truncate (clear) the file on open |
| `FS_O_APPEND` | All writes go to the end of the file |

---

### `fs_write`

```c
ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size);
```

Writes `size` bytes from `ptr` into the file at the current position. Returns the number of bytes actually written, or a negative errno on failure. Can be called repeatedly on the same open file to append data chunk by chunk.

| Parameter | Description |
|-----------|-------------|
| `zfp` | Pointer to an open `fs_file_t` |
| `ptr` | Pointer to the data buffer to write from |
| `size` | Number of bytes to write |

---

### `fs_read`

```c
ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size);
```

Reads up to `size` bytes from the current file position into `ptr`. Returns the number of bytes actually read (may be less than `size` at end of file), or a negative errno on failure. Returns `0` when the end of file is reached.

| Parameter | Description |
|-----------|-------------|
| `zfp` | Pointer to an open `fs_file_t` |
| `ptr` | Pointer to the buffer to read into |
| `size` | Maximum number of bytes to read |

---

### `fs_close`

```c
int fs_close(struct fs_file_t *zfp);
```

Flushes any pending writes and closes the file. **Always call this** when done — not closing a file can leave data unwritten to flash. Returns `0` on success, negative errno on failure.

| Parameter | Description |
|-----------|-------------|
| `zfp` | Pointer to the open `fs_file_t` to close |

---

### `fs_stat`

```c
int fs_stat(const char *path, struct fs_dirent *entry);
```

Retrieves metadata about a file or directory without opening it. Returns `0` on success, negative errno if the path does not exist or on failure.

| Parameter | Description |
|-----------|-------------|
| `path` | Full path to the file or directory |
| `entry` | Pointer to a `fs_dirent` struct that receives the result |

**`fs_dirent` fields populated after the call:**

| Field | Type | Description |
|-------|------|-------------|
| `entry.name` | `char[]` | File or directory name (not full path) |
| `entry.size` | `size_t` | File size in bytes (`0` for directories) |
| `entry.type` | `enum` | `FS_DIR_ENTRY_FILE` or `FS_DIR_ENTRY_DIR` |

---

### `fs_unlink`

```c
int fs_unlink(const char *path);
```

Deletes a file. The file must not be open when this is called. Returns `0` on success, negative errno on failure.

| Parameter | Description |
|-----------|-------------|
| `path` | Full path to the file to delete |

---

### Return Value Convention

All functions follow the same Zephyr convention:

| Return value | Meaning |
|---|---|
| `0` | Success |
| Negative integer | Failure — value is a negated `errno` code (e.g. `-ENOENT`, `-EIO`) |
| Positive integer | Success with a count (`fs_read`, `fs_write` return bytes transferred) |

---

## Build & Flash for ESP32

### Source
```sh
source /home/ehab/zephyrproject/zephyr/.venv/bin/activate
```

### Build
``` bash
west build -p always -b esp32_devkitc/esp32/procpu . --extra-dtc-overlay board/esp32.overlay -DPython3_EXECUTABLE=/home/ehab/zephyrproject/.venv/bin/python3
```

### Flash
``` bash
west flash --esp-device /dev/ttyUSB0
```

### Monitor
``` sh
pip install esp-idf-monitor
python -m esp_idf_monitor --port /dev/ttyUSB0 --baud 115200 build/zephyr/zephyr.elf
## to terminate
## ctrl + ]
```
