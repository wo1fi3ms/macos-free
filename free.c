/*
 * free — macOS için Linux (procps-ng) `free` komutunun yeniden yazımı.
 * Bellek bilgisi Mach VM istatistiklerinden (host_statistics64) ve
 * sysctl (hw.memsize, vm.swapusage) değerlerinden okunur.
 *
 * free — a macOS reimplementation of the Linux (procps-ng) `free` command.
 * Memory figures come from Mach VM statistics (host_statistics64) and
 * sysctl (hw.memsize, vm.swapusage).
 */

#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

#include <mach/mach.h>
#include <mach/mach_host.h>

#define FREE_VERSION "1.0.2"

/* Bellek ve takas değerleri, bayt cinsinden / Memory and swap figures, in bytes */
struct mem_info {
    uint64_t total;
    uint64_t used;
    uint64_t free;
    uint64_t shared;
    uint64_t buffers;
    uint64_t cache;
    uint64_t available;

    /* macOS'a özgü ayrıntılar (-d) / macOS-specific breakdown (-d) */
    uint64_t app;
    uint64_t wired;
    uint64_t compressed;
    uint64_t file_backed;
    uint64_t purgeable;
    uint64_t speculative;

    uint64_t swap_total;
    uint64_t swap_used;
    uint64_t swap_free;
};

/* Komut satırı seçenekleri / Command-line options */
struct options {
    uint64_t unit;     /* bölen; 0 = insan okunur / divisor; 0 = human-readable */
    int si;            /* 1000'lik birimler / powers of 1000 */
    int human;
    int show_total;
    int wide;
    int line;
    int detail;
    double seconds;    /* yenileme aralığı / refresh interval */
    long count;        /* tekrar sayısı, -1 = sonsuz / repeat count, -1 = forever */
    int repeat;
};

static const char *prog = "free";

/* Mach ve sysctl'den bellek bilgisini toplar / Collects memory info from Mach and sysctl */
static int read_mem_info(struct mem_info *mi)
{
    memset(mi, 0, sizeof(*mi));

    uint64_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, NULL, 0) != 0) {
        fprintf(stderr, "%s: cannot read hw.memsize: %s\n", prog, strerror(errno));
        return -1;
    }

    mach_port_t host = mach_host_self();
    vm_size_t page_size = 0;
    if (host_page_size(host, &page_size) != KERN_SUCCESS)
        page_size = (vm_size_t)sysconf(_SC_PAGESIZE);

    vm_statistics64_data_t vm;
    mach_msg_type_number_t cnt = HOST_VM_INFO64_COUNT;
    kern_return_t kr = host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &cnt);
    mach_port_deallocate(mach_task_self(), host);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "%s: host_statistics64 failed: %s\n", prog, mach_error_string(kr));
        return -1;
    }

    uint64_t ps = (uint64_t)page_size;
    uint64_t free_pages = vm.free_count > vm.speculative_count
                              ? (uint64_t)(vm.free_count - vm.speculative_count)
                              : 0;
    uint64_t internal = (uint64_t)vm.internal_page_count;
    uint64_t purgeable = (uint64_t)vm.purgeable_count;

    mi->total = memsize;
    mi->wired = (uint64_t)vm.wire_count * ps;
    mi->compressed = (uint64_t)vm.compressor_page_count * ps;
    mi->app = (internal > purgeable ? internal - purgeable : 0) * ps;
    mi->file_backed = (uint64_t)vm.external_page_count * ps;
    mi->purgeable = purgeable * ps;
    mi->speculative = (uint64_t)vm.speculative_count * ps;

    /*
     * Linux sütunlarına eşleme:
     *   free       = boş sayfalar (spekülatif hariç)
     *   buff/cache = dosya destekli + temizlenebilir (purgeable) sayfalar
     *   used       = total - free - buff/cache (Linux formülü)
     *   available  = total - (uygulama + kablolu + sıkıştırılmış),
     *                yani Etkinlik Monitörü'ndeki "Kullanılan Bellek"in tümleyeni
     *   shared     = macOS'ta karşılığı yok, 0
     *   buffers    = macOS birleşik tampon önbelleği kullanır, ayrı tampon yok, 0
     *
     * Mapping to Linux columns:
     *   free       = free pages (excluding speculative)
     *   buff/cache = file-backed + purgeable pages
     *   used       = total - free - buff/cache (Linux formula)
     *   available  = total - (app + wired + compressed), the complement of
     *                Activity Monitor's "Memory Used"
     *   shared     = no macOS equivalent, 0
     *   buffers    = macOS has a unified buffer cache, no separate buffers, 0
     */
    mi->free = free_pages * ps;
    mi->buffers = 0;
    mi->cache = mi->file_backed + mi->purgeable;
    mi->shared = 0;

    uint64_t taken = mi->free + mi->cache;
    mi->used = mi->total > taken ? mi->total - taken : 0;

    uint64_t mem_used = mi->app + mi->wired + mi->compressed;
    mi->available = mi->total > mem_used ? mi->total - mem_used : 0;

    struct xsw_usage sw;
    len = sizeof(sw);
    if (sysctlbyname("vm.swapusage", &sw, &len, NULL, 0) == 0) {
        mi->swap_total = sw.xsu_total;
        mi->swap_used = sw.xsu_used;
        mi->swap_free = sw.xsu_avail;
    }
    return 0;
}

