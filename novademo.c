#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

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

    f = fopen(SYM_FILE, "w");
    if (f)
    {
        dump_syms(f);
        fclose(f);
    }
}

void gen_scrwipe(uint16_t *addr)
{
    int i, j;
    uint8_t dunnit[28] = { 0, };

    // Copy the wipetab to zero page
    assemble("scrwipe:\n"
             "    LDA #40\n"
             "    STA ZPTMP\n"
             "    LDX #27\n"
             "scrwipe_copyloop:\n"
             "    LDA wipetab,X\n"
             "    STA ZPWIPETAB,X\n"
             "    LDA ZPTMP\n"
             "    STA ZPWIPEPOS,X\n"
             "    CLC\n"
             "    ADC #1\n"
             "    STA ZPTMP\n"
             "    DEX\n"
             "    BPL scrwipe_copyloop\n"
             "scrwipe_loop:\n"
             "    LDA #0\n"
             "    STA ZPTMP3\n"
             "    LDX #27\n"
             "scrwipe_frameloop:\n"
             "    LDY ZPWIPETAB,X\n"
             "    LDA ZPWIPEPOS,X\n"
             "    CMP #40\n"
             "    BCS scrwipe_skipline\n"
             "    CLC\n"
             "    ADC scrtab,Y\n"
             "    STA ZPTMP\n"
             "    INY\n"
             "    LDA scrtab,Y\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    LDY #0\n"
             "    LDA ZPTMP5\n"
             "    STA (ZPTMP),Y\n"
             "scrwipe_skipline:\n"
             "    LDY ZPWIPEPOS,X\n"
             "    BEQ scrwipe_skipdec\n"
             "    DEY\n"
             "    STY ZPWIPEPOS,X\n"
             "    INC ZPTMP3\n"
             "scrwipe_skipdec:\n"
             "    TXA\n"
             "    LSR\n"
             "    BCC scrwipe_singlespeed\n"
             "    LDA ZPTMP4\n"
             "    BNE scrwipe_singlespeed\n"
             "    INC ZPTMP4\n"
             "    JMP scrwipe_frameloop\n"
             "scrwipe_singlespeed:\n"
             "    LDA #0\n"
             "    STA ZPTMP4\n"
             "    DEX\n"
             "    BPL scrwipe_frameloop\n"
             "    LDX #20\n"
             "scrwipe_delay2:\n"
             "    LDY #0\n"
             "scrwipe_delay:\n"
             "    DEY\n"
             "    BNE scrwipe_delay\n"
             "    DEX\n"
             "    BNE scrwipe_delay2\n"
             "    LDA ZPTMP3\n"
             "    BNE scrwipe_loop\n"
             "    RTS\n"
             , tapdata, addr);

    sym_define("wipetab", *addr);
    for (i=0; i<28; i++)
    {
        j = rand() % 28;
        while (dunnit[j])
            j = ((j+1) % 28);
        dunnit[j] = 1;

        tapdata[(*addr)++] = j*2;
    }
}

#define SWOBCOLS  (39) /* column 0 reserved for colour set */
#define SWOBWIDTH (6*39)
#define SWOBMID   (SWOBWIDTH / 2)

#define STRIPEWOBBLE_MINSIZE (28)
#define STRIPEWOBBLE_MAXSIZE (60)
#define STRIPEWOBBLE_MIDDLE (STRIPEWOBBLE_MAXSIZE-STRIPEWOBBLE_MINSIZE)
#define STRIPEWOBBLE_SINTABSIZE (256)

void gen_stripewobblerline(int size, uint16_t *addr)
{
    char code[384];
    uint8_t colourtab[] = {4, 5, 6, 3, 7};
    int nextswap = (SWOBWIDTH-size)/2;  // we're doing half subpixel resolution, hence no
    int onoff, i, colidx;
    uint8_t thisbyte, lastbyte;

    colidx = ((size-STRIPEWOBBLE_MINSIZE) * sizeof(colourtab)) / (STRIPEWOBBLE_MAXSIZE-STRIPEWOBBLE_MINSIZE);
    if (colidx < 0)
        colidx = 0;
    if (colidx >= sizeof(colourtab))
        colidx = sizeof(colourtab)-1;

    /* Find first on/off toggle */
    onoff = 0;
    while (nextswap >= size)
    {
        onoff ^= 1;
        nextswap -= size;
    }

    snprintf(code, sizeof(code),
        "stripewobbler_line%d:\n" 
        "    LDY #0\n"
        "    LDA #%d\n"
        "    STA (ZPTMP),Y\n"
        "    INY\n", size, colourtab[colidx]);
    assemble(code, tapdata, addr);


    thisbyte = 0x40;
    lastbyte = 0x80;
    for (i=0; i<SWOBWIDTH; i++)
    {
        if (i >= nextswap)
        {
            onoff ^= 1;
            nextswap += size;
        }

        thisbyte |= (onoff << (5-(i%6)));

        if ((i%6) == 5)
        {
            if (thisbyte == lastbyte)
            {
                assemble("    STA (ZPTMP),Y\n"
                         "    INY\n", tapdata, addr);
            }
            else
            {
                snprintf(code, sizeof(code),
                    "    LDA #$%02x\n"
                    "    STA (ZPTMP),Y\n"
                    "    INY\n", thisbyte);
                assemble(code, tapdata, addr);
            }
            lastbyte = thisbyte;
            thisbyte = 0x40;
        }
    }

    assemble("   RTS", tapdata, addr);
}


