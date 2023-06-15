
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

SDL_Surface *srf = NULL;

uint8_t oric_screen[ORIC_HIRES_W * ORIC_HIRES_H];

#define BALL_W (28)
#define BALL_H (40) /* Make the ball slightly tall to compensate for the weird oric aspect ratio */

uint8_t balltable[BALL_H][4], balltable_offs3[BALL_H][4];
uint8_t ballidx[BALL_H];
int table_max = BALL_H;

int linestab[16][16];

void render(void)
{
    uint8_t *src, *dst;
    int x, y, i;
    static int spacezoom = 0;
    memset(oric_screen, 0, sizeof(oric_screen));

    for (y=0; y<BALL_H; y++)
    {
        x = balltable[ballidx[y]][0]*6;
        for (i=0; i<6; i++)
        {
            oric_screen[y*ORIC_HIRES_W+x+i] = (balltable[ballidx[y]][1]&(1<<(5-i))) ? 6 : 0;
            oric_screen[((BALL_H*2-1)-y)*ORIC_HIRES_W+x+i] = (balltable[ballidx[y]][1]&(1<<(5-i))) ? 6 : 0;
        }

        x+=6;
        for (i=0; i<balltable[ballidx[y]][2]*6; i++)
        {
            oric_screen[y*ORIC_HIRES_W+x+i] = 6;
            oric_screen[((BALL_H*2-1)-y)*ORIC_HIRES_W+x+i] = 6;
        }

        x += i;
        for (i=0; i<6; i++)
        {
            oric_screen[y*ORIC_HIRES_W+x+i] = (balltable[ballidx[y]][3]&(1<<(5-i))) ? 6 : 0;
            oric_screen[((BALL_H*2-1)-y)*ORIC_HIRES_W+x+i] = (balltable[ballidx[y]][3]&(1<<(5-i))) ? 6 : 0;
        }
    }

    for (y=0; y<BALL_H; y++)
    {
        x = balltable_offs3[ballidx[y]][0]*6;
        for (i=0; i<6; i++)
        {
            oric_screen[(y+60)*ORIC_HIRES_W+x+i] = (balltable_offs3[ballidx[y]][1]&(1<<(5-i))) ? 6 : 0;
            oric_screen[(((BALL_H*2-1)-y)+60)*ORIC_HIRES_W+x+i] = (balltable_offs3[ballidx[y]][1]&(1<<(5-i))) ? 6 : 0;
        }

        x+=6;
        for (i=0; i<balltable_offs3[ballidx[y]][2]*6; i++)
        {
            oric_screen[(y+60)*ORIC_HIRES_W+x+i] = 6;
            oric_screen[(((BALL_H*2-1)-y)+60)*ORIC_HIRES_W+x+i] = 6;
        }

        x += i;
        for (i=0; i<6; i++)
        {
            oric_screen[(y+60)*ORIC_HIRES_W+x+i] = (balltable_offs3[ballidx[y]][3]&(1<<(5-i))) ? 6 : 0;
            oric_screen[(((BALL_H*2-1)-y)+60)*ORIC_HIRES_W+x+i] = (balltable_offs3[ballidx[y]][3]&(1<<(5-i))) ? 6 : 0;
        }
    }

    for (i=0; i<12; i++)
    {
        if ((linestab[spacezoom/16][i] < 0) || (linestab[spacezoom/16][i] >= ORIC_HIRES_H))
        {
            printf("Line %d = %d\n", i, linestab[spacezoom/16][i]);
            continue;
        }

        for (x=0; x<ORIC_HIRES_W; x++)
        {
            oric_screen[linestab[spacezoom/16][i] * ORIC_HIRES_W + x] = 5;
        }
    }
    spacezoom = (spacezoom+1) % (16*16);

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


void balltable_calcline(uint8_t *tabptr, int l, int r)
{
    int x;

    tabptr[0] = l / 6; /* Value 0 is number of bytes to left of ball */
    x = 1 << (5 - (l % 6));   /* Value 1 is left edge pattern of ball */
    tabptr[1] = 0;
    while (x != 0)
    {
        tabptr[1] |= x;
        x >>= 1;
    }

    tabptr[2] = (r/6)-(l/6);

    tabptr[3] = 0;
    x = 0x20;
    while (x > (1 << (5 - (r % 6))))
    {
        tabptr[3] |= x;
        x >>= 1;
    }
}

SDL_bool init(void)
{
    int x, y, z, i, j, k, oldy;
    double ang;
    int ball_l[BALL_H] = { 0, };
    int ball_r[BALL_H] = { 0, };

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

    ang = (3.14159265f/2.0f);
    oldy = -1000;
    while (ang >= 0.0f)
    {
        x = (int)(-sin(ang) * BALL_W * 1.01f) + BALL_W;
        y = (int)(-cos(ang) * BALL_H * 1.01f) + BALL_H;

        if (x < 0) x = 0;
        if (y < 0) y = 0;

        if (oldy != y)
        {
            printf("y = %d\n", y);
            ball_l[y] = x;
            ball_r[y] = (BALL_W*2)-x;
            oldy = y;
        }

        ang -= 0.01f;
    }

    for (y=0; y<BALL_H; y++)
    {
        balltable_calcline(&balltable[y][0], ball_l[y], ball_r[y]);
        balltable_calcline(&balltable_offs3[y][0], ball_l[y]+3, ball_r[y]+3);
    }

    for (i=0; i<BALL_H; i++)
        ballidx[i] = i;

    i=0;
    j=0;
    table_max = BALL_H;
    while (i < (table_max-1))
    {
        if (memcmp(&balltable[i][0], &balltable[i+1][0], 4) != 0)
        {
            printf("%u != %u, continuing\n", i, i+1);
            i++;
            j++;
            continue;
        }

        memmove(&balltable[i][0], &balltable[i+1][0], 4 * (table_max-i));
        memmove(&balltable_offs3[i][0], &balltable_offs3[i+1][0], 4 * (table_max-i));
        for (k=j+1; k<BALL_H; k++)
            ballidx[k]--;

        j++;
        table_max--;
    }

    k = ORIC_HIRES_H;
    for (j=0; j<16; j++)
    {
        for (i=0, z=256; i<12; z-=16, i++)
        {
            linestab[j][i] = 7680/(z-j);
        }

        if (linestab[j][0] < k)
            k = linestab[j][0];
    }

    for (j=0; j<16; j++)
    {
        for (i=0; i<12; i++)
        {
            linestab[j][i] -= k;
        }
    }

    srand(time(NULL));

    CHECK_ALLOC(srf, SDL_SetVideoMode, (SDL_W, SDL_H, 8, SDL_HWPALETTE));
    SDL_SetPalette(srf, SDL_LOGPAL|SDL_PHYSPAL, colours, 0, 8);

    if (SDL_MUSTLOCK(srf))
        SDL_LockSurface(srf);

    render();

    if (SDL_MUSTLOCK(srf))
        SDL_UnlockSurface(srf);
    SDL_Flip(srf);

    /* Success */
    ret = SDL_TRUE;

error:
    return ret;
}

int main(int argc, char *argv[])
{
    SDL_bool done = SDL_FALSE;
    SDL_bool engrabbened = SDL_FALSE;
    int i, j;
    FILE *f = NULL;

    if (!init())
        goto error;

    f = fopen("../../bouncy_tables.h", "w");
    fprintf(f, "#ifndef _BOUNCY_TABLES_H__\n");
    fprintf(f, "#define _BOUNCY_TABLES_H__\n");
    fprintf(f, "\n");
    fprintf(f, "#define BALL_W (%u)\n", BALL_W);
    fprintf(f, "#define BALL_H (%u)\n", BALL_H);
    fprintf(f, "\n");
    fprintf(f, "uint8_t balltab[%u][8] =\n", table_max);
    fprintf(f, "    {\n");
    for (i=0; i<table_max; i++)
    {
        fprintf(f, "        { 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x },\n",
            balltable[i][0], balltable[i][1] | 0x40, balltable[i][2], balltable[i][3] | 0x40,
            balltable_offs3[i][0], balltable_offs3[i][1] | 0x40, balltable_offs3[i][2], balltable_offs3[i][3] | 0x40);
    }
    fprintf(f, "    };\n");
    fprintf(f, "\n");
    fprintf(f, "\n");
    fprintf(f, "uint8_t ballidx[BALL_H] =\n");
    fprintf(f, "    {\n");
    for (i=0; i<BALL_H; i++)
    {
        if ((i%16) == 0)
            fprintf(f, "\n        ");
        fprintf(f, "0x%02x, ", ballidx[i]);
    }
    fprintf(f, "\n    };\n");
    fprintf(f, "\n");
    fprintf(f, "uint8_t linestab[16][12] =\n");
    fprintf(f, "    {\n");
    for (i=0; i<16; i++)
    {
        fprintf(f, "        { ");
        for (j=0; j<12; j++)
            fprintf(f, "0x%02x, ", linestab[i][j]);
        fprintf(f, " },\n");
    }
    fprintf(f, "\n    };\n");
    fprintf(f, "#endif /* _BOUNCY_TABLES_H__ */\n");
    fclose(f);

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
//                        case SDLK_EQUALS:
//                        case SDLK_PLUS:
//                            i = (i+1) % NUM_FRAMES;
//                            break;
//
//                        case SDLK_UNDERSCORE:
//                        case SDLK_MINUS:
//                            i = (i+(NUM_FRAMES-1)) % NUM_FRAMES;
//                            break;
//
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

        render();

        if (SDL_MUSTLOCK(srf))
            SDL_UnlockSurface(srf);

        SDL_Flip(srf);
    }

error:
    SDL_Quit();
    return EXIT_SUCCESS;
}