/* Değeri seçilen birime göre biçimlendirir / Formats a value in the chosen unit */
static void format_value(char *buf, size_t n, uint64_t bytes, const struct options *o)
{
    if (!o->human) {
        snprintf(buf, n, "%llu", (unsigned long long)(bytes / o->unit));
        return;
    }

    /* procps-ng ile aynı biçim: B, Ki, Mi, Gi... ya da --si ile B, K, M, G...
     * Same style as procps-ng: B, Ki, Mi, Gi... or with --si B, K, M, G... */
    static const char suffixes[] = "BKMGTPE";
    double base = o->si ? 1000.0 : 1024.0;
    double v = (double)bytes;
    int i = 0;
    while (v >= base && i < (int)sizeof(suffixes) - 2) {
        v /= base;
        i++;
    }
    if (i == 0) {
        snprintf(buf, n, "%lluB", (unsigned long long)bytes);
        return;
    }
    const char *bin = o->si ? "" : "i";
    /* 9.95 ve üstü "10.0" yerine "10" olarak basılır / 9.95 and up prints as "10", not "10.0" */
    if (v < 9.95)
        snprintf(buf, n, "%.1f%c%s", v, suffixes[i], bin);
    else
        snprintf(buf, n, "%.0f%c%s", v, suffixes[i], bin);
}

/* Bir değeri sağa yaslı sütun olarak basar / Prints one value as a right-aligned column */
static void put_col(uint64_t bytes, const struct options *o)
{
    char buf[64];
    format_value(buf, sizeof(buf), bytes, o);
    printf(" %11s", buf);
}

