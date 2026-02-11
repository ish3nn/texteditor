//
// Created by ish3nn on 2/11/26.
//

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <stdbool.h>

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


#include "config.h"

#define MAX_FILES 100


struct cachedglyph {
	uint32_t codepoint;
	SDL_Texture *texture;
};

struct glyphcache {
	struct cachedglyph *data;
	int count;
	int capacity;
};

struct graphicscontext
{
	SDL_Window *window;
	SDL_Renderer *render;
	TTF_Font *fonteditor;
	TTF_Font *fontgutter;

	int width,height;
	float windowswidth,windowsheight;
	float refreshrate;

	char *filepath;
	char *filebuffer;
	size_t filesize;

	char *files[MAX_FILES];
	int filecount;

	SDL_FRect sidebar;
	float buttonheight;

	SDL_FRect sidebarbuttons[100]; // TODO: TUTA
	int sidebarbuttoncount;

	struct glyphcache glyphcache;

	bool dirtyframe;
};

typedef enum RenderUtilsMode {
	CALC_WINDOW,
	CALC_BUTTONS
} RenderUtilsMode;


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
currentdir(void *appstate)
{
	struct graphicscontext *context = appstate;
	if (!context) return;

	/* очистить старый список */
	for (int i = 0; i < context->filecount; i++)
		free(context->files[i]);

	context->filecount = 0;

	DIR *dir = opendir(".");
	if (!dir)
	{
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Не удалось открыть текущую директорию");
		return;
	}

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (context->filecount >= MAX_FILES)
			break;

		/* пропустить "." */
		if (strcmp(ent->d_name, ".") == 0)
			continue;

		context->files[context->filecount] = strdup(ent->d_name);
		context->filecount++;
	}

	closedir(dir);
}

void
changedir(const char *path, void *appstate)
{
	struct graphicscontext *context = appstate;
	if (!context || !path || !path[0])
		return;

	/* перейти в директорию */
	if (chdir(path) != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM,
								 "cd: не удалось перейти в '%s': %s",
								 path, strerror(errno));
		return;
	}

	/* получить новый абсолютный путь (опционально, если хранишь cwd) */
#ifdef PATH_MAX
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)))
		SDL_Log("cwd: %s", cwd);
#endif

	/* обновить список файлов (аналог ls) */
	currentdir(appstate);
}


void
graphics(void *appstate)
{
	struct graphicscontext *context = appstate;

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
renderutils(void *appstate, RenderUtilsMode mode)
{
	struct graphicscontext *context = appstate;
	if (!context) return;

	switch (mode)
	{
	case CALC_WINDOW:
	{
		int w, h;
		SDL_GetWindowSize(context->window, &w, &h);

		context->windowswidth  = (float)w;
		context->windowsheight = (float)h;
		break;
	}

	case CALC_BUTTONS:
	{
		float base = 28.0f;                 /* базовый размер кнопки */
		float desired = base * (float)ButtonSize;

		if (desired < 4.0f) desired = 4.0f; /* защита */

		int count = (int)(context->sidebar.h / desired);

		if (count < 1)   count = 1;
		if (count > 100) count = 100;

		context->sidebarbuttoncount = count;

		/* симметричная высота без остатка */
		context->buttonheight = context->sidebar.h / (float)count;
		break;
	}


	default:
		break;
	}
}


void
buildbuttons(void *appstate)
{
	struct graphicscontext *context = appstate;
	if (!context) return;

	int count = context->sidebarbuttoncount;
	if (count <= 0) return;
	if (count > 100) count = 100;

	float x = context->sidebar.x;
	float y = context->sidebar.y;
	float w = context->sidebar.w;

	/* базовая высота кнопки */

		for (int i = 0; i < count; i++)
		{
			//if (y + h > context->sidebar.y + context->sidebar.h)
			//	break;

			context->sidebarbuttons[i] = (SDL_FRect){
				x,
				y,
				w,
				context->buttonheight
		};

		y += context->buttonheight;
	}
}

void
renderhud(void *appstate)
{
	struct graphicscontext *context = appstate;

	renderutils(appstate, CALC_WINDOW);

	SDL_SetRenderDrawColor(context->render, 0, 0, 0, 255); /* цвет фона (черный) */
	SDL_RenderClear(context->render);

	context->sidebar = (SDL_FRect){
		0.0f,
		0.0f,
		context->windowswidth * SidebarRatio,
		context->windowsheight
	};

	renderutils(appstate, CALC_BUTTONS);

	//SDL_SetRenderDrawColor(context->render, SidebarColor);
	//SDL_RenderFillRect(context->render, &context->sidebar);

	buildbuttons(appstate);

	currentdir(appstate);

	int visible = context->sidebarbuttoncount;
	if (visible > context->filecount)
		visible = context->filecount;

	for (int i = 0; i < visible; i++)
	{
		SDL_FRect *r = &context->sidebarbuttons[i];

		SDL_SetRenderDrawColor(context->render, 60, 60, 60, 255);

		SDL_RenderFillRect(context->render, r);

		/* текст файла */
		// rendertext(context->files[i], r->x + 6, r->y + 4);
	}

	/*for (int i = 0; i < context->sidebarbuttoncount; i++)
	{
		SDL_FRect *r = &context->sidebarbuttons[i];

		if (i % 2 == 0)
			SDL_SetRenderDrawColor(context->render, 255, 255, 0, 255);  // жёлтый
		else
			SDL_SetRenderDrawColor(context->render, 255, 0, 0, 255);    // красный

		SDL_RenderFillRect(context->render, r);
	} */


	SDL_FRect gutter = {
		context->sidebar.w,
		0.0f,
		(context->windowswidth - context->sidebar.w) * GutterRatio,
		context->windowsheight
	};

	SDL_SetRenderDrawColor(context->render, GutterColor);
	SDL_RenderFillRect(context->render, &gutter);

	SDL_FRect editor = {
		gutter.x + gutter.w,
		0.0f,
		context->windowswidth - (gutter.x + gutter.w),
		context->windowsheight
	};

	SDL_SetRenderDrawColor(context->render, EditorColor);
	SDL_RenderFillRect(context->render, &editor);

	SDL_RenderPresent(context->render);
}

SDL_AppResult
SDL_AppInit(void **appstate, int argc, char *argv[])
{
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
	SDL_SetLogOutputFunction(loggingsystem, 0);

	struct graphicscontext *context = calloc(1, sizeof(*context));

	if (argc > 1 && argv[1] && argv[1][0] != '\0')
		context->filepath = argv[1];
	else
		context->filepath = "test.txt";


	context->dirtyframe = true;

	*appstate = context;

	graphics(context);
	return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate(void *appstate) //per frame
{
	renderhud(appstate);
	SDL_WaitEvent(NULL);
	return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppEvent(void *appstate, SDL_Event *event)
{
	struct graphicscontext *context = appstate;
	switch (event->type)
	{
	default:
		return SDL_APP_CONTINUE;

	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;

	case SDL_EVENT_WINDOW_EXPOSED:
	case SDL_EVENT_WINDOW_RESIZED:
		context->dirtyframe = true;
		break;


	}

	return SDL_APP_CONTINUE;
}

void
SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	struct graphicscontext *context = appstate;

	TTF_CloseFont(context->fonteditor);
	SDL_DestroyRenderer(context->render);
	SDL_DestroyWindow(context->window);

	SDL_Log("Выход из программы.");

	free(context);
	TTF_Quit();
	SDL_Quit();
}






void
do_tab(void *appstate)
{

}

void
do_backspace(void *appstate)
{

}
