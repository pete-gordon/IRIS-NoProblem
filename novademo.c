#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "novademo.h"
#include "asm6502.h"

uint8_t tapdata[65536];
uint32_t tapdata_used = 0;



void write_tapdata(void)
{
    FILE *f;
    uint8_t tap_header[] = { 0x16, 0x16, 0x16, 0x16, 0x24, 0x00, 0x00, 0x80, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const char *name = "NOVADEMO";

    tap_header[9]  = ((TAP_START + tapdata_used) >> 8);
    tap_header[10] = ((TAP_START + tapdata_used) & 0xff);
    tap_header[11] = (TAP_START >> 8);
    tap_header[12] = (TAP_START & 0xff);

    f = fopen(TAP_FILE, "wb");
    if (NULL == f)
    {
        fprintf(stderr, "Couldn't generate TAP file.\n");
        exit(EXIT_FAILURE);
    }

    if (1 != fwrite(tap_header, sizeof(tap_header), 1, f))
    {
        fclose(f);
        fprintf(stderr, "Couldn't generate TAP file.\n");
        exit(EXIT_FAILURE);
    }

    if (1 != fwrite(name, strlen(name)+1, 1, f))
    {
        fclose(f);
        fprintf(stderr, "Couldn't generate TAP file.\n");
        exit(EXIT_FAILURE);
    }

    if (1 != fwrite(&tapdata[TAP_START], tapdata_used, 1, f))
    {
        fclose(f);
        fprintf(stderr, "Couldn't generate TAP file.\n");
        exit(EXIT_FAILURE);
    }

    fclose(f);
}

int main(int argc, const char *argv[])
{
    uint16_t addr = TAP_START;
    assemble("    LDX #$00\n"
             "loop:\n"
             "    LDA hello,X\n"
             "    BEQ finish\n"
             "    JSR $CCFB\n"
             "    INX\n"
             "    JMP loop\n"
             "finish:\n"
             "    JMP finish\n"
             "hello:\n", tapdata, &addr);
    strcpy((char*)&tapdata[addr], "HELLO, WORLD");
    addr += 13;
    resolve_pending(tapdata, true);

    tapdata_used = addr - TAP_START;
    printf("tapdata_used is: %u\n", tapdata_used);
    write_tapdata();
    printf("SUCCESS!\n");
    return EXIT_SUCCESS;
}
