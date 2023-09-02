#include "render.h"

#include <assert.h>
#include <string.h>

static int filter(const SDL_Event * event) {
	if(event->type == SDL_QUIT) return 1;
	return 0;
}

void init_render(struct RenderContext *ctx) {
	_Bool ok;

	ctx->buffer = malloc(WIDTH * HEIGHT * 4);

	atexit(SDL_Quit);
	if(SDL_Init(SDL_INIT_VIDEO) < 0)
		assert(false);

	char *name = "Flipper";
	SDL_WM_SetCaption(name, name);
	SDL_WM_SetIcon(NULL, NULL);

	ok = SDL_SetVideoMode(WIDTH, HEIGHT, 24, SDL_HWSURFACE);
	assert(ok);

	ctx->surface = SDL_CreateRGBSurfaceFrom(
		ctx->buffer,
		WIDTH, HEIGHT,
		32, WIDTH * 4,
		0xff, 0xff << 8, 0xff << 16, 0
	);

	SDL_SetEventFilter(filter);
}

bool pump(struct RenderContext *ctx) {
	for(SDL_Event event; SDL_PollEvent(&event);)
		if(event.type == SDL_QUIT)
			return false;

	uint8_t *keystate = SDL_GetKeyState(NULL);
	ctx->keys[KC_UP] = keystate[SDLK_UP];
	ctx->keys[KC_LEFT] = keystate[SDLK_LEFT];
	ctx->keys[KC_RIGHT] = keystate[SDLK_RIGHT];
	ctx->keys[KC_ESC] = keystate[SDLK_ESCAPE];

	return true;
}

void render(struct RenderContext *ctx) {
    SDL_Surface * screen = SDL_GetVideoSurface();
    if(SDL_BlitSurface(ctx->surface, NULL, screen, NULL) == 0) {
        SDL_UpdateRect(screen, 0, 0, 0, 0);
	}
}

void stop(struct RenderContext *ctx) {
}
