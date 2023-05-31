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

#define CHWOBCOLS  (39) /* column 0 reserved for colour set */
#define CHWOBWIDTH (6*39)
#define CHWOBMID   (CHWOBWIDTH / 2)

#define CHESSWOBBLE_MINSIZE (24)
#define CHESSWOBBLE_MAXSIZE (40)
#define CHESSWOBBLE_MIDDLE (CHESSWOBBLE_MAXSIZE-CHESSWOBBLE_MINSIZE)
#define CHESSWOBBLE_SINTABSIZE (256)

void gen_chesswobblerline(int size, uint16_t *addr, int inverse)
{
    char code[384];
    uint8_t colourtab[] = {4, 5, 6, 3, 7};
    int nextswap = (CHWOBWIDTH-size)/2;
    int onoff, i, colidx;
    uint8_t thisbyte, lastbyte;

    colidx = ((size-CHESSWOBBLE_MINSIZE) * sizeof(colourtab)) / (CHESSWOBBLE_MAXSIZE-CHESSWOBBLE_MINSIZE);
    if (colidx < 0)
        colidx = 0;
    if (colidx >= sizeof(colourtab))
        colidx = sizeof(colourtab)-1;

    /* Find first on/off toggle */
    onoff = inverse;
    while (nextswap >= size)
    {
        onoff ^= 1;
        nextswap -= size;
    }

    snprintf(code, sizeof(code),
        "chesswobbler_line%d%s:\n" 
        "    LDY #0\n"
        "    LDA #%d\n"
        "    STA (ZPTMP),Y\n"
        "    INY\n", size, inverse ? "i" : "", colourtab[colidx]);
    assemble(code, tapdata, addr);


    thisbyte = 0x40;
    lastbyte = 0x80;
    for (i=0; i<CHWOBWIDTH; i++)
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


void gen_chesswobbler(uint16_t *addr)
{
    int i;
    double ang, calc;
    uint16_t labaddr;
    char label[32];
    uint16_t smcaddr;

    assemble("chesswobbler:\n"
             "    LDA #0\n"
             "    STA ZPCHWFRAME\n"
             "    STA ZPCHWFRAME2\n"
             "chesswobbler_loop:\n"
             "    LDA #2\n"
             "    CLC\n"
             "    ADC ZPCHWFRAME\n"
             "    STA ZPCHWFRAME\n"
             "    STA ZPSINPOS1\n"
             "    LDA ZPCHWFRAME2\n"
             "    SEC\n"
             "    SBC #5\n"
             "    STA ZPCHWFRAME2\n"
             "    STA ZPSINPOS2\n"
             "    LDA #0\n"
             "    STA ZPCHWYLO\n"
             "    STA ZPCHWYHI\n"
             "    STA ZPTMP\n"
             "    STA ZPCHWTOG\n"
             "    LDA #$A0\n"
             "    STA ZPTMP2\n"
             "    LDX #200\n"
             "chesswobbler_drawloop:\n"
             "    LDY ZPSINPOS1\n"
             "    LDA chesswobbler_sintab,Y\n"
             "    INY\n"
             "    INY\n"
             "    STY ZPSINPOS1\n"
             "    LDY ZPSINPOS2\n"
             "    CLC\n"
             "    ADC chesswobbler_sintab,Y\n"
             "    DEY\n"
             "    DEY\n"
             "    DEY\n"
             "    DEY\n"
             "    DEY\n"
             "    STY ZPSINPOS2\n"
             "    ASL\n"
             "    TAY\n"
             "    CLC\n"
             "    ADC ZPCHWYLO\n"
             "    STA ZPCHWYLO\n"
             "    LDA ZPCHWYHI\n"
             "    ADC #0\n"
             "    STA ZPCHWYHI\n"
             "    BEQ chesswobbler_notoggleyet\n"
             "    LDA #0\n"
             "    STA ZPCHWYLO\n"
             "    STA ZPCHWYHI\n"
             "    LDA ZPCHWTOG\n"
             "    EOR #1\n"
             "    STA ZPCHWTOG\n"
             "    BEQ chesswobbler_tog0\n"
             "    LDA #>chesswobbler_table_inv\n"
             "    STA chesswobbler_smc1lo\n"
             "    STA chesswobbler_smc2lo\n"
             "    LDA #<chesswobbler_table_inv\n"
             "    STA chesswobbler_smc1hi\n"
             "    STA chesswobbler_smc2hi\n"
             "    JMP chesswobbler_notoggleyet\n"
             "chesswobbler_tog0:\n"
             "    LDA #>chesswobbler_table\n"
             "    STA chesswobbler_smc1lo\n"
             "    STA chesswobbler_smc2lo\n"
             "    LDA #<chesswobbler_table\n"
             "    STA chesswobbler_smc1hi\n"
             "    STA chesswobbler_smc2hi\n"
             "chesswobbler_notoggleyet:\n"
             "cw_smc_calc1:\n"
             "    LDA chesswobbler_table,Y\n"
             "    STA ZPTMP4\n"
             "    INY\n"
             "cw_smc_calc2:\n"
             "    LDA chesswobbler_table,Y\n"
             "    STA ZPTMP5\n"
             "    JSR chesswobbler_jumpo\n"
             "    CLC\n"
             "    LDA #40\n"
             "    ADC ZPTMP\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    DEX\n"
             "    BNE chesswobbler_drawloop\n"
             "    JMP chesswobbler_loop\n"
             "chesswobbler_jumpo:\n"
             "    JMP (ZPTMP4)\n"
             ,
             tapdata, addr);
    
    smcaddr = sym_get("cw_smc_calc1");
    sym_define("chesswobbler_smc1lo", smcaddr+1);
    sym_define("chesswobbler_smc1hi", smcaddr+2);
    smcaddr = sym_get("cw_smc_calc2");
    sym_define("chesswobbler_smc2lo", smcaddr+1);
    sym_define("chesswobbler_smc2hi", smcaddr+2);

    sym_define("chesswobbler_sintab", *addr);
    for (ang=0.0f, i=0; i<CHESSWOBBLE_SINTABSIZE; i++, ang+=((2.0f*3.1419265f)/CHESSWOBBLE_SINTABSIZE))
    {
        calc = (sin(ang) * (CHESSWOBBLE_MIDDLE/2)) + (CHESSWOBBLE_MIDDLE/2); /* + CHESSWOBBLE_MINSIZE  would give us from MIN to MAX, but we actually want 0 to MAX-MIN */
        //printf("pos %d: ang = %u\n", i, (uint8_t)calc);
        tapdata[(*addr)++] = (uint8_t)(calc/2);
    }

    for (i=CHESSWOBBLE_MINSIZE; i<CHESSWOBBLE_MAXSIZE; i++)
        gen_chesswobblerline(i, addr, 0);

    for (i=CHESSWOBBLE_MINSIZE; i<CHESSWOBBLE_MAXSIZE; i++)
        gen_chesswobblerline(i, addr, 1);

    /* We build the table inverse since it makes the chess Y-toggle faster to calculate */
    sym_define("chesswobbler_table", *addr);
    for (i=CHESSWOBBLE_MAXSIZE-1; i>=CHESSWOBBLE_MINSIZE; i--)
    {
        snprintf(label, sizeof(label), "chesswobbler_line%d", i);
        labaddr = sym_get(label);
        tapdata[(*addr)++] = labaddr & 0xff;
        tapdata[(*addr)++] = labaddr >> 8;
    }

    sym_define("chesswobbler_table_inv", *addr);
    for (i=CHESSWOBBLE_MAXSIZE-1; i>=CHESSWOBBLE_MINSIZE; i--)
    {
        snprintf(label, sizeof(label), "chesswobbler_line%di", i);
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
    sym_define("ZPCHWTOG",    0); /* re-use ZPWIPETAB */
    sym_define("ZPCHWYLO",    1);
    sym_define("ZPCHWYHI",    2);
    sym_define("ZPCHWFRAME",  3);
    sym_define("ZPCHWFRAME2", 3);
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
             "    JSR chesswobbler\n"
             "demoend:\n"
             "    JMP demoend\n", tapdata, &addr);

    gen_scrwipe(&addr);
    gen_chesswobbler(&addr);

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
