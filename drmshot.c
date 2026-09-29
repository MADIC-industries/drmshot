// drmshot: dump a DRM/KMS CRTC's current framebuffer to a BMP file.
//
// Usage: drmshot [-c <crtc-id>] [-o <output.bmp>] [-d </dev/dri/cardN>]
// With no -c, dumps the first CRTC that has an active framebuffer.

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

static void write_bmp(char const *path, uint8_t const *src, uint32_t width,
                      uint32_t height, uint32_t pitch, uint32_t bpp) {
  if ((bpp != 32) && (bpp != 24)) {
    fprintf(stderr, "drmshot: unsupported bpp=%u (only 24/32 handled)\n", bpp);
    exit(1);
  }

  uint32_t const row_bytes = width * 3;
  uint32_t const row_padded = (row_bytes + 3u) & ~3u;
  uint32_t const pixel_bytes = bpp / 8u;
  uint32_t const data_size = row_padded * height;

  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    fprintf(stderr, "drmshot: fopen(%s): %s\n", path, strerror(errno));
    exit(1);
  }

  uint32_t const file_size = 14 + 40 + data_size;
  uint8_t bmp_header[14] = {
      'B',
      'M',
      (uint8_t)(file_size),
      (uint8_t)(file_size >> 8),
      (uint8_t)(file_size >> 16),
      (uint8_t)(file_size >> 24),
      0,
      0,
      0,
      0,
      54,
      0,
      0,
      0,
  };
  uint8_t dib_header[40] = {0};
  dib_header[0] = 40;
  memcpy(&dib_header[4], &width, 4);
  int32_t const neg_height =
      -(int32_t)height; // top-down source, so store height negated: BMP stays
                        // bottom-up-free
  memcpy(&dib_header[8], &neg_height, 4);
  dib_header[12] = 1;  // planes
  dib_header[14] = 24; // bpp (always write 24-bit BGR)
  memcpy(&dib_header[20], &data_size, 4);

  fwrite(bmp_header, 1, sizeof(bmp_header), f);
  fwrite(dib_header, 1, sizeof(dib_header), f);

  uint8_t *row_out = malloc(row_padded);
  memset(row_out, 0, row_padded);

  for (uint32_t y = 0; y < height; y++) {
    uint8_t const *src_row = src + (size_t)y * pitch;
    for (uint32_t x = 0; x < width; x++) {
      // ARGB8888/XRGB8888 in little-endian memory order: B,G,R,(A|X).
      // 24bpp (e.g. RGB888-packed) is treated the same, B,G,R order.
      row_out[x * 3 + 0] = src_row[x * pixel_bytes + 0];
      row_out[x * 3 + 1] = src_row[x * pixel_bytes + 1];
      row_out[x * 3 + 2] = src_row[x * pixel_bytes + 2];
    }
    fwrite(row_out, 1, row_padded, f);
  }

  free(row_out);
  fclose(f);
}

int main(int argc, char **argv) {
  char const *dev_path = "/dev/dri/card0";
  char const *out_path = "screenshot.bmp";
  int crtc_id = -1;

  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-c") == 0) && (i + 1 < argc)) {
      crtc_id = atoi(argv[++i]);
    } else if ((strcmp(argv[i], "-o") == 0) && (i + 1 < argc)) {
      out_path = argv[++i];
    } else if ((strcmp(argv[i], "-d") == 0) && (i + 1 < argc)) {
      dev_path = argv[++i];
    } else {
      fprintf(
          stderr,
          "usage: %s [-c <crtc-id>] [-o <output.bmp>] [-d </dev/dri/cardN>]\n",
          argv[0]);
      return 1;
    }
  }

  int fd = open(dev_path, O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    fprintf(stderr, "drmshot: open(%s): %s\n", dev_path, strerror(errno));
    return 1;
  }

  drmModeRes *res = drmModeGetResources(fd);
  if (res == NULL) {
    fprintf(stderr, "drmshot: drmModeGetResources(): %s\n", strerror(errno));
    return 1;
  }

  drmModeCrtc *crtc = NULL;
  for (int i = 0; i < res->count_crtcs; i++) {
    drmModeCrtc *c = drmModeGetCrtc(fd, res->crtcs[i]);
    if (c == NULL) {
      continue;
    }
    if ((crtc_id >= 0) && (c->crtc_id == (uint32_t)crtc_id)) {
      crtc = c;
      break;
    }
    if ((crtc_id < 0) && (c->buffer_id != 0)) {
      crtc = c;
      break;
    }
    drmModeFreeCrtc(c);
  }
  drmModeFreeResources(res);

  if (crtc == NULL) {
    fprintf(stderr, "drmshot: no matching active CRTC found\n");
    return 1;
  }
  if (crtc->buffer_id == 0) {
    fprintf(stderr, "drmshot: CRTC %u has no active framebuffer\n",
            crtc->crtc_id);
    return 1;
  }

  drmModeFB *fb = drmModeGetFB(fd, crtc->buffer_id);
  if (fb == NULL) {
    fprintf(stderr, "drmshot: drmModeGetFB(%u): %s\n", crtc->buffer_id,
            strerror(errno));
    return 1;
  }
  if (fb->handle == 0) {
    fprintf(stderr,
            "drmshot: FB %u has no accessible handle (not permitted, or not "
            "dumb-mappable)\n",
            fb->fb_id);
    return 1;
  }

  fprintf(stderr,
          "drmshot: CRTC %u, FB %u: %ux%u, pitch=%u, bpp=%u, depth=%u\n",
          crtc->crtc_id, fb->fb_id, fb->width, fb->height, fb->pitch, fb->bpp,
          fb->depth);

  int prime_fd = -1;
  if (drmPrimeHandleToFD(fd, fb->handle, DRM_CLOEXEC | DRM_RDWR, &prime_fd) !=
      0) {
    fprintf(stderr, "drmshot: drmPrimeHandleToFD(): %s\n", strerror(errno));
    return 1;
  }

  size_t const map_size = (size_t)fb->pitch * fb->height;
  void *map = mmap(NULL, map_size, PROT_READ, MAP_SHARED, prime_fd, 0);
  if (map == MAP_FAILED) {
    fprintf(stderr, "drmshot: mmap(): %s\n", strerror(errno));
    return 1;
  }

  write_bmp(out_path, (uint8_t const *)map, fb->width, fb->height, fb->pitch,
            fb->bpp);
  fprintf(stderr, "drmshot: wrote %s\n", out_path);

  munmap(map, map_size);
  close(prime_fd);
  drmModeFreeFB(fb);
  drmModeFreeCrtc(crtc);
  close(fd);
  return 0;
}
