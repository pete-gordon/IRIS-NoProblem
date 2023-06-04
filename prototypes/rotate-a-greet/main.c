
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include <SDL/sdl.h>

#define ORIC_HIRES_W (240)
#define ORIC_HIRES_H (200)

#define SDL_W (ORIC_HIRES_W * 3)
#define SDL_H (ORIC_HIRES_H * 3)

#define CHECK_RC(rc, func, args) if (rc != (func args )) { fprintf(stderr, "%s: %s failed\n", __FUNCTION__, #func); goto error; }
#define CHECK_ALLOC(store, func, args) if (NULL == (store = func args )) { fprintf(stderr, "%s: %s failed\n", __FUNCTION__, #func); goto error; }

#define NUM_FRAMES (8)
#define STEP_PER_FRAME (3.14159265f / (2.0f * NUM_FRAMES))

SDL_Surface *srf = NULL;

uint8_t oric_screen[ORIC_HIRES_W * ORIC_HIRES_H];
uint8_t *greetingbmp = NULL;
size_t greetingbmpsize = 0;

uint8_t texture[(228/6) * 38];

int min_y, max_y, numrows = 0;
int min_w, max_w, wrange = 0;

struct row
{
    uint8_t txrow;
    int w;
};

struct wmap
{
    bool set;
    int colour;
    int idx;
    uint8_t bytoffs[240];
    uint8_t bit[240];  // bit "8" means not part of the texture, write 0
};

struct row *rows = NULL;
struct wmap *wmaps = NULL;

void render(int frame)
{
    uint8_t *src, *dst;
    int x, y, i, v, tx, ty, dx[2], width;
    int sx[3][2], sy[3][2];
    //static int min_x=ORIC_HIRES_W-1, min_y=ORIC_HIRES_H-1, max_x=0, max_y=0;
    double angstep, px[3][2], py[3][2], pz[3][2];
    struct row *r = NULL;
    struct wmap *w = NULL;

    memset(oric_screen, 0, sizeof(oric_screen));

/*
    for (y=0; y<38; y++)
    {
        for (x=0; x<228; x++)
        {
            if (texture[y*(228/6) + (x/6)] & (1<<(5-(x%6))))
                oric_screen[y*ORIC_HIRES_W+x] = 7;
        }
    }
*/

    frame = (frame % NUM_FRAMES);

    if (rows)
    {
        for (y=0; y<numrows; y++)
        {
            r = &rows[frame*numrows+y];
            r->txrow = 0xff;
            r->w = -1;
        }

        r = NULL;
    }

    for (angstep=(3.14159265f * 0.25f) + (frame * STEP_PER_FRAME), v=0; v<3; v++, angstep+=(3.14159265/2))
    {
        px[v][0] = -40.2f;
        py[v][0] = -sin(angstep) * 9.35f;
        pz[v][0] = (cos(angstep) * 9.35f) + 100.0f;

        px[v][1] = 42.2;
        py[v][1] = -sin(angstep) * 9.35f;
        pz[v][1] = (cos(angstep) * 9.35f) + 100.0f;

        sx[v][0] = ((int)(px[v][0] * 256.0f / pz[v][0] + (ORIC_HIRES_W/2.0f)))<<8;
        sy[v][0] = (int)(py[v][0] * 256.0f / pz[v][0] + (ORIC_HIRES_H/2.0f));

        sx[v][1] = ((int)(px[v][1] * 256.0f / pz[v][1] + (ORIC_HIRES_W/2.0f)))<<8;
        sy[v][1] = (int)(py[v][1] * 256.0f / pz[v][1] + (ORIC_HIRES_H/2.0f));
    }

    for (v=0; v<2; v++)
    {
        if (sy[v+1][0] <= sy[v][0])
            continue;
        dx[0] = (sx[v+1][0] - sx[v][0]) / (sy[v+1][0] - sy[v][0]);
        dx[1] = (sx[v+1][1] - sx[v][1]) / (sy[v+1][1] - sy[v][1]);

        ty = 0;
        for (y=sy[v][0]; y<sy[v+1][0]; y++)
        {
            width = (sx[v][1]-sx[v][0])>>8;
            if (rows)
            {
                r = &rows[frame * numrows + (y - min_y)];
                r->txrow = ty>>8;
                r->w = width-min_w;

                w = &wmaps[r->w];
                w->set = true;
                w->colour = 7;
            }
            else
            {
                if (y < min_y)
                    min_y = y;
                if (y > max_y)
                    max_y = y;

                if (width < min_w)
                    min_w = width;
                if (width > max_w)
                    max_w = width;
            }

            tx = 0;
            for (x=sx[v][0]>>8; x<sx[v][1]>>8; x++)
            {
                if (w)
                {
                    w->bytoffs[x] = ((tx>>8)/6);
                    w->bit[x] = 5-((tx>>8)%6);
                }
                if (texture[(ty>>8)*(228/6) + ((tx>>8)/6)] & (1<<(5-((tx>>8)%6))))
                    oric_screen[y*ORIC_HIRES_W+x] = 7;

                tx += (228<<8) / ((sx[v][1]-sx[v][0])>>8);
            }

            sx[v][0] += dx[0];
            sx[v][1] += dx[1];

            ty += (38<<8) / (sy[v+1][0] - sy[v][0]);
        }
    }

    src = oric_screen;
    for (y=0; y<ORIC_HIRES_H; y++)
    {
        dst = &((uint8_t *)srf->pixels)[y * 3 * srf->pitch];
        for (x=0; x<ORIC_HIRES_W; x++)
        {
            for (i=0; i<3; i++)
            {
                dst[srf->pitch] = *src;
                dst[srf->pitch*2] = *src;
                (*(dst++)) = *src;
            }
            src++;
        }
    }
}


