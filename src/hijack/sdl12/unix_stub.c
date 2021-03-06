/*
 * Copyright 2006-2016, Björn Ståhl
 * License: 3-Clause BSD, see COPYING file in arcan source repository.
 * Reference: http://arcan-fe.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <math.h>
#include <stdbool.h>
#include <ulimit.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>

#include "util/resampler/speex_resampler.h"

#include <SDL/SDL.h>
#include <dlfcn.h>
#include "sdl12.h"

extern struct hijack_fwdtbl forwardtbl;

SDL_GrabMode ARCAN_SDL_WM_GrabInput(SDL_GrabMode mode);
void ARCAN_target_shmsize(int w, int h, int bpp);
int ARCAN_SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
SDL_Surface* ARCAN_SDL_CreateRGBSurface(Uint32 flags, int width, int height,
	int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
SDL_Surface* ARCAN_SDL_SetVideoMode(int w, int h, int ncps, Uint32 flags);
int ARCAN_SDL_PollEvent(SDL_Event* inev);
int ARCAN_SDL_WaitEvent(SDL_Event* inev);
int ARCAN_SDL_Flip(SDL_Surface* screen);
void ARCAN_SDL_UpdateRect(SDL_Surface* screen,
	Sint32 x, Sint32 y, Uint32 w, Uint32 h);
void ARCAN_SDL_UpdateRects(SDL_Surface* screen,
	int numrects, SDL_Rect* rects);
int ARCAN_SDL_UpperBlit(SDL_Surface* src, const SDL_Rect* srcrect,
	SDL_Surface *dst, SDL_Rect *dstrect);

/* quick debugging hack */
static char* lastsym;

/* linked list of hijacked symbols,
 * sym - symbol name
 * ptr - function pointer to the redirected function
 * bounce - function pointer to the original function */
struct symentry {
	char* sym;
	void* ptr;
	void* bounce;
	struct symentry* next;
};

static struct {
	struct symentry* first;
	struct symentry* last;
} symtbl = {0};

static void fatal_catcher(){
	fprintf(stderr, "ARCAN_Hijack, fatal error in (%s), aborting.\n", lastsym);
	abort();
}

static void* lookupsym(const char* symname, void* bounce, bool fatal){
	void* res = dlsym(RTLD_NEXT, symname);

	if (res == NULL && fatal){
//		fprintf(stderr, "ARCAN_Hijack, warning: %s not found.\n", symname);
		res = fatal_catcher;
	}

	struct symentry* dst = malloc(sizeof(struct symentry));

	if (!symtbl.first){
		symtbl.first = dst;
		symtbl.last  = dst;
	}
	else{
		symtbl.last->next = dst;
		symtbl.last       = dst;
	}

	dst->sym    = strdup(symname);
	dst->ptr    = res;
	dst->bounce = bounce;
	dst->next   = NULL;

	return res;
}

static struct symentry* find_symbol(const char* sym)
{
	struct symentry* res = symtbl.first;

	while (res != NULL) {
		if (strcmp(res->sym, sym) == 0)
			return res;

		res = res->next;
	}

	return res;
}

