# free (macOS)

Linux'taki `free` (procps-ng) komutunun macOS için yeniden yazımı. Tek dosyalık C programı, ek bağımlılık yok.

```
$ free -h
              total        used        free      shared  buff/cache   available
Mem:           64Gi        45Gi       2.4Gi          0B        17Gi        20Gi
Swap:            0B          0B          0B
```

## Kurulum

```bash
make
make test
sudo make install          # /usr/local/bin/free ve man sayfası
```

Başka bir yere kurmak için: `make install PREFIX=$HOME/.local`

## Seçenekler

| Seçenek | Açıklama |
|---|---|
| `-b`, `-k`, `-m`, `-g`, `--tebi`, `--pebi` | Bayt / KiB (varsayılan) / MiB / GiB / TiB / PiB |
| `--kilo`, `--mega`, `--giga`, `--tera`, `--peta` | 1000'lik birimler |
| `-h`, `--human` | İnsan okunur çıktı |
| `--si` | 1024 yerine 1000'lik katlar |
| `-w`, `--wide` | buffers ve cache ayrı sütunlarda |
| `-t`, `--total` | RAM + takas toplam satırı |
| `-L`, `--line` | Tek satırlık özet |
| `-d`, `--detail` | macOS dökümü: app, wired, compressed, file-backed, purgeable, speculative |
| `-s N`, `-c N` | N saniyede bir yenile / N kez yenile |
| `--help`, `-V` | Yardım / sürüm |

`-l` ve `-v` Linux'a özgüdür, macOS'ta hata verir.

## Sütunların macOS karşılıkları

| Sütun | Kaynak |
|---|---|
| total | `sysctl hw.memsize` |
| free | boş sayfalar, spekülatif hariç (`vm_stat` "Pages free") |
| buff/cache | dosya destekli + purgeable sayfalar (Etkinlik Monitörü "Önbelleğe Alınmış Dosyalar") |
| used | total − free − buff/cache (Linux formülü) |
| available | total − (app + wired + compressed), yani Etkinlik Monitörü "Kullanılan Bellek" değerinin tümleyeni |
| shared | macOS'ta karşılığı yok, 0 |
| buffers (`-w`) | macOS birleşik tampon önbelleği kullanır, 0 |
| Swap | `sysctl vm.swapusage` |