static void print_table(const struct mem_info *mi, const struct options *o)
{
    if (o->line) {
        char sw[64], ca[64], mu[64], mf[64];
        format_value(sw, sizeof(sw), mi->swap_used, o);
        format_value(ca, sizeof(ca), mi->buffers + mi->cache, o);
        format_value(mu, sizeof(mu), mi->used, o);
        format_value(mf, sizeof(mf), mi->free, o);
        printf("SwapUse %11s CachUse %11s  MemUse %11s MemFree %11s\n", sw, ca, mu, mf);
        return;
    }

    if (o->wide)
        printf("       %12s%12s%12s%12s%12s%12s%12s\n",
               "total", "used", "free", "shared", "buffers", "cache", "available");
    else
        printf("       %12s%12s%12s%12s%12s%12s\n",
               "total", "used", "free", "shared", "buff/cache", "available");

    printf("%-7s", "Mem:");
    put_col(mi->total, o);
    put_col(mi->used, o);
    put_col(mi->free, o);
    put_col(mi->shared, o);
    if (o->wide) {
        put_col(mi->buffers, o);
        put_col(mi->cache, o);
    } else {
        put_col(mi->buffers + mi->cache, o);
    }
    put_col(mi->available, o);
    printf("\n");

    printf("%-7s", "Swap:");
    put_col(mi->swap_total, o);
    put_col(mi->swap_used, o);
    put_col(mi->swap_free, o);
    printf("\n");

    if (o->show_total) {
        printf("%-7s", "Total:");
        put_col(mi->total + mi->swap_total, o);
        put_col(mi->used + mi->swap_used, o);
        put_col(mi->free + mi->swap_free, o);
        printf("\n");
    }

    if (o->detail) {
        /* macOS'a özgü döküm, Etkinlik Monitörü terimleriyle
         * macOS-specific breakdown, in Activity Monitor terms */
        printf("\n       %12s%12s%12s%12s%12s%12s\n",
               "app", "wired", "compressed", "file-backed", "purgeable", "speculative");
        printf("%-7s", "Detail:");
        put_col(mi->app, o);
        put_col(mi->wired, o);
        put_col(mi->compressed, o);
        put_col(mi->file_backed, o);
        put_col(mi->purgeable, o);
        put_col(mi->speculative, o);
        printf("\n");
    }
}

static void usage(FILE *out)
{
    fprintf(out,
        "\nUsage:\n"
        " %s [options]\n"
        "\n"
        "Options:\n"
        " -b, --bytes         show output in bytes\n"
        "     --kilo          show output in kilobytes\n"
        "     --mega          show output in megabytes\n"
        "     --giga          show output in gigabytes\n"
        "     --tera          show output in terabytes\n"
        "     --peta          show output in petabytes\n"
        " -k, --kibi          show output in kibibytes (default)\n"
        " -m, --mebi          show output in mebibytes\n"
        " -g, --gibi          show output in gibibytes\n"
        "     --tebi          show output in tebibytes\n"
        "     --pebi          show output in pebibytes\n"
        " -h, --human         show human-readable output\n"
        "     --si            use powers of 1000 instead of 1024\n"
        " -w, --wide          wide output (separate buffers and cache)\n"
        " -t, --total         show total for RAM + swap\n"
        " -L, --line          show output on a single line\n"
        " -d, --detail        show macOS memory breakdown (app, wired, compressed...)\n"
        " -s N, --seconds N   repeat printing every N seconds\n"
        " -c N, --count N     repeat printing N times, then exit\n"
        "\n"
        "     --help          display this help and exit\n"
        " -V, --version       output version information and exit\n"
        "\n"
        "Note: -l/--lohi and -v/--committed are Linux-specific and not supported on macOS.\n",
        prog);
}

/* Pozitif sayı ayrıştırır / Parses a positive number */
static int parse_double(const char *s, double *out)
{
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || end == s || *end != '\0' || !(v > 0.0) || !isfinite(v))
        return -1;
    *out = v;
    return 0;
}

static int parse_long(const char *s, long *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || v < 1)
        return -1;
    *out = v;
    return 0;
}

static void sleep_seconds(double s)
{
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
}

enum {
    OPT_KILO = 256, OPT_MEGA, OPT_GIGA, OPT_TERA, OPT_PETA,
    OPT_TEBI, OPT_PEBI, OPT_SI, OPT_HELP
};