void build_forwardtbl()
{
  forwardtbl.sdl_grabinput = lookupsym("SDL_WM_GrabInput",ARCAN_SDL_WM_GrabInput, true);
	forwardtbl.sdl_openaudio = lookupsym("SDL_OpenAudio",ARCAN_SDL_OpenAudio, true);
	forwardtbl.sdl_peepevents = lookupsym("SDL_PeepEvents",NULL, true);
	forwardtbl.sdl_pollevent = lookupsym("SDL_PollEvent",ARCAN_SDL_PollEvent, true);
	forwardtbl.sdl_waitevent = lookupsym("SDL_WaitEvent", ARCAN_SDL_WaitEvent, true);
	forwardtbl.sdl_pushevent = lookupsym("SDL_PushEvent",NULL, true);
	forwardtbl.sdl_flip = lookupsym("SDL_Flip",ARCAN_SDL_Flip, true);
	forwardtbl.sdl_iconify = lookupsym("SDL_WM_IconifyWindow", NULL, true);
	forwardtbl.sdl_updaterect = lookupsym("SDL_UpdateRect", ARCAN_SDL_UpdateRect, true);
	forwardtbl.sdl_updaterects = lookupsym("SDL_UpdateRects", ARCAN_SDL_UpdateRects, true);
	forwardtbl.sdl_upperblit = lookupsym("SDL_UpperBlit", ARCAN_SDL_UpperBlit, true);
	forwardtbl.sdl_starteventloop = lookupsym("SDL_StartEventLoop", NULL, true);
	forwardtbl.sdl_setvideomode = lookupsym("SDL_SetVideoMode", ARCAN_SDL_SetVideoMode, true);
	forwardtbl.sdl_creatergbsurface = lookupsym("SDL_CreateRGBSurface", ARCAN_SDL_CreateRGBSurface, true);

/* we need XOpenDisplay() to return some nonsens */

	forwardtbl.glLineWidth = lookupsym("glLineWidth", NULL, true);
	forwardtbl.glPointSize = lookupsym("glPointSize", NULL, true);

/* SDL_mixer hijack, might not be present */
	forwardtbl.audioproxy = lookupsym("Mix_Volume", NULL, false);
}

__attribute__((constructor))
static void hijack_init(void){
/* force an SDL audio/video driver with a known behavior */
	setenv("SDL_VIDEODRIVER", "dummy", 0);
	setenv("SDL_AUDIODRIVER", "dummy", 0);
	build_forwardtbl();
}

__attribute__((destructor))
static void hijack_close(void){
}

/* UNIX hijack requires both symbol collision and pointers to forward call */

SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode)
{
	lastsym = "SDL_WM_GrabInput";
	return ARCAN_SDL_WM_GrabInput(mode);
}

void SDL_WarpMouse(uint16_t x, uint16_t y){
	lastsym = "SDL_WarpMouse";
}

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
	lastsym = "SDL_OpenAudio";
	return ARCAN_SDL_OpenAudio(desired, obtained);
}

SDL_Surface* SDL_SetVideoMode(int w, int h, int ncps, Uint32 flags)
{
	lastsym = "SDL_SetVideoMode";
	return ARCAN_SDL_SetVideoMode(w, h, ncps, flags);
}

int SDL_PollEvent(SDL_Event* ev)
{
	lastsym = "SDL_PollEvent";
	return ARCAN_SDL_PollEvent(ev);
}

int SDL_WaitEvent(SDL_Event* ev)
{
	lastsym = "SDL_WaitEvent";
	return ARCAN_SDL_WaitEvent(ev);
}

/* Used by double-buffered non-GL apps */
int SDL_Flip(SDL_Surface* screen)
{
	lastsym = "SDL_Flip";
	return ARCAN_SDL_Flip(screen);
}

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask){
	lastsym = "SDL_CreateRGBSurface";
	return ARCAN_SDL_CreateRGBSurface(flags, width, height, depth, Rmask, Gmask, Bmask, Amask);
}

void SDL_WM_SetCaption(const char* title, const char* icon)
{
	lastsym = "SDL_WM_SetCaption";
	ARCAN_SDL_WM_SetCaption(title, icon);
}

void SDL_UpdateRects(SDL_Surface* screen, int numrects, SDL_Rect* rects){
	lastsym = "SDL_UpdateRects";
	ARCAN_SDL_UpdateRects(screen, numrects, rects);
}

void SDL_UpdateRect(SDL_Surface* surf, Sint32 x, Sint32 y, Uint32 w, Uint32 h){
	lastsym = "SDL_UpdateRect";
	ARCAN_SDL_UpdateRect(surf, x, y, w, h);
}

/* disable fullscreen attempts on X11, VideoMode hijack removes it from flags */
int SDL_WM_ToggleFullscreen(SDL_Surface* screen){
		return 0;
}

DECLSPEC int SDLCALL SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect){
	return ARCAN_SDL_UpperBlit(src, srcrect, dst, dstrect);
}
