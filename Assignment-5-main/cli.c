#include "kernel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* Trigger a MAJOR page fault by writing to a large mmap'd file. */
int generate_pagefault() {
    const char* path = "fault.bin";
    const size_t SZ = 512UL * 1024 * 1024;  /* 512 MB */

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return -1; }

    if (ftruncate(fd, (off_t)SZ) != 0) { perror("ftruncate"); close(fd); return -1; }

    void* m = mmap(NULL, SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) { perror("mmap"); close(fd); return -1; }

    volatile char* p = (volatile char*)m;
    for (size_t off = 0; off < SZ; off += 4096) {
        p[off] = (char)(off & 0xFF);
    }

    msync(m, SZ, MS_SYNC);
    munmap(m, SZ);
    close(fd);
    unlink(path);
    return 0;
}

/* Helper: munmap a mapping returned by loadimage_mmap */
static void unmap_image(struct image* img) {
    if (!img || !img->pixels) return;
    size_t total = sizeof(struct image) +
                   (size_t)img->width * img->height * sizeof(struct pixel);
    munmap((void*)((char*)img->pixels - sizeof(struct image)), total);
}

int main(int argc, char** argv) {
    if (argc != 6) {
        printf("Incorrect number of arguments. Expected: ./cli <MODE=kernel|mmap|convert|uconvert|fault> <input_image> <width> <height> <output_image_path>\n");
        return -1;
    }

    const char* mode    = argv[1];
    const char* infile  = argv[2];
    int width           = atoi(argv[3]);
    int height          = atoi(argv[4]);
    const char* outfile = argv[5];

    int kernel[3][3] = {{1,1,1},{1,1,1},{1,1,1}};

    /* ---------- fault ---------- */
    if (strcmp(mode, "fault") == 0) {
        return generate_pagefault();
    }

    /* ---------- convert: BMP -> mmap binary ---------- */
    if (strcmp(mode, "convert") == 0) {
        struct image* img = malloc(sizeof(struct image));
        img->width  = width;
        img->height = height;
        img->pixels = malloc(sizeof(struct pixel) * (size_t)width * height);
        if (loadimage((char*)infile, img) != 0) {
            printf("loadimage failed\n");
            free(img->pixels);
            free(img);
            return -1;
        }

        saveimage_mmap((char*)outfile, img);

        /* free the pixels array BEFORE freeing the struct */
        free(img->pixels);
        free(img);
        return 0;
    }

    /* ---------- uconvert: mmap binary -> BMP ---------- */
    if (strcmp(mode, "uconvert") == 0) {
        struct image* img = malloc(sizeof(struct image));
        img->width  = width;
        img->height = height;
        img->pixels = NULL;
        if (loadimage_mmap((char*)infile, img) != 0) {
            printf("loadimage_mmap failed\n");
            free(img);
            return -1;
        }

        saveimage((char*)outfile, img);

        /* munmap the read-only region, then free the struct */
        unmap_image(img);
        free(img);
        return 0;
    }

    /* ---------- mmap: kernel on mmap'd input ---------- */
    if (strcmp(mode, "mmap") == 0) {
        struct image* img = malloc(sizeof(struct image));
        img->width  = width;
        img->height = height;
        img->pixels = NULL;
        if (loadimage_mmap((char*)infile, img) != 0) {
            printf("loadimage_mmap failed\n");
            free(img);
            return -1;
        }

        struct image* out = apply_kernel(img, (int*)kernel, 3, 1.0f / 9.0f);
        saveimage_mmap((char*)outfile, out);

        /* out was malloc'd internally by apply_kernel */
        free(out->pixels);
        free(out);

        /* img points into the mmap region */
        unmap_image(img);
        free(img);
        return 0;
    }

    /* ---------- kernel: BMP input ---------- */
    if (strcmp(mode, "kernel") == 0) {
        struct image* img = malloc(sizeof(struct image));
        img->width  = width;
        img->height = height;
        img->pixels = malloc(sizeof(struct pixel) * (size_t)width * height);
        if (loadimage((char*)infile, img) != 0) {
            printf("loadimage failed\n");
            free(img->pixels);
            free(img);
            return -1;
        }

        struct image* out = apply_kernel(img, (int*)kernel, 3, 1.0f / 9.0f);
        saveimage((char*)outfile, out);

        free(out->pixels);
        free(out);

        free(img->pixels);
        free(img);
        return 0;
    }

    printf("Unknown mode: %s\n", mode);
    return -1;
}