#include "render.h"
#include "font8x8_basic.h"

#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

static void plot(struct RenderContext *ctx, uint16_t x, uint16_t y, uint8_t v) {
	uint32_t base = (x + (y * WIDTH)) * 4;
	ctx->buffer[base + 0] = v;
	ctx->buffer[base + 1] = v;
	ctx->buffer[base + 2] = v;
	ctx->buffer[base + 3] = 255;
}

static void plotLine(struct RenderContext *ctx, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t fill) {
	uint16_t dx = abs(x1-x0);
	int8_t sx = x0<x1 ? 1 : -1;
	int16_t dy = -abs(y1-y0);
	int8_t sy = y0<y1 ? 1 : -1;
	int16_t err = dx + dy;
	uint8_t cnt = 0;

	for(;;) {
		if(cnt == 0) {
			plot(ctx, x0, y0, 255);
		}
		cnt = (cnt+1) % fill;
		if(x0==x1 && y0==y1) break;
		int16_t e2 = 2*err;
		if(e2 >= dy) { err += dy; x0 += sx; }
		if(e2 <= dx) { err += dx; y0 += sy; }
	}
}

struct dolphin {
	float angle;

	int32_t x;
	int32_t y;

	float velx;
	float vely;

	uint8_t inWater;
} player;

static bool process(struct RenderContext *ctx) {
	if(!pump(ctx)) {
		return false;
	}

	if(ctx->keys[KC_ESC]) {
		return false;
	}

	if(ctx->keys[KC_LEFT]) {
		player.angle += .04;
	}
	if(ctx->keys[KC_RIGHT]) {
		player.angle -= .04;
	}
	if(player.angle < 0.0) player.angle += M_PI*2;
	if(player.angle > 0.0) player.angle -= M_PI*2;

	player.inWater = player.y <= 0;
	float dx = cos(player.angle), dy = sin(player.angle);

	// Apply gravity over water
	if(!player.inWater) {
		player.vely -= 0.05;
	}

	// Apply drag under water
	if(player.inWater) {
		player.velx *= .99999f;
		player.vely *= .99999f;

		// Dot product of the velocity and player direction
		/* float vel_mag = sqrtf(powf(player.velx, 2) + powf(player.vely, 2)); */
		if(fabs(player.velx) > 0.00001 || fabs(player.vely) > 0.00001) {
			float dot = -dy * (player.velx) + dx * (player.vely);
			player.velx += -dy * -dot * .3;
			player.vely += dx * -dot * .3;
		}
	}

	static int last_up = 0;
	if(ctx->keys[KC_UP] && !last_up && player.inWater) {
		player.velx += dx * 1;
		player.vely += dy * 1;
	}
	last_up = ctx->keys[KC_UP];

	player.x += roundf(player.velx);
	player.y += roundf(player.vely);

	memset(ctx->buffer, 0, WIDTH * HEIGHT * 4);

	plotLine(ctx, 200 - dx*10                , 120 - -dy*10                , 200 + dx*10                , 120 + -dy*10                , 1);
	plotLine(ctx, 200 - dx*10 - player.velx  , 120 - -dy*10 + player.vely  , 200 + dx*10 - player.velx  , 120 + -dy*10 + player.vely  , 2);
	plotLine(ctx, 200 - dx*10 - player.velx*3, 120 - -dy*10 + player.vely*3, 200 + dx*10 - player.velx*3, 120 + -dy*10 + player.vely*3, 3);

	static float t = 0.0;
	t += 0.01667;

	int w_base = 120 + player.y;
	int w_offset = player.x;
	for(int x = 0; x < 400; x++) {
		float wave = cosf((w_offset + x + t*13) * M_PI*2 / 400  * 4) * 2;
		wave += cosf((w_offset + x + t*26) * M_PI*2 / 400 * 7.3) * 2.4;
		wave += cosf((w_offset + x + -t*50) * M_PI*2 / 400 * 1.2) * 4;
		float y = w_base + wave;
		if(y > 0 && y < 240) {
			plot(ctx, x, y, 255);
		}
	}

	return true;
}

float lerpf(float a, float b, float t) {
	return a * (1-t) + b * (t);
}

int main(int argc, char * argv[]) {
	struct RenderContext ctx;

	init_render(&ctx);

	player.y = 100;

	uint8_t fps;
	struct timespec frame_start;
	struct timespec prev_frame_start;
	while(true) {
		prev_frame_start = frame_start;
		clock_gettime(CLOCK_MONOTONIC, &frame_start);
		uint16_t frame_time = (frame_start.tv_sec - prev_frame_start.tv_sec) * 1000000000 + (frame_start.tv_nsec - prev_frame_start.tv_nsec) / 1000;
		fps = lerpf(fps, 1000000.0 / frame_time, 0.8);

		if(!process(&ctx)) {
			break;
		}

		char str[255];
		sprintf(str, "FPS %d", fps);
		uint8_t *display = ctx.buffer;
		for(char *c = str; *c != '\0'; c++) {
			uint8_t *letter = (uint8_t *)font8x8_basic[(uint8_t)*c];
			for(uint8_t row = 0; row < 8; row++) {
				uint8_t mask = 0x01;
				for(uint8_t col = 0; col < 8; col++) {
					if((letter[row] & mask) != 0) {
						display[0] = 255;
						display[1] = 255;
						display[2] = 255;
						display[3] = 255;
					}
					display += 4;
					mask = mask << 1;
				}
				display += WIDTH * 4 - 4*8;
			}
			display -= WIDTH * 4 * 8 - 8*4;
		}

		render(&ctx);

		{
			struct timespec frame_sleep;
			clock_gettime(CLOCK_MONOTONIC, &frame_sleep);
			uint32_t frame_time = (frame_sleep.tv_sec - frame_start.tv_sec) * 1000000000 + (frame_sleep.tv_nsec -frame_start.tv_nsec) / 1000;
			if(frame_time < 16000) {
				usleep(16600-frame_time);
			}
		}
	}

	stop(&ctx);

	printf("END\n");
	return 0;
}