void gen_stripewobbler(uint16_t *addr)
{
    int i;
    double ang, calc;
    uint16_t labaddr;
    char label[32];

    assemble("stripewobbler:\n"
             "    LDA #0\n"
             "    STA ZPSWFRAME\n"
             "    STA ZPSWFRAME2\n"
             "stripewobbler_loop:\n"
             "    LDA #2\n"
             "    CLC\n"
             "    ADC ZPSWFRAME\n"
             "    STA ZPSWFRAME\n"
             "    STA ZPSINPOS1\n"
             "    LDA ZPSWFRAME2\n"
             "    SEC\n"
             "    SBC #5\n"
             "    STA ZPSWFRAME2\n"
             "    STA ZPSINPOS2\n"
             "    LDA #0\n"
             "    STA ZPSWYLO\n"
             "    STA ZPSWYHI\n"
             "    STA ZPTMP\n"
             "    STA ZPSWTOG\n"
             "    LDA #$A0\n"
             "    STA ZPTMP2\n"
             "    LDX #200\n"
             "stripewobbler_drawloop:\n"
             "    LDY ZPSINPOS1\n"
             "    LDA stripewobbler_sintab,Y\n"
             "    INY\n"
             "    INY\n"
             "    STY ZPSINPOS1\n"
             "    LDY ZPSINPOS2\n"
             "    CLC\n"
             "    ADC stripewobbler_sintab,Y\n"
             "    DEY\n"
             "    DEY\n"
             "    DEY\n"
             "    DEY\n"
             "    DEY\n"
             "    STY ZPSINPOS2\n"
             "    ASL\n"
             "    TAY\n"
             "stripewobbler_notoggleyet:\n"
             "cw_smc_calc1:\n"
             "    LDA stripewobbler_table,Y\n"
             "    STA ZPTMP4\n"
             "    INY\n"
             "cw_smc_calc2:\n"
             "    LDA stripewobbler_table,Y\n"
             "    STA ZPTMP5\n"
             "    JSR stripewobbler_jumpo\n"
             "    CLC\n"
             "    LDA #40\n"
             "    ADC ZPTMP\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    DEX\n"
             "    BNE stripewobbler_drawloop\n"
             "    JMP stripewobbler_loop\n"
             "stripewobbler_jumpo:\n"
             "    JMP (ZPTMP4)\n"
             ,
             tapdata, addr);
    
    sym_define("stripewobbler_sintab", *addr);
    for (ang=0.0f, i=0; i<STRIPEWOBBLE_SINTABSIZE; i++, ang+=((2.0f*3.1419265f)/STRIPEWOBBLE_SINTABSIZE))
    {
        calc = (sin(ang) * (STRIPEWOBBLE_MIDDLE/2)) + (STRIPEWOBBLE_MIDDLE/2); /* + STRIPEWOBBLE_MINSIZE  would give us from MIN to MAX, but we actually want 0 to MAX-MIN */
        //printf("pos %d: ang = %u\n", i, (uint8_t)calc);
        tapdata[(*addr)++] = (uint8_t)(calc/2);
    }

    for (i=STRIPEWOBBLE_MINSIZE; i<STRIPEWOBBLE_MAXSIZE; i++)
        gen_stripewobblerline(i, addr);

    sym_define("stripewobbler_table", *addr);
    for (i=STRIPEWOBBLE_MINSIZE; i<STRIPEWOBBLE_MAXSIZE; i++)
    {
        snprintf(label, sizeof(label), "stripewobbler_line%d", i);
        labaddr = sym_get(label);
        tapdata[(*addr)++] = labaddr & 0xff;
        tapdata[(*addr)++] = labaddr >> 8;
    }
}


int main(int argc, const char *argv[])
{
    int i;
    uint16_t addr = TAP_START;

    srand(time(NULL));
    sym_define("ZPWIPETAB",   0);
    sym_define("ZPWIPEPOS",   28);
    sym_define("ZPTMP",       28*2);
    sym_define("ZPTMP2",      28*2+1);
    sym_define("ZPTMP3",      28*2+2);
    sym_define("ZPTMP4",      28*2+3);
    sym_define("ZPTMP5",      28*2+4);
    sym_define("ZPSWTOG",    0); /* re-use ZPWIPETAB */
    sym_define("ZPSWYLO",    1);
    sym_define("ZPSWYHI",    2);
    sym_define("ZPSWFRAME",  3);
    sym_define("ZPSWFRAME2", 3);
    sym_define("ZPSINPOS1",   4);
    sym_define("ZPSINPOS2",   5);

    assemble("demostart:\n"
             "    SEI\n"
             "    LDA #19\n"
             "    STA ZPTMP5\n"
             "    JSR scrwipe\n"
             "    LDA #16\n"
             "    STA ZPTMP5\n"
             "    JSR scrwipe\n"
             "    LDA #30\n" /* switch to hires */
             "    STA $BB80\n"
             "    JSR stripewobbler\n"
             "demoend:\n"
             "    JMP demoend\n", tapdata, &addr);

    gen_scrwipe(&addr);
    gen_stripewobbler(&addr);

    /* Define a handy table for text screen access */
    sym_define("scrtab", addr);
    resolve_pending(tapdata, false);

    for (i=0; i<28; i++)
    {
        tapdata[addr++] = (0xbb80 + (i*40)) & 0xff;
        tapdata[addr++] = (0xbb80 + (i*40)) >> 8;
    }

    resolve_pending(tapdata, true);
    tapdata_used = addr - TAP_START;
    printf("tapdata_used is: %u\n", tapdata_used);
    write_tapdata();
    printf("SUCCESS!\n");
    return EXIT_SUCCESS;
}