void render_fromrows(int frame)
{
    uint8_t *src, *dst;
    int x, y, i;
    struct row *r = NULL;
    struct wmap *w = NULL;

    memset(oric_screen, 0, sizeof(oric_screen));

    frame = (frame % NUM_FRAMES);

    for (y=0; y<numrows; y++)
    {
        r = &rows[frame*numrows+y];
        if ((r->txrow == 0xff) || (r->w == -1))
            continue;

        w = &wmaps[r->w];
        for (x=0; x<240; x++)
        {
            if (w->bit[x] > 5)
                continue;
            if (texture[r->txrow*(228/6)+w->bytoffs[x]] & 1<<w->bit[x])
                oric_screen[y*ORIC_HIRES_W+x] = w->colour;
        }
    }

    src = oric_screen;
    for (y=0; y<ORIC_HIRES_H; y++)
    {
        dst = &((uint8_t *)srf->pixels)[y * 3 * srf->pitch];
        for (x=0; x<ORIC_HIRES_W; x++)
        {
            for (i=0; i<3; i++)
            {
                dst[srf->pitch] = *src;
                dst[srf->pitch*2] = *src;
                (*(dst++)) = *src;
            }
            src++;
        }
    }
}


SDL_bool init(void)
{
    FILE *f = NULL;
    int x, y, pitch;
    SDL_bool ret = SDL_FALSE;
    SDL_Color colours[8] =
        {
            { .r = 0x00, .g = 0x00, .b = 0x00 },
            { .r = 0xff, .g = 0x00, .b = 0x00 },
            { .r = 0x00, .g = 0xff, .b = 0x00 },
            { .r = 0xff, .g = 0xff, .b = 0x00 },
            { .r = 0x00, .g = 0x00, .b = 0xff },
            { .r = 0xff, .g = 0x00, .b = 0xff },
            { .r = 0x00, .g = 0xff, .b = 0xff },
            { .r = 0xff, .g = 0xff, .b = 0xff }
        };

    srand(time(NULL));

    CHECK_ALLOC(f, fopen, ("greetings.bmp", "rb"));
    fseek(f, 0, SEEK_END);
    greetingbmpsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    CHECK_ALLOC(greetingbmp, malloc, (greetingbmpsize));
    CHECK_RC(1, fread, (greetingbmp, greetingbmpsize, 1, f));
    fclose(f);
    f = NULL;

    memset(texture, 0, sizeof(texture));
    pitch = (((228+7)/8) + 3) & 0xfffffffc;
    for (y=0; y<38; y++)
    {
        for (x=0; x<228; x++)
        {
            if (greetingbmp[0x3e + (37-y)*pitch + (x/8)] & (1<<(7-(x&7))))
                texture[y * (228/6) + (x/6)] |= (1<<(5-(x%6)));
        }
    }


    CHECK_ALLOC(srf, SDL_SetVideoMode, (SDL_W, SDL_H, 8, SDL_HWPALETTE));
    SDL_SetPalette(srf, SDL_LOGPAL|SDL_PHYSPAL, colours, 0, 8);

    if (SDL_MUSTLOCK(srf))
        SDL_LockSurface(srf);

    render(0);

    if (SDL_MUSTLOCK(srf))
        SDL_UnlockSurface(srf);
    SDL_Flip(srf);

    /* Success */
    ret = SDL_TRUE;

error:
    if (f)
        fclose(f);
    return ret;
}

