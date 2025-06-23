### Small File System 🗄️

A minimal user-space file system that stores files on top of a raw **block device stub** (provided as `TBlkDev`).  
The project was developed for the *Operating Systems* course to demonstrate:

* on-disk data structures & serialization
* allocation of free blocks via a bitmap
* file-descriptor table and sequential I/O
* mount / un-mount semantics and persistence across “reboots”

---

## Key specs

| Property | Value / Notes |
|----------|---------------|
| **Block size** | 512 B |
| **Device size** | 8 MiB – 1 GiB (`DEVICE_SIZE_MIN … MAX`) |
| **Max files** | `DIR_ENTRIES_MAX` |
| **Max open files** | `OPEN_FILES_MAX` |
| **Filename length** | ≤ `FILENAME_LEN_MAX` (no directory hierarchy) |
| **Metadata budget** | ≤ 10 % of the device capacity |

### On-disk layout

```
| Boot / FS head | Bitmap of free sectors | Directory table | Data blocks |
```

* `FileSysHead` – magic, block counts, pointers  
* Bitmap – 1 bit / block (0 = free, 1 = used)  
* Directory – fixed array of `<filename, size, firstBlock>`  
* Data blocks – singly indirect if file grows beyond one block

---

## Build & run

```bash
cd mini-FileSystem
make                       # builds lib & sample_tester

# format a 32-MiB “device” and run demo tests
./sample_tester 32
```

> **Note:** the project is self-contained, no STL; only `<cstdio>`, `<cstring>`, `<cstdlib>`.

---

## Implemented API (excerpt)

```cpp
// formatting
bool createFs ( TBlkDev dev );

// life-cycle
std::unique_ptr<CFileSystem> mount  ( TBlkDev dev );
bool                        umount ( void );

// file operations
int  openFile  ( const char *name, bool writeMode );
size_t readFile  ( int fd, void *dst, size_t len );
size_t writeFile ( int fd, const void *src, size_t len );
bool closeFile ( int fd );
bool deleteFile( const char *name );
size_t fileSize ( const char *name );

// directory listing
bool findFirst ( TFile &info );
bool findNext  ( TFile &info );
```

Unit tests cover:

1. **Short files** – create, list, read contents  
2. **Deletes & re-use** of freed space  
3. **Large files** (> 4 KiB) with indirect blocks  
4. **Capacity** check: ≥ 90 % of device can be filled  
5. **Mount after reboot** – data survive unmount  
6. **Multiple descriptors** (≤ `OPEN_FILES_MAX`)  

---

Feel free to open issues if you spot a bug or have improvement ideas!
