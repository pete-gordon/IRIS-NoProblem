
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#define CHECK_RC(rc, func, args) if (rc != (func args )) { fprintf(stderr, "%s: %s failed\n", __FUNCTION__, #func); exit(EXIT_FAILURE); }
#define CHECK_ALLOC(store, func, args) if (NULL == (store = func args )) { fprintf(stderr, "%s: %s failed\n", __FUNCTION__, #func); exit(EXIT_FAILURE); }

#define MAX_FIXUPS (1024)

struct fixup
{
    char dstsym[128];
    char srcsym[128];
    int offset;
};

int num_fixups = 0;
struct fixup fixups[MAX_FIXUPS];

bool isbin(char c)
{
    return ((c >= '0') && (c <= '1'));
}

bool isnum(char c)
{
    return ((c >= '0') && (c <= '9'));
}

bool ishex(char c)
{
    if (isnum( c )) return true;
    if ((c >= 'a') && (c <= 'f')) return true;
    if ((c >= 'A') && (c <= 'F')) return true;
    return false;
}

bool isalph(char c)
{
    if ((c >= 'a') && (c <= 'z')) return true;
    if ((c >= 'A') && (c <= 'Z')) return true;
    return false;
}

bool issymstart(char c)
{
    return (isalph(c) || (c == '_') || (c == '.'));
}

bool issymchar(char c)
{
    return (issymstart(c) || (isnum(c)));
}

