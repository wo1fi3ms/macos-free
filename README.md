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

Download `free-macos-universal.tar.gz` from the
[latest release](https://github.com/wo1fi3ms/macos-free/releases/latest).
The binary is universal (Apple Silicon and Intel) and needs macOS 11 or later.

```bash
curl -LO https://github.com/wo1fi3ms/macos-free/releases/latest/download/free-macos-universal.tar.gz
gh attestation verify free-macos-universal.tar.gz --repo wo1fi3ms/macos-free
tar -xzf free-macos-universal.tar.gz
mkdir -p ~/.local/bin ~/.local/share/man/man1
cp free-macos-universal/free ~/.local/bin/ && cp free-macos-universal/free.1 ~/.local/share/man/man1/
```

#### Verifying the binary

Release binaries are never uploaded by hand. A GitHub Actions workflow
([`release.yml`](.github/workflows/release.yml)) builds them from the tagged
commit and signs a [build provenance attestation](https://docs.github.com/en/actions/security-for-github-actions/using-artifact-attestations)
with Sigstore. `gh attestation verify` (GitHub CLI) checks that the archive was
built by that workflow in this repository; it fails for any file built
elsewhere. Releases are immutable: once published, their files and tag cannot
be changed.

If you do not want to trust any prebuilt binary, build from source (below);
it is a single C file.

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
