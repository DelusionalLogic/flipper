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

static const uint8_t dither[]  = {
  0x03, 0x83, 0x23, 0xa3, 0x0b, 0x8b, 0x2b, 0xab, 
  0xc3, 0x43, 0xe3, 0x63, 0xcb, 0x4b, 0xeb, 0x6b, 
  0x33, 0xb3, 0x13, 0x93, 0x3b, 0xbb, 0x1b, 0x9b, 
  0xf3, 0x73, 0xd3, 0x53, 0xfb, 0x7b, 0xdb, 0x5b, 
  0x0f, 0x8f, 0x2f, 0xaf, 0x07, 0x87, 0x27, 0xa7, 
  0xcf, 0x4f, 0xef, 0x6f, 0xc7, 0x47, 0xe7, 0x67, 
  0x3f, 0xbf, 0x1f, 0x9f, 0x37, 0xb7, 0x17, 0x97, 
  0xff, 0x7f, 0xdf, 0x5f, 0xf7, 0x77, 0xd7, 0x57
};

static void plot(struct RenderContext *ctx, uint16_t x, uint16_t y, uint8_t v) {
	assert(x >= 0 && x < WIDTH);
	assert(y >= 0 && y < HEIGHT);
	uint32_t base = (x + (y * WIDTH)) * 4;
	ctx->buffer[base + 0] = v;
	ctx->buffer[base + 1] = v;
	ctx->buffer[base + 2] = v;
	ctx->buffer[base + 3] = 255;
}

static void plotLine(struct RenderContext *ctx, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t fill) {
	assert(x0 < 0xFFFF);
	assert(x1 < 0xFFFF);
	assert(y0 < 0xFFFF);
	assert(y1 < 0xFFFF);
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
	float wiggleT;

	float bend;
	float wiggle;

} player;

// @COMPLETE: It's possible for the player to splash multiple times, maybe we
// should have multiple of these things
struct splash {
	// A random number to make the particles unique
	uint8_t seed;

	int32_t x;
	uint8_t life;
	uint8_t alive;

	uint8_t scale;
	uint8_t escale;
} splash;