int main(int argc, char *argv[])
{
    SDL_bool done = SDL_FALSE;
    SDL_bool engrabbened = SDL_FALSE;
    int i, x, y, midx;
    FILE *f = NULL;
    char tmp[256];
    uint8_t cols[] = { 1, 1, 2, 5, 6, 6, 7, 7, 7 };

    if (!init())
        goto error;

    min_y = 200;
    max_y = 0;
    min_w = 240;
    max_w = 0;

    for (i=0; i<NUM_FRAMES; i++)
        render(i);

    numrows = (max_y-min_y)+1;
    wrange = (max_w-min_w)+1;

    snprintf(tmp, sizeof(tmp), "numrows = %u, wrange = %u\n", numrows, wrange);
    SDL_WM_SetCaption(tmp, tmp);

    rows = malloc(sizeof(struct row) * numrows * NUM_FRAMES);
    wmaps = malloc(sizeof(struct wmap) * wrange);

    midx = 0;
    for (i=0; i<wrange; i++)
    {
        wmaps[i].set = false;
        memset(&wmaps[i].bytoffs[0], 0, 240);
        memset(&wmaps[i].bit[0], 8, 240);
    }

    for (i=0; i<NUM_FRAMES; i++)
        render(i);

    for (i=0; i<wrange; i++)
    {
        if (!wmaps[i].set)
            continue;

        wmaps[i].colour = cols[(i * sizeof(cols)) / wrange];
    }

    f = fopen("../../rotatogreet_tables.h", "w");
    fprintf(f, "#ifndef _ROTATOGREET_TABLES_H__\n");
    fprintf(f, "#define _ROTATOGREET_TABLES_H__\n");
    fprintf(f, "\n");
    fprintf(f, "struct rogmap\n");
    fprintf(f, "{\n");
    fprintf(f, "    uint8_t colour;\n");
    fprintf(f, "    uint8_t bytoffs[240];\n");
    fprintf(f, "    uint8_t bit[240];\n");
    fprintf(f, "};\n");
    fprintf(f, "\n");
    fprintf(f, "struct rogrow\n");
    fprintf(f, "{\n");
    fprintf(f, "    uint8_t txrow;\n");
    fprintf(f, "    int mapi;\n");
    fprintf(f, "};\n");
    fprintf(f, "\n");
    for (i=0; i<wrange; i++)
    {
        if (!wmaps[i].set)
            continue;

        fprintf(f, "const struct rogmap rotatogreet_map%u =\n", i);
        fprintf(f, "    {\n");
        fprintf(f, "        .colour = %u,\n", wmaps[i].colour);
        fprintf(f, "        .bytoffs =\n");
        fprintf(f, "            {");
        for (x=0; x<240; x++)
        {
            if ((x%16)==0)
                fprintf(f, "\n                ");
            fprintf(f, "0x%02x, ", wmaps[i].bytoffs[x]);
        }
        fprintf(f, "\n");
        fprintf(f, "            },\n");
        fprintf(f, "        .bit =\n");
        fprintf(f, "            {");
        for (x=0; x<240; x++)
        {
            if ((x%16)==0)
                fprintf(f, "\n                ");
            fprintf(f, "0x%02x, ", wmaps[i].bit[x]);
        }
        fprintf(f, "\n");
        fprintf(f, "            },\n");
        fprintf(f, "    };\n");
        fprintf(f, "\n");
    }

    fprintf(f, "const struct rogmap *rotatogreet_maps[] =\n");
    fprintf(f, "    {");
    for (i=0, x=0; i<wrange; i++)
    {
        if (!wmaps[i].set)
            continue;

        wmaps[i].idx = midx++;

        if ((x%4)==0)
            fprintf(f, "\n        ");

        fprintf(f, "&rotatogreet_map%u, ", i);
        x++;
    }
    fprintf(f, "\n    };\n");
    fprintf(f, "\n");
    fprintf(f, "struct rogrow rotatogreet_frames[%u][%u] =\n", NUM_FRAMES, numrows);
    fprintf(f, "    {\n");
    for (i=0; i<NUM_FRAMES; i++)
    {
        fprintf(f, "        {\n");
        for (y=0; y<numrows; y++)
        {
            fprintf(f, "            {\n");
            fprintf(f, "                .txrow = %u,\n", rows[i*numrows+y].txrow);
            if (rows[i*numrows+y].txrow != 0xff)
                fprintf(f, "                .mapi = %u,\n", wmaps[rows[i*numrows+y].w].idx);
            else
                fprintf(f, "                .mapi = -1,\n");
            fprintf(f, "            },\n");
        }
        fprintf(f, "        },\n");
    }
    fprintf(f, "    };\n");
    fprintf(f, "\n");
    fprintf(f, "uint8_t rotatogreet_testtx[] =\n");
    fprintf(f, "    {");
    for (i=0; i<sizeof(texture); i++)
    {
        if ((i%16)==0)
            fprintf(f, "\n        ");
        fprintf(f, "0x%02x, ", texture[i]);
    }
    fprintf(f, "\n");
    fprintf(f, "    };\n");
    fprintf(f, "\n");
    fprintf(f, "#endif /* _ROTATOGREET_TABLES_H__ */\n");
    fclose(f);

    i = 0;

    while (!done)
    {
        SDL_Event event;
//      SDL_WaitEvent(&event);

        while (SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                case SDL_QUIT:
                    done = SDL_TRUE;
                    break;

//              case SDL_MOUSEMOTION:
//                  offsx += event.motion.xrel;
//                  offsy += event.motion.yrel;
//                  break;

                  case SDL_KEYDOWN:
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_EQUALS:
                        case SDLK_PLUS:
                            i = (i+1) % NUM_FRAMES;
                            break;

                        case SDLK_UNDERSCORE:
                        case SDLK_MINUS:
                            i = (i+(NUM_FRAMES-1)) % NUM_FRAMES;
                            break;

                        case SDLK_SPACE:
                            engrabbened = !engrabbened;
                            if (engrabbened)
                            {
                                SDL_ShowCursor(SDL_FALSE);
                                SDL_WM_GrabInput(SDL_GRAB_ON);
                            } else {
                                SDL_ShowCursor(SDL_TRUE);
                                SDL_WM_GrabInput(SDL_GRAB_OFF);
                            }
                            break;

                        case SDLK_ESCAPE:
                            done = SDL_TRUE;
                            break;

                        default:
                            break;
                    }
                    break;
            }
        };

        if (SDL_MUSTLOCK(srf))
            SDL_LockSurface(srf);

        render_fromrows(i);

        if (SDL_MUSTLOCK(srf))
            SDL_UnlockSurface(srf);

        SDL_Flip(srf);
    }

error:
    if (greetingbmp)
        free(greetingbmp);
    SDL_Quit();
    return EXIT_SUCCESS;
}