int main(int argc, char **argv)
{
    const char *slash = strrchr(argv[0], '/');
    prog = slash ? slash + 1 : argv[0];

    struct options o = {
        .unit = 1024, .si = 0, .human = 0, .show_total = 0, .wide = 0,
        .line = 0, .detail = 0, .seconds = 1.0, .count = -1, .repeat = 0
    };
    /* Birim harfi; --si ile 1000'lik tabana çevrilir
     * Unit exponent; switched to base 1000 by --si */
    int exp = 1;
    int unit_is_si = 0;

    static const struct option longopts[] = {
        {"bytes",     no_argument,       NULL, 'b'},
        {"kilo",      no_argument,       NULL, OPT_KILO},
        {"mega",      no_argument,       NULL, OPT_MEGA},
        {"giga",      no_argument,       NULL, OPT_GIGA},
        {"tera",      no_argument,       NULL, OPT_TERA},
        {"peta",      no_argument,       NULL, OPT_PETA},
        {"kibi",      no_argument,       NULL, 'k'},
        {"mebi",      no_argument,       NULL, 'm'},
        {"gibi",      no_argument,       NULL, 'g'},
        {"tebi",      no_argument,       NULL, OPT_TEBI},
        {"pebi",      no_argument,       NULL, OPT_PEBI},
        {"human",     no_argument,       NULL, 'h'},
        {"si",        no_argument,       NULL, OPT_SI},
        {"wide",      no_argument,       NULL, 'w'},
        {"total",     no_argument,       NULL, 't'},
        {"line",      no_argument,       NULL, 'L'},
        {"detail",    no_argument,       NULL, 'd'},
        {"seconds",   required_argument, NULL, 's'},
        {"count",     required_argument, NULL, 'c'},
        {"lohi",      no_argument,       NULL, 'l'},
        {"committed", no_argument,       NULL, 'v'},
        {"help",      no_argument,       NULL, OPT_HELP},
        {"version",   no_argument,       NULL, 'V'},
        {NULL, 0, NULL, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "bkmghwtLds:c:lvV", longopts, NULL)) != -1) {
        switch (c) {
        case 'b': exp = 0; unit_is_si = 0; break;
        case 'k': exp = 1; unit_is_si = 0; break;
        case 'm': exp = 2; unit_is_si = 0; break;
        case 'g': exp = 3; unit_is_si = 0; break;
        case OPT_TEBI: exp = 4; unit_is_si = 0; break;
        case OPT_PEBI: exp = 5; unit_is_si = 0; break;
        case OPT_KILO: exp = 1; unit_is_si = 1; break;
        case OPT_MEGA: exp = 2; unit_is_si = 1; break;
        case OPT_GIGA: exp = 3; unit_is_si = 1; break;
        case OPT_TERA: exp = 4; unit_is_si = 1; break;
        case OPT_PETA: exp = 5; unit_is_si = 1; break;
        case 'h': o.human = 1; break;
        case OPT_SI: o.si = 1; break;
        case 'w': o.wide = 1; break;
        case 't': o.show_total = 1; break;
        case 'L': o.line = 1; break;
        case 'd': o.detail = 1; break;
        case 's':
            if (parse_double(optarg, &o.seconds) != 0) {
                fprintf(stderr, "%s: invalid seconds value: %s\n", prog, optarg);
                return 1;
            }
            o.repeat = 1;
            break;
        case 'c':
            if (parse_long(optarg, &o.count) != 0) {
                fprintf(stderr, "%s: invalid count: %s\n", prog, optarg);
                return 1;
            }
            o.repeat = 1;
            break;
        case 'l':
        case 'v':
            fprintf(stderr, "%s: option -%c is Linux-specific and not supported on macOS\n", prog, c);
            return 1;
        case OPT_HELP:
            usage(stdout);
            return 0;
        case 'V':
            printf("%s (macOS) %s\n", prog, FREE_VERSION);
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "%s: unexpected argument: %s\n", prog, argv[optind]);
        usage(stderr);
        return 1;
    }

    /* --si, -k/-m/-g için de 1000 tabanını seçer (procps-ng ile aynı)
     * --si also selects base 1000 for -k/-m/-g (same as procps-ng) */
    uint64_t base = (unit_is_si || o.si) ? 1000 : 1024;
    o.unit = 1;
    for (int i = 0; i < exp; i++)
        o.unit *= base;

    setvbuf(stdout, NULL, _IOLBF, 0);

    for (long n = 0;; n++) {
        struct mem_info mi;
        if (read_mem_info(&mi) != 0)
            return 1;
        print_table(&mi, &o);

        if (!o.repeat)
            break;
        if (o.count > 0 && n + 1 >= o.count)
            break;
        if (!o.line)
            printf("\n");
        fflush(stdout);
        sleep_seconds(o.seconds);
    }
    return 0;
}
