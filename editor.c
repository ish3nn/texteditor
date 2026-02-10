//
// Created by ish3nn on 2/9/26.
//

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <stdbool.h>

#include "config.h"

#define GLYPH_COUNT 256
#define gutter_w 60.0f

struct glyph {
	SDL_Texture *texture;
	int w, h;
	int advance;
};

struct graphicscontext
{
	SDL_Window *window;
	SDL_Renderer *render;
	TTF_Font *fonteditor;
	TTF_Font *fontgutter;

	int width,height;
	float refreshrate;

	char *filepath;
	char *filebuffer;
	size_t filesize;

	struct glyph glyphs[GLYPH_COUNT];

	bool dirtyframe;
};

static void loggingsystem(void *userdata, int category, SDL_LogPriority priority, const char *message);
static void readfile(const char *path, void *appstate);
static void savefile(const char *path, void *appstate);
static void cacheglyphs(void *appstate);
static void rendertext(void *appstate);
static void graphics(struct graphicscontext *context);

/* ------------------------------------------------------------ */

void
loggingsystem(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
	(void)userdata;

	const char *lvl = "UNKNOWN";
	const char *cat = 0;
	const char *col = "";
	const char *reset = "\x1b[0m";

	/* priority → текст + цвет */
	switch (priority){
	case SDL_LOG_PRIORITY_VERBOSE: lvl = "VERBOSE";
		col = "\x1b[90m"; // gray
		break;

	case SDL_LOG_PRIORITY_DEBUG: lvl = "DEBUG";
		col = "\x1b[36m"; // cyan
		break;

	case SDL_LOG_PRIORITY_INFO: lvl = "INFO";
		col = "\x1b[32m"; // green
		break;

	case SDL_LOG_PRIORITY_WARN: lvl = "WARN";
		col = "\x1b[33m"; // yellow
		break;

	case SDL_LOG_PRIORITY_ERROR: lvl = "ERROR";
		col = "\x1b[31m"; // red
		break;

	case SDL_LOG_PRIORITY_CRITICAL: lvl = "CRITICAL";
		col = "\x1b[35m"; // magenta
		break;

	default: break;
	}

	/* category → текст */
	switch (category)
	{
	case SDL_LOG_CATEGORY_APPLICATION: cat = "APP";
		break;
	case SDL_LOG_CATEGORY_ERROR: cat = "ERROR";
		break;
	case SDL_LOG_CATEGORY_ASSERT: cat = "ASSERT";
		break;
	case SDL_LOG_CATEGORY_SYSTEM: cat = "SYSTEM";
		break;
	case SDL_LOG_CATEGORY_AUDIO: cat = "AUDIO";
		break;
	case SDL_LOG_CATEGORY_VIDEO: cat = "VIDEO";
		break;
	case SDL_LOG_CATEGORY_RENDER: cat = "RENDER";
		break;
	case SDL_LOG_CATEGORY_INPUT: cat = "INPUT";
		break;
	case SDL_LOG_CATEGORY_TEST: cat = "TEST";
		break;
	default: cat = "USER";
		break;
	}

	fprintf(stderr, "%s[%s][%s]: %s%s\n", col, lvl, cat, message, reset);
}

void
readfile(const char *path, void *appstate)
{
	struct graphicscontext *context = appstate;

	FILE *file = fopen(path, "rb");
	if (!file)
	{
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM,"Не удалось открыть файл");
		return;
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	rewind(file);

	free(context->filebuffer);
	context->filebuffer = malloc(size + 1);

	fread(context->filebuffer, 1, size, file);
	fclose(file);

	context->filebuffer[size] = '\0';
	context->filesize = size;
}

void
savefile(const char *path, void *appstate)
{
	struct graphicscontext *context = appstate;

	FILE *file = fopen(path, "wb");
	if (!file)
	{
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Не удалось открыть файл для записи");
		return;
	}

	fwrite(context->filebuffer, 1, context->filesize, file);
	fclose(file);

	SDL_Log("Файл сохранён");
}

void
cacheglyphs(void *appstate)
{
	struct graphicscontext *context = appstate;
	SDL_Color white = {255,255,255,255};

	for (uint32_t c = 32; c < 127; c++) {

		SDL_Surface *surf =
			TTF_RenderGlyph_Blended(context->fonteditor, c, white);
		if (!surf)
			continue;

		SDL_Texture *tex =
			SDL_CreateTextureFromSurface(context->render, surf);

		context->glyphs[c].texture = tex;
		context->glyphs[c].w = surf->w;
		context->glyphs[c].h = surf->h;

		int minx, maxx, miny, maxy, advance;
		if (TTF_GetGlyphMetrics(context->fonteditor, c,
		                        &minx, &maxx, &miny, &maxy, &advance) == 0)
			context->glyphs[c].advance = advance;
		else
			context->glyphs[c].advance = surf->w;

		SDL_DestroySurface(surf);
	}
}

void
rendertext(void *appstate)
{
	struct graphicscontext *context = appstate;
	if (!context || !context->filebuffer) return;

	int startx = 80;
	int x = startx;
	int y = 10;

	int line_h = TTF_GetFontHeight(context->fonteditor);

	const char *buf = context->filebuffer;
	size_t size = context->filesize;

	for (size_t i = 0; i < size; i++)
	{
		unsigned char c = (unsigned char)buf[i];

		if (c == '\n') {
			x = startx;
			y += line_h;
			continue;
		}

		struct glyph *g = &context->glyphs[c];

		if (g->texture) {
			SDL_FRect dst = { (float)x, (float)y,
			                  (float)g->w, (float)g->h };
			SDL_RenderTexture(context->render, g->texture, NULL, &dst);
		}

		x += g->advance;
	}
}