float clampf(float min, float max, float t) {
	return fmaxf(fminf(t, max), min);
}

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

	{
		float dir;
		{
			float dx = cos(player.angle), dy = sin(player.angle);
			float dot = dx * (player.velx) + dy * (player.vely);
			dir = dot > 0.0 ? 1 : -1;
		}

		if(ctx->keys[KC_LEFT]) {
			player.angle += .04;
			player.bend += dir * 0.15;
		} else if(ctx->keys[KC_RIGHT]) {
			player.angle -= .04;
			player.bend -= dir * 0.15;
		} else {
			player.bend -= (player.bend > 0.0 ? 1 : -1) * 0.1;
		}
		if(player.angle < 0.0) player.angle += M_PI*2;
		if(player.angle > 0.0) player.angle -= M_PI*2;
		player.bend = clampf(-1.0, 1.0, player.bend);
	}

	float dx = cos(player.angle), dy = sin(player.angle);

	if(player.inWater ^ (player.y <= 0)) {
		splash.seed = rand();
		splash.x = player.x;
		float dot = -dy * (player.velx) + dx * (player.vely);
		splash.scale = fminf(fabsf(dot) * 64, 255.0);
		splash.life = lerpf(30, 60, splash.scale/255.0);
		splash.alive = splash.life;
		float pdot = dx * (player.velx) + dy * (player.vely);
		splash.escale = player.inWater ? 0 : fmin(fabsf(pdot) * 64, 255.0);
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

		if(fabs(player.velx) > 0.00001 || fabs(player.vely) > 0.00001) {
			float dot = -dy * (player.velx) + dx * (player.vely);
			// There's some layer of less heavy water near the surface
			float depth_factor = powf(clampf(0.0, 1.0, -player.y / 50.0), 2);
			player.velx += -dy * -dot * .3 * depth_factor;
			player.vely +=  dx * -dot * .3 * depth_factor;
		}
	}

	static int last_up = 0;
	if(ctx->keys[KC_UP] && !last_up && player.inWater) {
		player.velx += dx * 1;
		player.vely += dy * 1;
		player.wiggleT = 60;
	}
	last_up = ctx->keys[KC_UP];

	player.x += roundf(player.velx);
	player.y += roundf(player.vely);

	static float t = 0.0;
	t += 0.01667;

	{ // Draw the background and wave
		int w_offset = player.x;
		float wave[WIDTH];
		for(int x = 0; x < WIDTH; x++) {
			wave[x] = sinf((w_offset + x + t*26) * M_PI*2 / 400  * 5.5) * 2;
			wave[x] += sinf((w_offset + x - t*4) * M_PI*2 / 400  * 4) * 2;
			wave[x] += sinf((w_offset + x + t*33) * M_PI*2 / 400 * 7.3) * 1.2;
			wave[x] += sinf((w_offset + x + -t*50) * M_PI*2 / 400 * 1.2) * 4;
		}

		for(uint16_t y = 0; y < HEIGHT; y++) {
			int16_t ly = (-player.y - HEIGHT/2.0) + y;
			for(uint16_t x = 0; x < WIDTH; x++) {
				int16_t lx = (player.x - WIDTH/2.0) + x;
				uint8_t color = 0.0;
				float h = wave[x] - ly;
				if(fabs(h) < 2.0) {
					/* plot(ctx, x, y, 255); */
					color = 120;
					color = lerpf(255, 0, clampf(0.0, 1.0, fabs(h)/2));
				} else {

					if(h < 0.0) {
						// Underwater
						if(ly < 50.0) {
							color = lerpf(40, 0, clampf(0.0, 1.0, -h/20.0));
						} else if(ly > 500) {
							color = lerpf(0, 255.5, clampf(0.0, 1.0, (ly-500)/100.0));
							/* color = 0.0; */
						}
					} else {
						if(h < 10.0) {
							/* color = lerpf(0, 255, clampf(0.0, 1.0, h/10.0)); */
						} else {
							color = lerpf(55, 0, clampf(0.0, 1.0, -ly/1000.0));
						}
					}
				}

				uint8_t qcolor  = dither[abs(lx) % 8 + (abs(ly) % 8) * 8] <= color ? 255 : 0;
				plot(ctx, x, y, qcolor);
			}
		}
	}

	if(splash.alive > 0) {
		int y_base = 120 + player.y;
		int x_base = splash.x - player.x + 200;

		float prev_t = clampf(0, 1, 1.0 - (splash.alive+1.5)/(float)splash.life);
		float t = 1.0 - splash.alive/(float)splash.life;

		for(uint8_t i = 0; i < splash.scale/4; i++) {
			if(noise(0x40 ^ i ^ splash.seed) <= t) {
				continue;
			}
			uint16_t x      = x_base +      t * (noise(i ^ splash.seed)-0.5) * splash.scale * 1;
			uint16_t prev_x = x_base + prev_t * (noise(i ^ splash.seed)-0.5) * splash.scale * 1;

			uint16_t y      = y_base - sin(     t * M_PI * lerpf(0.8, 1.0, noise(i ^ 0x80 ^ splash.seed))) * noise(i ^ 0x80 ^ splash.seed) * splash.scale * 0.25;
			uint16_t prev_y = y_base - sin(prev_t * M_PI * lerpf(0.8, 1.0, noise(i ^ 0x80 ^ splash.seed))) * noise(i ^ 0x80 ^ splash.seed) * splash.scale * 0.25;
			// We don't do clipping. Just discard any particle partly outside
			// the viewport
			if(x >= 0 && x < 400 && y >= 0 && y < 240) {
				if(prev_x >= 0 && prev_x < 400 && prev_y >= 0 && prev_y < 240) {
					plotLine(ctx, x, y, prev_x, prev_y, 1);
				}
			}
		}

		// This kinda looks bad
		/* for(uint8_t i = 0; i < splash.escale/4; i++) { */
		/* 	if(noise(0x40 | i) <= t) { */
		/* 		continue; */
		/* 	} */
		/* 	uint16_t x      = x_base +      t * (noise(i)-0.5) * splash.escale * 0.25; */
		/* 	uint16_t prev_x = x_base + prev_t * (noise(i)-0.5) * splash.escale * 0.25; */

		/* 	uint16_t y      = y_base + sin(     t * M_PI * lerpf(0.8, 1.0, noise(i | 0x80))) * noise(i | 0x80) * splash.escale * 0.2625; */
		/* 	uint16_t prev_y = y_base + sin(prev_t * M_PI * lerpf(0.8, 1.0, noise(i | 0x80))) * noise(i | 0x80) * splash.escale * 0.2625; */
		/* 	if(x >= 0 && x < 400 && y >= 0 && y < 240) { */
		/* 		plotLine(ctx, x, y, prev_x, prev_y, 1); */
		/* 	} */
		/* } */

		splash.alive--;
	}

	if(player.wiggleT > 0.0) {
		player.wiggleT -= 1.0;
		player.wiggle += M_PI/15.0;
	} else {
		player.wiggle = 0.0;
	}

	float wiggle = lerpf(0.0, -sin(player.wiggle) * 0.8, player.wiggleT/60.0);
	float tx = cos(player.angle - player.bend * 0.2 - wiggle), ty = sin(player.angle - player.bend * 0.2 - wiggle);
	float hx = cos(player.angle + player.bend * 0.2), hy = sin(player.angle + player.bend * 0.2);
	// Tail
	plotLine(ctx, 200 - tx*10                , 120 - -ty*10                , 200                        , 120                         , 1);
	// Head
	plotLine(ctx, 200                        , 120                         , 200 + hx*10                , 120 + -hy*10                , 1);
	plotLine(ctx, 200 - dx*10 - player.velx  , 120 - -dy*10 + player.vely  , 200 + dx*10 - player.velx  , 120 + -dy*10 + player.vely  , 2);
	plotLine(ctx, 200 - dx*10 - player.velx*3, 120 - -dy*10 + player.vely*3, 200 + dx*10 - player.velx*3, 120 + -dy*10 + player.vely*3, 3);

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
