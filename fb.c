#include "render.h"

#include <assert.h>
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>

void init_render(struct RenderContext *ctx) {
	ctx->prev_tty = 0;

	int fd = open("/dev/input/event1", O_RDONLY|O_NONBLOCK);
	int rc = libevdev_new_from_fd(fd, &ctx->dev);
	if (rc < 0) {
		fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
		exit(1);
	}

	printf("Input device name: \"%s\"\n", libevdev_get_name(ctx->dev));
	printf(
		"Input device ID: bus %#x vendor %#x product %#x\n",
		libevdev_get_id_bustype(ctx->dev),
		libevdev_get_id_vendor(ctx->dev),
		libevdev_get_id_product(ctx->dev)
	);

	sleep(1);

	{
		ctx->ttyfd = open("/dev/tty0", O_RDWR);
		if(ctx->ttyfd < 0) {
			fprintf(stderr, "Failed to open initial tty (%m)\n");
			abort();
		}

		uint32_t nr;
		if (ioctl(ctx->ttyfd, VT_OPENQRY, &nr) < 0) {
			perror("ioctl VT_OPENQRY");
			goto fail;
		}
		close(ctx->ttyfd);

		char vtname[128];
		sprintf(vtname, "/dev/tty%d", nr);

		ctx->ttyfd = open(vtname, O_RDWR | O_NDELAY);
		if(ctx->ttyfd < 0) {
			fprintf(stderr, "Failed to open tty (%m)\n");
			abort();
		}

		{
			struct vt_stat stat;
			if (ioctl(ctx->ttyfd, VT_GETSTATE, &stat) < 0) {
				perror("VT_GETSTATE");
				goto fail;
			}
			ctx->prev_tty = stat.v_active;
		}

		if (ioctl(ctx->ttyfd, VT_ACTIVATE, nr) < 0) {
			perror("VT_ACTIVATE");
			goto fail;
		}

		if (ioctl(ctx->ttyfd, VT_WAITACTIVE, nr) < 0) {
			perror("VT_WAITACTIVE");
			goto fail;
		}

		if (ioctl(ctx->ttyfd, KDSETMODE, KD_GRAPHICS) < 0) {
			perror("KDSETMODE");
			goto fail;
		}
	}

	int fbfd = open("/dev/fb0", O_RDWR);
	if(fbfd < 0) {
		fprintf(stderr, "Failed to init framebuffer (%m)\n");
		goto fail;
	}

#ifndef NDEBUG
	struct fb_var_screeninfo vinfo;
	ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
	assert(vinfo.xres == WIDTH);
	assert(vinfo.yres == HEIGHT);
	assert(vinfo.bits_per_pixel == 32);
#endif

	ioctl(fbfd, KDSETMODE, KD_GRAPHICS);
	ctx->fbuffer = mmap(0, WIDTH * HEIGHT * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);
	ctx->buffer = malloc(WIDTH * HEIGHT * 4);
	assert(ctx->buffer != NULL);

	memset(ctx->keys, 0, KC_LAST * sizeof(uint8_t));
	return;

fail:
	if(ctx->ttyfd > 0) close(ctx->ttyfd);
	abort();
}

bool pump(struct RenderContext *ctx) {
	int rc = 1;

	do {
		struct input_event ev;
		rc = libevdev_next_event(ctx->dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == 0) {
			/* printf( */
			/* 	"Event: %s %s %d\n", */
			/* 	libevdev_event_type_get_name(ev.type), */
			/* 	libevdev_event_code_get_name(ev.type, ev.code), */
			/* 	ev.value */
			/* ); */
			if(ev.type == EV_KEY) {
				switch(ev.code) {
					case KEY_D:
						ctx->keys[KC_LEFT] = ev.value;
						break;
					case KEY_K:
						ctx->keys[KC_RIGHT] = ev.value;
						break;
					case KEY_SPACE:
						ctx->keys[KC_UP] = ev.value;
						break;
					case KEY_ESC:
						ctx->keys[KC_ESC] = ev.value;
						break;
				}
			}
		}
	} while (rc == 1 || rc == 0);
	
	return true;
}

void render(struct RenderContext *ctx) {
	memcpy(ctx->fbuffer, ctx->buffer, WIDTH * HEIGHT * 4);
}

void stop(struct RenderContext *ctx) {
	memset(ctx->fbuffer, 0, WIDTH*HEIGHT*4);
	munmap(ctx->fbuffer, WIDTH*HEIGHT*4);
	close(ctx->fbfd);
	free(ctx->buffer);

	// Shouldn't this be saved when opened too?
	if (ioctl(ctx->ttyfd, KDSETMODE, KD_TEXT) < 0) {
		perror("KDSETMODE");
		goto err;
	}

	if (ioctl(ctx->ttyfd, VT_ACTIVATE, ctx->prev_tty) < 0) {
		perror("VT_ACTIVATE");
		goto err;
	}

	close(ctx->ttyfd);
	return;

err:
	close(ctx->ttyfd);
	abort();
}