/* ------------------------------------------------------------ */

void
frame(void *appstate)
{
	struct graphicscontext *context = appstate;
	if (!context || !context->dirtyframe) return;

	SDL_RenderClear(context->render);

	rendertext(context);

	SDL_RenderPresent(context->render);
	context->dirtyframe = false;
}

void
graphics(struct graphicscontext *context)
{
	if (!context)
	{
		assert(context != NULL);
		return;
	};

	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();

	SDL_DisplayID displayid = SDL_GetPrimaryDisplay(); // получение инфы о дисплее
	if (!displayid)
	{
		assert(displayid != 0);
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Ошибка ПОЛУЧЕНИЯ информации о мониторе, fallback режим");
		goto fallback;
	}

	const SDL_DisplayMode *phys = SDL_GetCurrentDisplayMode(displayid); // обращаемся к дисплею, что не надежно
	// нужен fallback
	if (!phys)
	{
		assert(phys != NULL);
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Ошибка ОБРАЩЕНИЯ информации о мониторе, fallback режим");
		goto fallback;
	}

	context->width = phys->w;
	context->height = phys->h;

	if (!phys->refresh_rate)
		context->refreshrate = 16.67f; // fallback ~60Hz
	else
		context->refreshrate = (1000.0f / phys->refresh_rate) + 0.5f;

	goto normal;

	fallback:
		context->width = 800;
	context->height = 600;
	context->refreshrate = 16.67f;

	normal:

		SDL_CreateWindowAndRenderer(
			"TEXT EDITOR",
			context->width,
			context->height,
			SDL_WINDOW_RESIZABLE,
			&context->window,
			&context->render
		);

	context->fonteditor = FONT_EDITOR
	context->fontgutter = FONT_GUTTER
	if (!context->fonteditor){
		assert(context->fonteditor != NULL);

		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Ошибка загрузки шрифта: %s\n", SDL_GetError());

		context->fonteditor = TTF_OpenFont("assets/fallback/font.ttf", 48);
		if (!context->fonteditor)
		{
			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Ошибка загрузки fallback шрифта");
			return;
		}

		SDL_Log("Fallback шрифт успешно загружен.");
	}
	SDL_Log("Шрифт успешно загружен.");

	context->dirtyframe = true;

	SDL_StartTextInput(context->window);
}

void
textinput(void *appstate, SDL_Event *event)
{
	struct graphicscontext *context = appstate;
	if (!context || !event) return;

	const char *text = event->text.text;
	if (!text || !text[0]) return;

	size_t add = strlen(text);

	char *newbuf = realloc(context->filebuffer, context->filesize + add + 1);
	if (!newbuf) {
		SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "realloc failed (OOM)");
		return;
	}

	context->filebuffer = newbuf;

	memcpy(context->filebuffer + context->filesize, text, add);
	context->filesize += add;
	context->filebuffer[context->filesize] = '\0';

	context->dirtyframe = true;
}

void
backspace(void *appstate)
{
	struct graphicscontext *context = appstate;
	if (!context || !context->filebuffer || context->filesize == 0)
		return;

	size_t i = context->filesize;

	/* идём назад до начала UTF-8 символа */
	do {
		i--;
	} while (i > 0 && ((context->filebuffer[i] & 0xC0) == 0x80));

	context->filesize = i;
	context->filebuffer[i] = '\0';

	context->dirtyframe = true;
}



/* ------------------------------------------------------------ */

SDL_AppResult
SDL_AppInit(void **appstate, int argc, char *argv[])
{
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
	SDL_SetLogOutputFunction(loggingsystem, 0);

	struct graphicscontext *context = calloc(1, sizeof(*context));

	graphics(context);

	if (argc > 1 && argv[1] && argv[1][0] != '\0')
		context->filepath = argv[1];
	else
		context->filepath = "test.txt";

	readfile(context->filepath, context);
	cacheglyphs(context);

	context->dirtyframe = true;

	*appstate = context;
	return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate(void *appstate) //per frame
{
	frame(appstate);
	SDL_WaitEvent(NULL);
	return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppEvent(void *appstate, SDL_Event *event)
{
	struct graphicscontext *context = appstate;

	switch (event->type){
	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;

	case SDL_EVENT_WINDOW_EXPOSED:
	case SDL_EVENT_WINDOW_RESIZED:
		context->dirtyframe = true; break;

	case SDL_EVENT_TEXT_INPUT:
		textinput(appstate, event);
		break;

	case SDL_EVENT_KEY_DOWN:
		if (event->key.key == SDLK_BACKSPACE)
			backspace(appstate);

		if ((event->key.mod & SDL_KMOD_CTRL) && event->key.key == SDLK_S)
			savefile(context->filepath, context);

		break;

	/*case SDL_EVENT_KEY_DOWN:
		if (event->key.key == SDLK_TAB)
			tab(appstate);
		if (event->key.key == SDLK_RETURN)
			enter(appstate);
		if ((event->key.mod & SDL_KMOD_CTRL) && event->key.key == SDLK_S)
			save_file(context, context->filepath);

		break;
  */
	default:
		return SDL_APP_CONTINUE;
	}
	return SDL_APP_CONTINUE;
}

/* ------------------------------------------------------------ */

void
SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	struct graphicscontext *context = appstate;

	for (int i = 0; i < GLYPH_COUNT; i++)
		if (context->glyphs[i].texture)
			SDL_DestroyTexture(context->glyphs[i].texture);

	TTF_CloseFont(context->fonteditor);
	SDL_DestroyRenderer(context->render);
	SDL_DestroyWindow(context->window);

	free(context->filebuffer);
	free(context);
	SDL_Quit();
}