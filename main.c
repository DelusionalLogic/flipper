#include "render.h"
#include "font8x8_basic.h"

#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
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

struct splash {
	int32_t x;
	uint8_t alive;
	uint8_t scale;
} splash;

float lerpf(float a, float b, float t) {
	return a * (1-t) + b * (t);
}

float hash(float p) {
	float f;
	p = modff(p * 0.011, &f);
	p *= p + 7.5;
	p *= p + p;
	p = modff(p, &f);
	return p;
}

float noise(float x) {
    float i;
    float f = modff(x, &i);
    float u = f * f * (3.0 - 2.0 * f);
    return lerpf(hash(i), hash(i + 1.0), u);
}

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

	float dx = cos(player.angle), dy = sin(player.angle);

	if(player.inWater ^ (player.y <= 0)) {
		splash.x = player.x;
		splash.alive = 60;
		float dot = -dy * (player.velx) + dx * (player.vely);
		splash.scale = fminf(fabsf(dot) * 64, 255.0);
	}
	player.inWater = player.y <= 0;

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


	if(splash.alive > 0) {
		int y_base = 120 + player.y;
		int x_base = splash.x - player.x + 200;

		float t = 1.0 - splash.alive/60.0;

		for(uint8_t i = 0; i < 32; i++) {
			if(noise(0x40 | i) > t ) {
				uint16_t x = x_base + t * (noise(i)-0.5) * 1 * splash.scale;
				uint16_t y = y_base - sin(t * M_PI * lerpf(0.8, 1.0, noise(i | 0x80))) * noise(i | 0x80) * splash.scale/4;
				if(x >= 0 && x < 400 && y >= 0 && y < 240) {
					plot(ctx, x, y, 255);
				}
			}
		}

		splash.alive--;
	}

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

void text(char *str, uint8_t *pos) {
	for(char *c = str; *c != '\0'; c++) {
		uint8_t *letter = (uint8_t *)font8x8_basic[(uint8_t)*c];
		for(uint8_t row = 0; row < 8; row++) {
			uint8_t mask = 0x01;
			for(uint8_t col = 0; col < 8; col++) {
				if((letter[row] & mask) != 0) {
					pos[0] = 255;
					pos[1] = 255;
					pos[2] = 255;
					pos[3] = 255;
				}
				pos += 4;
				mask = mask << 1;
			}
			pos += WIDTH * 4 - 4*8;
		}
		pos -= WIDTH * 4 * 8 - 8*4;
	}
}

int main(int argc, char * argv[]) {
	struct RenderContext ctx;

	init_render(&ctx);

	player.y = 100;

	uint8_t fps = 0;
	struct timespec frame_start = {0};
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
		text(str, ctx.buffer);

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
