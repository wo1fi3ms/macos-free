# free (macOS)

A macOS reimplementation of the Linux `free` command (procps-ng). A single C file with no dependencies.

```
$ free -h
              total        used        free      shared  buff/cache   available
Mem:           64Gi        45Gi       2.4Gi          0B        17Gi        20Gi
Swap:            0B          0B          0B
```

## Installation

### Prebuilt binary

Download `free-<version>-macos-universal.tar.gz` from the
[latest release](https://github.com/wo1fi3ms/macos-free/releases/latest).
The binary is universal (Apple Silicon and Intel) and needs macOS 11 or later.

```bash
curl -LO https://github.com/wo1fi3ms/macos-free/releases/download/v1.0.0/free-1.0.0-macos-universal.tar.gz
tar -xzf free-1.0.0-macos-universal.tar.gz
cd free-1.0.0-macos-universal
mkdir -p ~/.local/bin ~/.local/share/man/man1
cp free ~/.local/bin/ && cp free.1 ~/.local/share/man/man1/
```

The binary is ad-hoc signed but not notarized. If you downloaded the archive
with a web browser, macOS may refuse to run it; remove the quarantine flag once:

```bash
xattr -d com.apple.quarantine ~/.local/bin/free
```

### From source

Requires the Xcode Command Line Tools (`xcode-select --install`).

```bash
make
make test
sudo make install          # installs /usr/local/bin/free and the man page
```

To install without `sudo`, for example into `~/.local`:

```bash
make install PREFIX=$HOME/.local
```

Make sure `$PREFIX/bin` is on your `PATH`.

## Options

| Option | Description |
|---|---|
| `-b`, `-k`, `-m`, `-g`, `--tebi`, `--pebi` | Bytes / KiB (default) / MiB / GiB / TiB / PiB |
| `--kilo`, `--mega`, `--giga`, `--tera`, `--peta` | Powers of 1000 |
| `-h`, `--human` | Human-readable output |
| `--si` | Use powers of 1000 instead of 1024 |
| `-w`, `--wide` | Show buffers and cache in separate columns |
| `-t`, `--total` | Add a RAM + swap total line |
| `-L`, `--line` | One-line summary |
| `-d`, `--detail` | macOS breakdown: app, wired, compressed, file-backed, purgeable, speculative |
| `-s N`, `-c N` | Refresh every N seconds / repeat N times |
| `--help`, `-V` | Help / version |

`-l` and `-v` are Linux-specific and exit with an error on macOS.

## How the columns map to macOS

| Column | Source |
|---|---|
| total | `sysctl hw.memsize` |
| free | free pages, excluding speculative ones (`vm_stat` "Pages free") |
| buff/cache | file-backed + purgeable pages (Activity Monitor "Cached Files") |
| used | total − free − buff/cache (the Linux formula) |
| available | total − (app + wired + compressed), the complement of Activity Monitor's "Memory Used" |
| shared | no macOS equivalent, always 0 |
| buffers (`-w`) | macOS uses a unified buffer cache, always 0 |
| Swap | `sysctl vm.swapusage` |

## License

[MIT](LICENSE)
