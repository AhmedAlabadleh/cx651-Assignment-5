#include "loader.h"
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

/* =========================================================
 *  mmap-based raw image I/O (Assignment 5, Parts 3-4)
 * ========================================================= */

/*
 * Loads an image from a raw image file using memory-mapped I/O.
 * File layout: [struct image header][pixel data]
 * Pixels pointer points into a READ-ONLY mapping.
 * Caller must munmap (not free) the region.
 */
int loadimage_mmap(char* filename, struct image* image) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return -1;

    /* Read header first to learn dimensions */
    struct image header;
    ssize_t got = read(fd, &header, sizeof(struct image));
    if (got != (ssize_t)sizeof(struct image)) { close(fd); return -1; }

    size_t pixel_bytes = (size_t)header.width * header.height * sizeof(struct pixel);
    size_t total_bytes = sizeof(struct image) + pixel_bytes;

    void* base = mmap(NULL, total_bytes, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) { close(fd); return -1; }

    struct image* hdr = (struct image*)base;
    image->width  = hdr->width;
    image->height = hdr->height;
    image->pixels = (struct pixel*)((char*)base + sizeof(struct image));

    close(fd);
    return 0;
}

/*
 * Saves an image to a raw image file using memory-mapped I/O.
 * File layout: [struct image header][pixel data]
 */
int saveimage_mmap(char* filename, struct image* image) {
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -1;

    size_t pixel_bytes = (size_t)image->width * image->height * sizeof(struct pixel);
    size_t total_bytes = sizeof(struct image) + pixel_bytes;

    if (ftruncate(fd, (off_t)total_bytes) != 0) { close(fd); return -1; }

    void* base = mmap(NULL, total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) { close(fd); return -1; }

    /* Copy header (with pixels pointer irrelevant on disk) */
    struct image header = *image;
    memcpy(base, &header, sizeof(struct image));
    memcpy((char*)base + sizeof(struct image), image->pixels, pixel_bytes);

    if (msync(base, total_bytes, MS_SYNC) != 0) {
        perror("msync");
    }

    munmap(base, total_bytes);
    close(fd);
    return 0;
}

/* =========================================================
 *  BMP image I/O (kept from the original starter)
 * ========================================================= */

int loadimage(char* filename, struct image* image) {
    int fd = open(filename, O_RDONLY);
    uint32_t x, y;
    BMPHeader header;
    BMPInfoHeader infoHeader;

    if (fd == -1) return -1;

    header.type = 0;

    read(fd, &header, sizeof(BMPHeader));
    read(fd, &infoHeader, sizeof(BMPInfoHeader));

    if (header.type != 0x4D42 || infoHeader.bits != 24) {
        printf("corrupted header\n");
        close(fd);
        return -1;
    }
    int padding = (4 - (infoHeader.width * 3) % 4) % 4;

    lseek(fd, header.offset, SEEK_SET);

    y = infoHeader.height - 1;

    image->pixels = malloc(sizeof(struct pixel) * image->width * image->height);

    do {
        for (x = 0; x < infoHeader.width; x++) {
            unsigned char color[3];
            read(fd, color, sizeof(unsigned char) * 3);
            image->pixels[x + y * image->width].r = color[0];
            image->pixels[x + y * image->width].g = color[1];
            image->pixels[x + y * image->width].b = color[2];
        }
        lseek(fd, padding, SEEK_CUR);
    } while (y-- > 0);

    close(fd);
    return 0;
}

int saveimage(char* filename, struct image* image) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int x, y;

    if (fd == -1) return 1;

    BMPHeader header = { 0x4D42, (uint32_t)(54 + image->width * image->height * 3), 0, 0, 54 };
    BMPInfoHeader infoHeader = { 40, (uint32_t)image->width, (uint32_t)image->height, 1, 24, 0,
                                 (uint32_t)(image->width * image->height * 3), 0, 0, 0, 0 };

    write(fd, &header, sizeof(BMPHeader));
    write(fd, &infoHeader, sizeof(BMPInfoHeader));

    int padding = (4 - (image->width * 3) % 4) % 4;

    y = image->height - 1;
    do {
        for (x = 0; x < image->width; x++) {
            struct pixel p = image->pixels[x + y * image->width];
            unsigned char color[3] = {
                (unsigned char)(((uint32_t) p.r) & 0xFF),
                (unsigned char)(((uint32_t) p.g) & 0xFF),
                (unsigned char)(((uint32_t) p.b) & 0xFF)
            };
            write(fd, color, sizeof(unsigned char) * 3);
        }
        for (int i = 0; i < padding; i++) {
            unsigned char pad = 0;
            write(fd, &pad, 1);
        }
    } while (y-- > 0);

    close(fd);
    return 0;
}