int main(int argc, const char *argv[])
{
    FILE *fh, *f;
    int v, v2, v3, v4, i, j;
    char line[1024];

    if (argc < 4)
    {
        fprintf(stderr, "Usage: musicconv <music_file> <music_config> <output_header>\n");
        return EXIT_FAILURE;
    }

    CHECK_ALLOC(fh, fopen, (argv[3], "w"));
    fprintf(fh, "#ifndef _MUSICHEADER__\n");
    fprintf(fh, "#define _MUSICHEADER__\n");

    fprintf(fh, "\n");
    fprintf(fh, "// Player config\n");

    CHECK_ALLOC(f, fopen, (argv[2], "r"));
    while (NULL != fgets(line, sizeof(line), f))
    {
        if (1 == sscanf(line, "\tPLY_CFG_ConfigurationIsPresent = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_ConfigurationIsPresent %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseTranspositions = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseTranspositions %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseHardwareSounds = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseHardwareSounds %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffects = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffects %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseInstrumentLoopTo = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseInstrumentLoopTo %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_NoSoftNoHard = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_NoSoftNoHard %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_NoSoftNoHard_Noise = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_NoSoftNoHard_Noise %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_SoftOnly = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_SoftOnly %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_SoftOnly_Noise = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_SoftOnly_Noise %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_SoftOnly_SoftwareArpeggio = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_SoftOnly_SoftwareArpeggio %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_SoftOnly_SoftwarePitch = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_SoftOnly_SoftwarePitch %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_SoftToHard = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_SoftToHard %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_SoftToHard_SoftwareArpeggio = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_SoftToHard_SoftwareArpeggio %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_Legato = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_Legato %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_Reset = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_Reset %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_PitchGlide = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_PitchGlide %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_PitchDown = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_PitchDown %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_PitchTable = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_PitchTable %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_ArpeggioTable = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_ArpeggioTable %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_SetVolume = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_SetVolume %u\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tPLY_CFG_UseEffect_VolumeOut = %u", &v))
        {
            fprintf(fh, "#define PLY_CFG_UseEffect_VolumeOut %u\n", v);
            continue;
        }
    }
    fclose(f);

    fprintf(fh, "\n");
    fprintf(fh, "// Music data\n");
    fprintf(fh, "const char *musicdata =\n");
    CHECK_ALLOC(f, fopen, (argv[1], "r"));
    while (NULL != fgets(line, sizeof(line), f))
    {
        char sym[128];
        bool wordsym = false;

        i=0;
        if (issymstart(line[0]))
        {
            while ((i < 128) && (issymchar(line[i])))
            {
                sym[i] = line[i];
                i++;
            }
            sym[i] = 0;
            if (strcmp(sym, "Main_Subsong0") == 0)
                fprintf(fh, "    \"%s:\\n\"\n", sym);
            else
                fprintf(fh, "    \".%s:\\n\"\n", sym);
        }

        if (4 == sscanf(line, "\t!byte %u, %u, %u, %u", &v, &v2, &v3, &v4))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v2);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v3);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v4);
            continue;
        }

        if (4 == sscanf(line, "\tdb %u, %u, %u, %u", &v, &v2, &v3, &v4))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v2);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v3);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v4);
            continue;
        }

        if (3 == sscanf(line, "\t!byte %u, %u, %u", &v, &v2, &v3))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v2);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v3);
            continue;
        }

        if (3 == sscanf(line, "\tdb %u, %u, %u", &v, &v2, &v3))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v2);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v3);
            continue;
        }

        if (2 == sscanf(line, "\t!byte %u, %u", &v, &v2))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v2);
            continue;
        }

        if (2 == sscanf(line, "\tdb %u, %u", &v, &v2))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            fprintf(fh, "    \"    !byte %u\\n\"\n", v2);
            continue;
        }

        if (1 == sscanf(line, "\t!byte %u", &v))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tdb %u", &v))
        {
            fprintf(fh, "    \"    !byte %u\\n\"\n", v);
            continue;
        }

        if (4 == sscanf(line, "\t!word %u, %u, %u, %u", &v, &v2, &v3, &v4))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            fprintf(fh, "    \"    !word %u\\n\"\n", v2);
            fprintf(fh, "    \"    !word %u\\n\"\n", v3);
            fprintf(fh, "    \"    !word %u\\n\"\n", v4);
            continue;
        }

        if (4 == sscanf(line, "\tdw %u, %u, %u, %u", &v, &v2, &v3, &v4))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            fprintf(fh, "    \"    !word %u\\n\"\n", v2);
            fprintf(fh, "    \"    !word %u\\n\"\n", v3);
            fprintf(fh, "    \"    !word %u\\n\"\n", v4);
            continue;
        }

        if (3 == sscanf(line, "\t!word %u, %u, %u", &v, &v2, &v3))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            fprintf(fh, "    \"    !word %u\\n\"\n", v2);
            fprintf(fh, "    \"    !word %u\\n\"\n", v3);
            continue;
        }

        if (3 == sscanf(line, "\tdw %u, %u, %u", &v, &v2, &v3))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            fprintf(fh, "    \"    !word %u\\n\"\n", v2);
            fprintf(fh, "    \"    !word %u\\n\"\n", v3);
            continue;
        }

        if (2 == sscanf(line, "\t!word %u, %u", &v, &v2))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            fprintf(fh, "    \"    !word %u\\n\"\n", v2);
            continue;
        }

        if (2 == sscanf(line, "\tdw %u, %u", &v, &v2))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            fprintf(fh, "    \"    !word %u\\n\"\n", v2);
            continue;
        }

        if (1 == sscanf(line, "\t!word %u", &v))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            continue;
        }

        if (1 == sscanf(line, "\tdw %u", &v))
        {
            fprintf(fh, "    \"    !word %u\\n\"\n", v);
            continue;
        }

        while (isspace(line[i]))
            i++;

        wordsym = false;
        if (strncmp(&line[i], "!word", 5) == 0)
        {
            i += 5;
            wordsym = true;
        }
        else if (strncmp(&line[i], "dw", 2) == 0)
        {
            i += 2;
            wordsym = true;
        }

        if (wordsym)
        {
            while (isspace(line[i]))
                i++;

            if (!issymstart(line[i]))
            {
                printf("Symbol expected: %s\n", line);
                exit(EXIT_FAILURE);
            }

            j = 0;
            while ((j < 128) && (issymchar(line[i])))
                sym[j++] = line[i++];
            sym[j] = 0;

            if (strcmp(sym, "Main_Subsong0") != 0)
            {
                memmove(&sym[1], sym, strlen(sym)+1);
                sym[0] = '.';
            }

            while (isspace(line[i]))
                i++;

            if (1 == sscanf(&line[i], "+ %u", &v))
            {
                fprintf(fh, "    \"    !word %s_pl%u\\n\"\n", sym, v);

                for (j=0; j<num_fixups; j++)
                {
                    if ((strcmp(fixups[j].srcsym, sym) == 0) &&
                        (fixups[j].offset == v))
                        break;
                }

                if (j == num_fixups)
                {
                    if (num_fixups >= MAX_FIXUPS)
                    {
                        fprintf(stderr, "TOO MANY FIXUPS\n");
                        exit(EXIT_FAILURE);
                    }

                    strcpy(fixups[num_fixups].srcsym, sym);
                    snprintf(fixups[num_fixups].dstsym, 128, "%s_pl%u", sym, v);
                    fixups[num_fixups++].offset = v;
                }
            } else {
                fprintf(fh, "    \"    !word %s\\n\"\n", sym);
            }
        }
    }

    fprintf(fh, "    \"\\n\";\n");
    fprintf(fh, "\n");
    fprintf(fh, "struct musicdata_fixup\n");
    fprintf(fh, "{\n");
    fprintf(fh, "    char *dstsym;\n");
    fprintf(fh, "    char *srcsym;\n");
    fprintf(fh, "    int offset;\n");
    fprintf(fh, "};\n");
    fprintf(fh, "\n");
    fprintf(fh, "struct musicdata_fixup musfixups[] =\n");
    fprintf(fh, "    {\n");
    for (i=0; i<num_fixups; i++)
    {
        fprintf(fh, "        {\n");
        fprintf(fh, "            .dstsym = \"%s\",\n", fixups[i].dstsym);
        fprintf(fh, "            .srcsym = \"%s\",\n", fixups[i].srcsym);
        fprintf(fh, "            .offset = %u\n", fixups[i].offset);
        fprintf(fh, "        },\n");
    }
    fprintf(fh, "    };\n");
    fprintf(fh, "\n");
    fprintf(fh, "#endif /* _MUSICHEADER__ */\n");
    fclose(fh);

    return EXIT_SUCCESS;
}