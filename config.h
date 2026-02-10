//
// Created by ish3nn on 2/9/26.
//

#ifndef TEXTEDITOR_CONFIG_H
#define TEXTEDITOR_CONFIG_H

#define SIDEBAR_Color SDL_SetRenderDrawColor(context->render, 0, 0, 0, 255);
#define GUTTER_Color SDL_SetRenderDrawColor(context->render, 35, 35, 35, 255);
#define EDITOR_Color SDL_SetRenderDrawColor(context->render, 28, 28, 28, 255);
#define SYMBOL "↩"
#define WRAP_symbol SDL_Color color = {160,160,160,255};
#define NUM_Color SDL_Color color = {120,120,120,255};

#define FONT "InterVariable.ttf"

#define FONT_Color SDL_Color color = {255,255,255,255};
#define FONT_EDITOR TTF_OpenFont(FONT, 48);
#define FONT_GUTTER TTF_OpenFont(FONT, 38);

#endif // TEXTEDITOR_CONFIG_H
