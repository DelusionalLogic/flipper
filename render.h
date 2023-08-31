#pragma once

#define SDL 1
#define FB 2

#include <stdint.h>
#include <stdbool.h>

#if RENDER == SDL
#include <SDL/SDL.h>
#elif RENDER == FB
#include <libevdev/libevdev.h>
#endif

#define WIDTH 400
#define HEIGHT 240

enum KeyCode {
	KC_LEFT,
	KC_RIGHT,
	KC_UP,

	KC_ESC,
	KC_LAST,
};

struct RenderContext {
	union {
#if RENDER == SDL
		SDL_Surface *surface;
#elif RENDER == FB
		struct libevdev *dev;
#endif
	};
	uint8_t keys[KC_LAST];
	uint8_t *buffer;
};

void init_render(struct RenderContext *ctx);
bool pump(struct RenderContext *ctx);
void render(struct RenderContext *ctx);
