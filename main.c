#include "render.h"

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

int main(int argc, char * argv[]) {
	struct RenderContext ctx;

	init_render(&ctx);

	player.y = 100;

	struct timespec frame_start;
	while(true) {
		clock_gettime(CLOCK_MONOTONIC, &frame_start);
		if(!process(&ctx)) {
			break;
		}

		render(&ctx);

		struct timespec frame_sleep;
		clock_gettime(CLOCK_MONOTONIC, &frame_sleep);
		uint32_t frame_time = (frame_sleep.tv_sec - frame_start.tv_sec) * 1000000000 + (frame_sleep.tv_nsec -frame_start.tv_nsec) / 1000;
		if(frame_time < 16000) {
			usleep(16600-frame_time);
		}
	}

	stop(&ctx);

	printf("END\n");
	return 0;
}
