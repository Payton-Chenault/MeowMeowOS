#include "../libs/meow_libc.h"

#define MODULE "PLAY"
DESCRIPTION("play.elf: Play uncompressed 16-bit stereo PCM WAV audio files");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);
extern void log_warning(const char *module, const char *fmt, ...);
extern void log_error(const char *module, const char *fmt, ...);

typedef struct __attribute__((packed)) {
    char riff_tag[4];     // "RIFF"
    uint32_t riff_size;
    char wave_tag[4];     // "WAVE"
} wave_header_t;

typedef struct __attribute__((packed)) {
    char chunk_id[4];     // "fmt "
    uint32_t chunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wave_fmt_chunk_t;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: play <filename.wav>\n");
        log_error(MODULE, "play: missing audio file argument");
        return 1;
    }

    const char *filename = argv[1];
    log_info(MODULE, "Opening WAV audio file '%s'...", filename);

    int fd = open(filename);
    if (fd < 0) {
        printf("play: cannot open '%s'\n", filename);
        log_error(MODULE, "play: failed to open file '%s'", filename);
        return 1;
    }

    wave_header_t header;
    if (read(fd, &header, sizeof(wave_header_t)) != sizeof(wave_header_t)) {
        printf("play: invalid file or premature EOF\n");
        close(fd);
        return 1;
    }

    if (strncmp(header.riff_tag, "RIFF", 4) != 0 || strncmp(header.wave_tag, "WAVE", 4) != 0) {
        printf("play: '%s' is not a valid RIFF/WAVE file\n", filename);
        log_error(MODULE, "play: invalid RIFF header signature");
        close(fd);
        return 1;
    }

    wave_fmt_chunk_t fmt;
    char chunk_hdr[8];
    uint32_t data_size = 0;

    // Scan chunks until finding "fmt " and "data"
    while (read(fd, chunk_hdr, 8) == 8) {
        uint32_t csize = *(uint32_t *)(chunk_hdr + 4);

        if (strncmp(chunk_hdr, "fmt ", 4) == 0) {
            lseek(fd, -8, SEEK_CUR);
            read(fd, &fmt, sizeof(wave_fmt_chunk_t));
            if (csize > sizeof(wave_fmt_chunk_t) - 8) {
                lseek(fd, csize - (sizeof(wave_fmt_chunk_t) - 8), SEEK_CUR);
            }
        } else if (strncmp(chunk_hdr, "data", 4) == 0) {
            data_size = csize;
            break;
        } else {
            lseek(fd, csize, SEEK_CUR);
        }
    }

    if (data_size == 0 || fmt.audio_format != 1) {
        printf("play: unsupported audio format (only uncompressed PCM is supported)\n");
        log_error(MODULE, "play: non-PCM or missing data chunk");
        close(fd);
        return 1;
    }

    printf("=====================================================\n");
    printf(" Playing Audio: %s\n", filename);
    printf(" Format:        %u-bit PCM, %s\n",
           fmt.bits_per_sample, (fmt.num_channels == 2) ? "Stereo" : "Mono");
    printf(" Sample Rate:   %u Hz\n", fmt.sample_rate);
    printf(" Size:          %u bytes\n", data_size);
    printf("=====================================================\n");

    uint8_t *audio_data = (uint8_t *)malloc(data_size);
    if (!audio_data) {
        printf("play: out of memory allocating audio buffer (%u bytes)\n", data_size);
        log_error(MODULE, "play: malloc(%u) failed", data_size);
        close(fd);
        return 1;
    }

    int bytes_read = read(fd, audio_data, data_size);
    close(fd);

    if (bytes_read <= 0) {
        printf("play: failed to read audio payload\n");
        free(audio_data);
        return 1;
    }

    sys_set_busy(true);
    int ret = sys_sound_play(audio_data, bytes_read, fmt.sample_rate, (uint8_t)fmt.num_channels, (uint8_t)fmt.bits_per_sample);
    sys_set_busy(false);

    free(audio_data);

    if (ret != 0) {
        printf("play: audio playback failed (code %d)\n", ret);
        log_error(MODULE, "play: sys_sound_play failed with code %d", ret);
        return 1;
    }

    printf("Playback finished.\n");
    log_info(MODULE, "Finished playback of '%s'", filename);
    return 0;
}