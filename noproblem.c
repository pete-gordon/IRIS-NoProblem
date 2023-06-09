#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "noproblem.h"
#include "asm6502.h"
#include "rotatogreet_tables.h"

uint8_t tapdata[65536];
uint32_t tapdata_used = 0;



void write_tapdata(void)
{
    FILE *f;
    uint8_t tap_header[] = { 0x16, 0x16, 0x16, 0x16, 0x24, 0x00, 0x00, 0x80, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const char *name = "NOPROBLEM";

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

#define STRIPEWOBBLE_MINSIZE (24)
#define STRIPEWOBBLE_MAXSIZE (56)
#define STRIPEWOBBLE_MIDDLE (STRIPEWOBBLE_MAXSIZE-STRIPEWOBBLE_MINSIZE)
#define STRIPEWOBBLE_SINTABSIZE (256)

void gen_stripewobblerline(int size, uint16_t *addr)
{
    char code[384];
    uint8_t colourtab[] = {0, 4, 0, 4, 4, 4, 5, 4, 5, 5, 5, 6, 5, 6, 6, 6, 3, 6, 3, 3, 3, 7, 3, 7, 7, 7, 7 };
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
        "    INY\n", size, (0 == colourtab[colidx]) ? 4 : colourtab[colidx]);
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

        if (0 != colourtab[colidx])
        {
            thisbyte |= (onoff << (5-(i%6)));
        }
        else
        {
            if (onoff)
            {
                thisbyte |= ((i&1) << (5-(i%6)));
            }
        }

        if ((i%6) == 5)
        {
            if (thisbyte == lastbyte)
            {
                snprintf(code, sizeof(code),
                    "    STA (ZPTMP),Y\n"
                    "%s", (i==(SWOBWIDTH-1)) ? "" : "    INY");
                assemble(code, tapdata, addr);
            }
            else
            {
                snprintf(code, sizeof(code),
                    "    LDA #$%02x\n"
                    "    STA (ZPTMP),Y\n"
                    "%s", thisbyte, (i==(SWOBWIDTH-1)) ? "" : "    INY");
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
    uint16_t labaddr, smcaddr;
    char label[32];

    assemble(".swfinish:\n"
             "    RTS\n"
             "stripewobbler:\n"
             "    LDA #0\n"
             "    STA ZPSWFRAME\n"
             "    STA ZPSWFRAME2\n"
             "    STA ZPSWTIME\n"
             "stripewobbler_loop:\n"
             "    LDY ZPSWTIME\n"
             "    INY\n"
             "    STY ZPSWTIME\n"
             "    BEQ .swfinish\n"
             "    LDA #3\n"
             "    CLC\n"
             "    ADC ZPSWFRAME\n"
             "    STA ZPSWFRAME\n"
             "    STA ZPSINPOS1\n"
             "    LDA ZPSWFRAME2\n"
             "    SEC\n"
             "    SBC #11\n"
             "    STA ZPSWFRAME2\n"
             "    STA ZPSINPOS2\n"
             "    LDA #0\n" 
             "    STA ZPTMP\n"
             "    LDA #$A0\n"
             "    STA ZPTMP2\n"
             "    LDX #200\n"
             "    CLC\n" // Nothing in the loop below should set C, so clear it now (once per frame), and avoid 2 CLCs per scanline later.
             "stripewobbler_drawloop:\n"
             "    LDY ZPSINPOS1\n"
             "    LDA stripewobbler_sintab,Y\n"
             "    INY\n"
             "    INY\n"
             "    STY ZPSINPOS1\n"
             "    LDY ZPSINPOS2\n"
             //"    CLC\n"
             "    ADC stripewobbler_sintab,Y\n" // shouldn't set C as max value fits
             "    DEY\n"
             "    DEY\n"
             "    DEY\n"
             "    STY ZPSINPOS2\n"
             //"    ASL\n"                        // didn't set C when present, but we've optimised it out by pre-multiplying the sintab by 2
             "    TAY\n"
             "    LDA stripewobbler_table,Y\n"
             "    STA stripewobbler_smc_lo\n"
             "    INY\n"
             "    LDA stripewobbler_table,Y\n"
             "    STA stripewobbler_smc_hi\n"
             "swob_smc_calc:\n"
             "    JSR $0000\n"                  // line routines don't set C
             //"    CLC\n"
             "    LDA #40\n"
             "    ADC ZPTMP\n"                  // This might set C...
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"                     // ... but this then consume s it
             "    STA ZPTMP2\n"
             "    DEX\n"
             "    BNE stripewobbler_drawloop\n"
             "    JMP stripewobbler_loop\n"
             ,
             tapdata, addr);

    smcaddr = sym_get("swob_smc_calc");
    sym_define("stripewobbler_smc_lo", smcaddr+1);
    sym_define("stripewobbler_smc_hi", smcaddr+2);

    sym_define("stripewobbler_sintab", *addr);
    for (ang=0.0f, i=0; i<STRIPEWOBBLE_SINTABSIZE; i++, ang+=((2.0f*3.1419265f)/STRIPEWOBBLE_SINTABSIZE))
    {
        calc = (sin(ang) * (STRIPEWOBBLE_MIDDLE/2)) + (STRIPEWOBBLE_MIDDLE/2); /* + STRIPEWOBBLE_MINSIZE  would give us from MIN to MAX, but we actually want 0 to MAX-MIN */
        //printf("pos %d: ang = %u\n", i, (uint8_t)calc);
        tapdata[(*addr)++] = ((uint8_t)(calc/2))*2; /* Pre-multiply by 2 as we use it to index into a jumptable */
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

struct rog_describe_bits
{
    int     numtxbytes;
    uint8_t bytoffs[8];
    uint8_t bits[8];
    int     shift[8];
    char    sym[MAX_SYMLEN];
    int     yinc;
};

void gen_rotatogreet(uint16_t *addr)
{
    char tmp[256];
    int i, j, x, y, srcoffs, bit, sh, curry;
    struct rog_describe_bits rdb[NUM_ENTRIES(rotatogreet_maps)][40];
    uint16_t smcaddr;
    bool ais40;
    FILE *f;
    uint8_t *greetfontbmp = NULL;
    size_t greetfontbmpsize = 0;
    uint8_t greetfont[(312*16)/6];
    int pitch;
    /* Re-use the memory from stripewobbler tables for the rotator texture memory */
    uint16_t txaddr = sym_get("stripewobbler");
    int steps[] = { 12, 12, 12, 8, 5, 3, 2, 1 };

    f = fopen("greetfont.bmp", "rb");
    if (NULL == f)
    {
        fprintf(stderr, "Failed to open greetfont.bmp\n");
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    greetfontbmpsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    greetfontbmp = malloc(greetfontbmpsize);
    if (NULL == greetfontbmp)
    {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        exit(EXIT_FAILURE);
    }

    fread(greetfontbmp, greetfontbmpsize, 1, f);
    fclose(f);
    f = NULL;

    memset(greetfont, 0, sizeof(greetfont));
    pitch = (((312+7)/8) + 3) & 0xfffffffc;
    for (y=0; y<16; y++)
    {
        for (x=0; x<312; x++)
        {
            if (greetfontbmp[0x3e + (15-y)*pitch + (x/8)] & (1<<(7-(x&7))))
                greetfont[y * (312/6) + (x/6)] |= (1<<(5-(x%6)));
        }
    }

    sym_define("rotatogreet_frametab", 0x0248);
    sym_define("rotatogreet_font", *addr);
    sym_define("rotatogreet_font_m35", (*addr)-35); /* Convenient offset during scroll function */
    memcpy(&tapdata[*addr], greetfont, sizeof(greetfont));
    *addr += sizeof(greetfont);

    memset(rdb, 0, sizeof(rdb));

    // Ensure tscoffs2 is on same page as 1
    if (((txaddr + (228/6)*11 +1) & 0xff00) !=
        ((txaddr + (228/6)*11 +2) & 0xff00))
        txaddr++;
    sym_define("rotatogreet_texture", txaddr);

    assemble("rotatogreet:\n"
             "    LDA #>rotatogreet_frametab\n"
             "    STA ZPTMP\n"
             "    LDA #<rotatogreet_frametab\n"
             "    STA ZPTMP2\n"
             "    LDY #15\n"
             "rog_ftab_loop1:\n"
             "    TYA\n"
             "    ASL\n"
             "    AND #15\n"
             "    STA (ZPTMP),Y\n"
             "    DEY\n"
             "    BPL rog_ftab_loop1\n"
             "    LDY #16\n"
             "rog_ftab_loop2:\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    CPY #32\n"
             "    BNE rog_ftab_loop2\n"
             "    LDX #23\n"
             "rog_ftab_loop3:\n"
             "    TXA\n"
             "    ASL\n"
             "    AND #15\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    DEX\n"
             "    BPL rog_ftab_loop3\n"
             "rog_ftab_loop4:\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    CPY #96\n"
             "    BNE rog_ftab_loop4\n"

             "    LDA #30\n"
             "    STA $A000\n"
             "    LDY #0\n"
             "    STY $A001\n"
             "    STY ZPROGMLEND\n"

             "    LDA #$A0\n"
             "    STA ZPTMP2\n"
             "    LDA #$28\n"
             "    STA ZPTMP\n"
             "    LDX #199\n"
             "rog_clrscr:\n"
             "    LDY #39\n"
             "    LDA #$40\n"
             ".rogclrit:\n"
             "    STA (ZPTMP),Y\n"
             "    DEY\n"
             "    BPL .rogclrit\n"
             "    LDA ZPTMP\n"
             "    CLC\n"
             "    ADC #40\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    DEX\n"
             "    BNE rog_clrscr\n"

             "    LDA #>rotatogreet_texture\n"
             "    STA ZPTMP\n"
             "    LDA #<rotatogreet_texture\n"
             "    STA ZPTMP2\n"
             "    LDX #37\n"
             "rog_cleartx_rowloop:\n"
             "    LDY #37\n"
             "    LDA #$3f\n"
             "rog_cleartx_colloop:\n"
             "    CPX #34\n"
             "    BCS .nozapit\n"
             "    CPX #4\n"
             "    BCC .nozapit\n"
             "    CPY #0\n"
             "    BEQ .rightedge\n"
             "    CPY #37\n"
             "    BNE .zapit\n"
             ".leftedge:\n"
             "    LDA #$0f\n"
             "    BNE .zonkedit\n"
             ".rightedge:\n"
             "    LDA #$3c\n"
             "    BNE .zonkedit\n"
             ".zapit:\n"
             "    LDA #0\n"
             ".zonkedit:\n"
             ".nozapit:\n"
             "    STA (ZPTMP),Y\n"
             "    DEY\n"
             "    BPL rog_cleartx_colloop\n"
             "    LDA ZPTMP\n"
             "    CLC\n"
             "    ADC #38\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    DEX\n"
             "    BPL rog_cleartx_rowloop\n"
             "    LDA #0\n"
             "    STA ZPROGSCHLF\n"
             "    STA ZPROGSCPOS\n"
             "    LDA #0\n"
             ".rog_initial_scrollin:\n"
             "    PHA\n"
             "    JSR rotatogreet_scrollit\n"
             "    PLA\n"
             "    ADC #0\n" // Currently C is always set on exit
             "    CMP #27\n"
             "    BNE .rog_initial_scrollin\n"

             "    LDX #$80\n"
             "    LDY #46\n"
             "rog_setup_lastfrm:\n"
             "    LDA #$BF\n"
             "    STA ZPROGLFRMH,Y\n"
             "    TXA\n"
             "    STA ZPROGLFRML,Y\n"
             "    DEY\n"
             "    BPL rog_setup_lastfrm\n"

             "rog_bringitin_loop:\n"
             "    LDY #0\n"
             "rog_linesin_buildtab:\n"
             "    LDA rotatogreet_cidxtab,Y\n", tapdata, addr);

    snprintf(tmp, sizeof(tmp),
             "    CMP #%u\n"
             "    BNE .doadd\n", NUM_ENTRIES(steps)*2 -2);
    assemble(tmp, tapdata, addr);

    assemble("    TAX\n"
             "    LDA #$80\n"
             "    STA ZPROGLFRML,Y\n"
             "    LDA #$BF\n"
             "    STA ZPROGLFRMH,Y\n"
             "    BNE .dontadd\n"

             ".doadd:\n"
             "    CLC\n"
             "    ADC #2\n"
             "    STA rotatogreet_cidxtab,Y\n"
             "    BMI .linesin_skipthis\n"

             "    TAX\n"
             "    LDA rog_linetab_lo,Y\n"
             "    STA ZPROGLFRML,Y\n"
             "    LDA rog_linetab_hi,Y\n"
             "    STA ZPROGLFRMH,Y\n"

             ".dontadd:\n"
             "    LDA rotatogreet_lineintab,X\n"
             "    CLC\n"
             "    ADC rotatogreet_offstab_lo,Y\n"
             "    STA rog_linetab_lo,Y\n"
             "    INX\n"
             "    LDA rotatogreet_lineintab,X\n"
             "    ADC rotatogreet_offstab_hi,Y\n"
             "    STA rog_linetab_hi,Y\n"
             ".linesin_skipthis:\n"
             "    INY\n"
             "    CPY #47\n"
             "    BNE rog_linesin_buildtab\n"
             "    JSR rotatogreet_lineinframe\n"
             "    LDA rotatogreet_cidxtab\n", tapdata, addr);
    snprintf(tmp, sizeof(tmp),
             "    CMP #%u\n"
             "    BNE rog_bringitin_loop\n", NUM_ENTRIES(steps)*2 -2);
    assemble(tmp, tapdata, addr);

    assemble("    LDY #0\n"
             "rotatogreet_mainloop:\n"
             "    LDA ZPROGMLEND\n"
             "    BNE .rollout\n"
             "    LDX rotatogreet_frametab,Y\n"
             "    LDA rotatogreet_frouts,X\n"
             "    STA rotatogreet_smc_lo\n"
             "    INX\n"
             "    LDA rotatogreet_frouts,X\n"
             "    STA rotatogreet_smc_hi\n"
             "    TYA\n"
             "    PHA\n"
             ".rog_smc_pos:\n"
             "    JSR rotatogreet_drawframe0\n"
             ".rog_smc_pos2:\n"
             "    JSR rotatogreet_scrollit\n"
             "    PLA\n"
             "    TAY\n"
             "    INY\n"
             "    CPY #96\n"
             "    BNE rotatogreet_mainloop\n"
             "    LDY #0\n"
             "    JMP rotatogreet_mainloop\n"
             ".rollout:\n"
             "    LDX rotatogreet_frametab,Y\n"
             ".rolloutloop:\n"
             "    LDA rotatogreet_frouts,X\n"
             "    STA rotatogreet_smc3_lo\n"
             "    INX\n"
             "    LDA rotatogreet_frouts,X\n"
             "    STA rotatogreet_smc3_hi\n"
             "    INX\n"
             "    TXA\n"
             "    AND #15\n"
             "    PHA\n"
             ".rog_smc_pos3:\n"
             "    JSR rotatogreet_drawframe0\n"
             "    JSR rotatogreet_rollaway\n"
             "    PLA\n"
             "    TAX\n"
             "    JMP .rolloutloop\n", tapdata, addr);

    smcaddr = sym_get(".rog_smc_pos");
    sym_define("rotatogreet_smc_lo", smcaddr+1);
    sym_define("rotatogreet_smc_hi", smcaddr+2);

    smcaddr = sym_get(".rog_smc_pos2");
    sym_define("rotatogreet_smc2_lo", smcaddr+1);
    sym_define("rotatogreet_smc2_hi", smcaddr+2);

    smcaddr = sym_get(".rog_smc_pos3");
    sym_define("rotatogreet_smc3_lo", smcaddr+1);
    sym_define("rotatogreet_smc3_hi", smcaddr+2);

    {
        int offs=0;

        sym_define("rotatogreet_cidxtab", *addr);
        for (i=0; i<47; i++)
            tapdata[(*addr)++] = ((i-46)*2) & 0xff;

        sym_define("rotatogreet_lineintab", *addr);
        for (i=0; i<NUM_ENTRIES(steps); i++)
        {
            offs += steps[i];
            tapdata[(*addr)++] = (0xa000 + offs * 40) & 0xff;
            tapdata[(*addr)++] = (0xa000 + offs * 40) >> 8;
        }

        sym_define("rotatogreet_offstab_lo", *addr);
        for (i=0; i<47; i++)
            tapdata[(*addr)++] = (i * 40) & 0xff;

        sym_define("rotatogreet_offstab_hi", *addr);
        for (i=0; i<47; i++)
            tapdata[(*addr)++] = (i * 40) >> 8;

        sym_define("rog_linetab_lo", *addr);
        memset(&tapdata[*addr], 0xe0, 47);
        *addr += 47;
        sym_define("rog_linetab_hi", *addr);
        memset(&tapdata[*addr], 0xbf, 47);
        *addr += 47;
    }
   

    {
        const char *str = "GREETINGS:DEFENCE FORCE:DEKADENCE:DARKAGE:LOONIES:RMC CAVE DWELLERS::IO PRINT:";

        sym_define("rotatogreet_str", *addr);
        for (i=0; str[i]; i++)
        {
            if ((str[i] >= 'A') && (str[i] <= 'Z'))
            {
                tapdata[(*addr)++] = (str[i]-'A') * 2;
                continue;
            }
            tapdata[(*addr)++] = (str[i]==':') ? 0xf7 : 0xff;
        }
        tapdata[(*addr)++] = 0xc0;
    }

    sym_define("rotatogreet_txctable_lo", *addr);
    for (i=0; i<16; i++)
    {
        tapdata[*addr] = (txaddr + (228/6)*11 + 1 + (38*(15-i))) & 0xff;
        if (tapdata[(*addr)++] == 255)
        {
            fprintf(stderr, "Page problem in txctable. Please adjust table location\n");
            exit(EXIT_FAILURE);
        }
    }

    sym_define("rotatogreet_txctable_hi", *addr);
    for (i=0; i<16; i++)
        tapdata[(*addr)++] = ((txaddr + (228/6)*11 + 1 + (38*(15-i)))>>8) & 0xff;

    assemble("rotatogreet_scrollit:\n"
             "    LDY ZPROGSCPOS\n"
             "    LDA rotatogreet_str,Y\n"
             "    BPL .normalchar\n"

             "    LDX #$A9\n"
             "    STX .rogscroll_smc1\n"
             "    LDX #00\n"
             "    STX .rogscroll_smc2\n"

             "    CMP #$C0\n"
             "    BNE rogscroll_isspace\n"

             "    STX ZPROG10PLO\n"
             "    LDA #>rotatogreet_10print\n"
             "    STA rotatogreet_smc2_lo\n"
             "    LDA #<rotatogreet_10print\n"
             "    STA rotatogreet_smc2_hi\n"
             "    RTS\n"

             "rogscroll_isspace:\n"
             "    CMP #$FF\n"
             "    BEQ rogscroll_isspace2\n"
             "    ADC #1\n"
             "    STA rotatogreet_str,Y\n"
             "    JMP rogscroll_nomoveon\n"

             ".normalchar:\n"
             "    LDX #$B1\n"
             "    STX .rogscroll_smc1\n"
             "    LDX #60\n"
             "    STX .rogscroll_smc2\n"

             "    ORA ZPROGSCHLF\n"
             "    CLC\n"
             "    ADC #>rotatogreet_font_m35\n"
             "    STA ZPTMP5\n"
             "    LDA #<rotatogreet_font_m35\n"
             "    ADC #0\n"
             "    STA ZPTMP6\n"

             "rogscroll_isspace2:\n"
             "    LDA ZPROGSCHLF\n"
             "    EOR #1\n"
             "    STA ZPROGSCHLF\n"
             "    BNE rogscroll_nomoveon\n"
             "    INY\n"
             "    STY ZPROGSCPOS\n"

             "rogscroll_nomoveon:\n"
             "    LDX #15\n"
             "rogscroll_rowloop:\n"
             "    LDY rotatogreet_txctable_lo,X\n"
             "    STY ZPTMP\n"
             "    INY\n"
             "    STY ZPTMP3\n"
             "    LDY rotatogreet_txctable_hi,X\n"
             "    STY ZPTMP2\n"
             "    STY ZPTMP4\n"
             "    LDY #0\n"
             "rogscroll_loop:\n"
             "    LDA (ZPTMP3),Y\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA (ZPTMP3),Y\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA (ZPTMP3),Y\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA (ZPTMP3),Y\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA (ZPTMP3),Y\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    CPY #35\n"
             "    BNE rogscroll_loop\n"
             ".rogscroll_smc1:\n"
             "    LDA (ZPTMP5),Y\n"
             "    STA (ZPTMP),Y\n"
             "    DEX\n"
             "    BMI .done\n"
             "    LDA ZPTMP5\n"
             "    CLC\n"
             "    ADC #52\n"
             "    STA ZPTMP5\n"
             "    BCC rogscroll_rowloop\n"
             "    LDY ZPTMP6\n"
             "    INY\n"
             "    STY ZPTMP6\n"
             "    BPL rogscroll_rowloop\n"
             ".done:\n"
             "    RTS\n", tapdata, addr);

    assemble("rotatogreet_10print:\n"
             "    LDA ZPROG10PLO\n"
             "    BNE .alreadysetup\n"
             "    LDA #0\n"
             "    STA ZPROG10PCT\n"
             "    LDA #<rotatogreet_texture\n"
             "    STA ZPROG10PHI\n"
             "    LDA #>rotatogreet_texture\n"
             "    STA ZPROG10PLO\n"

             "    LDY #5\n"
             "    LDA #$30\n"
             ".genloop1:\n"
             "    STA ZPROGFSLSH,Y\n"
             "    LSR\n"
             "    DEY\n"
             "    BPL .genloop1\n"
             "    LDY #5\n"
             "    LDA #3\n"
             ".genloop2:\n"
             "    STA ZPROGBSLSH,Y\n"
             "    ASL\n"
             "    DEY\n"
             "    BPL .genloop2\n"

             ".alreadysetup:\n"
             "    LDA $0304\n"
             "    EOR ZPROG10PCT\n"
             "    AND #2\n"
             "    BEQ .dofrwrd\n"
             "    LDA #>ZPROGBSLSH\n"
             "    BNE .pickedslash\n"
             ".dofrwrd:\n"
             "    LDA #>ZPROGFSLSH\n"
             ".pickedslash:\n"
             "    STA .10p_smc\n"
             "    LDA ZPROG10PLO\n"
             "    CLC\n"
             "    ADC ZPROG10PCT\n"
             "    STA ZPTMP\n"
             "    LDA ZPROG10PHI\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    LDY #0\n"
             "    LDX #0\n"
             ".10printloop:\n"
             "    LDA ZPROGFSLSH,X\n"
             "    STA (ZPTMP),Y\n"
             "    LDA ZPTMP\n"
             "    ADC #38\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    INX\n"
             "    CPX #6\n"
             "    BNE .10printloop\n"
             "    LDY ZPROG10PCT\n"
             "    INY\n"
             "    CPY #38\n"
             "    BEQ .newrow\n"
             "    STY ZPROG10PCT\n"
             "    RTS\n"
             ".newrow:\n"
             "    LDA #0\n"
             "    STA ZPROG10PCT\n"
             "    LDA ZPROG10PLO\n"
             "    CLC\n"
             "    ADC #228\n"
             "    STA ZPROG10PLO\n"
             "    LDA ZPROG10PHI\n"
             "    ADC #0\n"
             "    STA ZPROG10PHI\n", tapdata, addr);
    smcaddr = sym_get("rotatogreet_texture") + 228*6;
    snprintf(tmp, sizeof(tmp),
        "    LDA ZPROG10PLO\n"
        "    EOR ZPROG10PHI\n"
        "    CMP #%u\n"
        "    BNE .keepgoing\n"
        "    LDA #1\n"
        "    STA ZPROGMLEND\n"
        ".keepgoing:\n"
        "    RTS\n", (smcaddr >> 8) ^ (smcaddr&0xff));
    assemble(tmp, tapdata, addr);

    assemble("rotatogreet_rollaway:\n"
             "    LDX #46\n"
             "    LDY #0\n"
             ".rollaway:\n"
             "    CPX #3\n"
             "    BCS .doneclr\n"
             "    LDA rog_linetab_lo,X\n"
             "    STA ZPTMP\n"
             "    LDA rog_linetab_hi,X\n"
             "    STA ZPTMP2\n"
             "    LDA #0\n"
             "    STA (ZPTMP),y\n"
             ".doneclr:\n"
             "    CLC\n"
             "    LDA rog_linetab_lo,X\n"
             "    ADC #$78\n"
             "    STA rog_linetab_lo,X\n"
             "    LDA rog_linetab_hi,X\n"
             "    ADC #0\n"
             "    STA rog_linetab_hi,X\n"
             "    DEX\n"
             "    BPL .rollaway\n"
             "    RTS\n", tapdata, addr);

    smcaddr = sym_get(".rogscroll_smc1");
    sym_define(".rogscroll_smc2", smcaddr+1);
    smcaddr = sym_get(".10printloop");
    sym_define(".10p_smc", smcaddr+1);

    assemble("rotatogreet_writebyte:\n"
             "    ORA #$40\n"
             "rotatogreet_writesetbyte:\n"
             "    STA $FFFF,X\n"
             "    INX\n"
             "    RTS\n"
             "rotatogreet_moveon:\n"
             "    LDX #0\n"
             "    LDA rog_smc1_lo\n"
             "    CLC\n"
             "    ADC #40\n"
             "    STA rog_smc1_lo\n"
             "    TXA\n"
             "    ADC rog_smc1_hi\n"
             "    STA rog_smc1_hi\n"
             "    CMP #$BF\n"
             "    BNE .noclip\n"
             "    LDA rog_smc1_lo\n"
             "    CMP #$40\n"
             "    BCC .noclip\n"
             "    LDA #$E0\n"
             "    STA rog_smc1_lo\n"
             "    LDA #$BF\n"
             "    STA rog_smc1_hi\n"
             ".noclip:\n"
             "    RTS\n", tapdata, addr);

    for (i=0; i<NUM_ENTRIES(rotatogreet_maps); i++)
    {
        for (x=0; x<40; x++)
        {
            rdb[i][x].numtxbytes = 0;
            memset(rdb[i][x].bytoffs, 0xff, 6);

            for (bit=5; bit>=0; bit--)
            {
                srcoffs = x*6+(5-bit);
                if (rotatogreet_maps[i]->bit[srcoffs] >= 6)
                    continue;

                if (rdb[i][x].bytoffs[rdb[i][x].numtxbytes] == 0xff)
                {
                    rdb[i][x].bytoffs[rdb[i][x].numtxbytes] = rotatogreet_maps[i]->bytoffs[srcoffs];
                    rdb[i][x].bits[rdb[i][x].numtxbytes] = 1<<rotatogreet_maps[i]->bit[srcoffs];
                    rdb[i][x].shift[rdb[i][x].numtxbytes] = bit - rotatogreet_maps[i]->bit[srcoffs];
                }
                else
                {
                    /* We should only ever need to do INY, otherwise poobums */
                    if ((rotatogreet_maps[i]->bytoffs[srcoffs] != rdb[i][x].bytoffs[rdb[i][x].numtxbytes]) &&
                        (rotatogreet_maps[i]->bytoffs[srcoffs] != (rdb[i][x].bytoffs[rdb[i][x].numtxbytes]+1)))
                    {
                        fprintf(stderr, "Need more than INY :-(\n");
                        exit(EXIT_FAILURE);
                    }

                    if ((rdb[i][x].bytoffs[rdb[i][x].numtxbytes] == rotatogreet_maps[i]->bytoffs[srcoffs]) &&
                        ((bit - rotatogreet_maps[i]->bit[srcoffs]) == rdb[i][x].shift[rdb[i][x].numtxbytes]))
                    {
                        rdb[i][x].bits[rdb[i][x].numtxbytes] |= 1<<rotatogreet_maps[i]->bit[srcoffs];
                    }
                    else
                    {
                        rdb[i][x].numtxbytes++;
                        rdb[i][x].bytoffs[rdb[i][x].numtxbytes] = rotatogreet_maps[i]->bytoffs[srcoffs];
                        rdb[i][x].bits[rdb[i][x].numtxbytes] = 1<<rotatogreet_maps[i]->bit[srcoffs];
                        rdb[i][x].shift[rdb[i][x].numtxbytes] = bit - rotatogreet_maps[i]->bit[srcoffs];
                    }
                }
            }

            if (rdb[i][x].bytoffs[rdb[i][x].numtxbytes] != 0xff)
                rdb[i][x].numtxbytes++;
        }

        for (x=0; x<40; x++)
        {
            if (rdb[i][x].numtxbytes == 0)
                continue;

            rdb[i][x].yinc = 0;
            strcpy(tmp, "rog");
            for (j=0; j<rdb[i][x].numtxbytes; j++)
            {
                if (strlen(tmp) >= MAX_SYMLEN)
                {
                    fprintf(stderr, "Generated symbol got too long!\n");
                    exit(EXIT_FAILURE);
                }

                strcat(tmp, "_");
                if ((j>0) && (rdb[i][x].bytoffs[j] > rdb[i][x].bytoffs[j-1]))
                {
                    rdb[i][x].yinc++;
                    strcat(tmp, "I");
                }

                snprintf(&tmp[strlen(tmp)], 32, "%02x", rdb[i][x].bits[j]);
                if (rdb[i][x].shift[j] > 0)
                    snprintf(&tmp[strlen(tmp)], 32, "ls%x", rdb[i][x].shift[j]);
                else if (rdb[i][x].shift[j] < 0)
                    snprintf(&tmp[strlen(tmp)], 32, "rs%x", -rdb[i][x].shift[j]);
            }

            if (strlen(tmp) >= MAX_SYMLEN)
            {
                fprintf(stderr, "Generated symbol got too long!\n");
                exit(EXIT_FAILURE);
            }

            strcpy(rdb[i][x].sym, tmp);

            if (sym_set(rdb[i][x].sym))
                continue;

            snprintf(tmp, sizeof(tmp), "%s:\n", rdb[i][x].sym);
            assemble(tmp, tapdata, addr);

            for (j=0; j<rdb[i][x].numtxbytes; j++)
            {
                if ((j>0) && (rdb[i][x].bytoffs[j] > rdb[i][x].bytoffs[j-1]))
                    assemble("    INY\n", tapdata, addr);

                snprintf(tmp, sizeof(tmp),
                    "    LDA (ZPROGTROWLO),Y\n"
                    "    AND #$%02x\n", rdb[i][x].bits[j]);
                assemble(tmp, tapdata, addr);

                if (((rdb[i][x].bits[j] == 0x20) ||
                     (rdb[i][x].bits[j] == 0x10) ||
                     (rdb[i][x].bits[j] == 0x08) ||
                     (rdb[i][x].bits[j] == 0x04) ||
                     (rdb[i][x].bits[j] == 0x02) ||
                     (rdb[i][x].bits[j] == 0x01)) &&
                    (rdb[i][x].shift[j] != 0))
                {
                    if (rdb[i][x].shift[j] > 0)
                    {
                        snprintf(tmp, sizeof(tmp),
                            "    BEQ .skipit\n"
                            "    LDA #$%02x\n",
                            (rdb[i][x].bits[j]<<rdb[i][x].shift[j]) & 0x3f);
                    }
                    else
                    {
                        snprintf(tmp, sizeof(tmp),
                            "    BEQ .skipit\n"
                            "    LDA #$%02x\n",
                            (rdb[i][x].bits[j]>>-rdb[i][x].shift[j]) & 0x3f);
                    }
                    assemble(tmp, tapdata, addr);

                }
                else
                {
                    if (abs(rdb[i][x].shift[j]) > 2)
                        assemble("    BEQ .skipit\n", tapdata, addr);

                    if (rdb[i][x].shift[j] > 0)
                    {
                        for (sh=0; sh<rdb[i][x].shift[j]; sh++)
                            assemble("    ASL\n", tapdata, addr);
                    } else {
                        for (sh=0; sh<-rdb[i][x].shift[j]; sh++)
                            assemble("    LSR\n", tapdata, addr);
                    }
                }

                assemble(".skipit:\n", tapdata, addr);
                if (j!=0)
                    assemble("    ORA ZPROGACCUM\n", tapdata, addr);
                if (j!=(rdb[i][x].numtxbytes-1))
                    assemble("    STA ZPROGACCUM\n", tapdata, addr);

                resolve_and_remove_temporary_syms(tapdata);
            }

            assemble("    JMP rotatogreet_writebyte\n", tapdata, addr);
        }
    }

    for (i=0; i<NUM_ENTRIES(rotatogreet_maps); i++)
    {
        snprintf(tmp, sizeof(tmp), "rotatogreet_map%u", i);
        sym_define(tmp, *addr);

        assemble("    LDX #0\n", tapdata, addr);

        ais40 = false;
        curry = -99;
        for (x=0; x<40; x++)
        {
            if (rdb[i][x].numtxbytes == 0)
            {
                if (x == 0)
                {
                    snprintf(tmp, sizeof(tmp), "    LDA #$%02x\n", rotatogreet_maps[i]->colour);
                    assemble(tmp, tapdata, addr);
                    ais40 = false;
                }
                else if (!ais40)
                {
                    assemble("    LDA #$40\n", tapdata, addr);
                    ais40 = true;
                }

                assemble("    JSR rotatogreet_writesetbyte\n", tapdata, addr);
                continue;
            }

            if ((curry+1) == rdb[i][x].bytoffs[0])
            {
                assemble("    INY\n", tapdata, addr);
                curry++;
            }
            else if (curry != rdb[i][x].bytoffs[0])
            {
                snprintf(tmp, sizeof(tmp), "    LDY #$%02x\n", rdb[i][x].bytoffs[0]);
                assemble(tmp, tapdata, addr);
                curry = rdb[i][x].bytoffs[0];
            }
            snprintf(tmp, sizeof(tmp), "    JSR %s\n", rdb[i][x].sym);
            assemble(tmp, tapdata, addr);
            ais40 = false;
            curry += rdb[i][x].yinc;
        }

        assemble("    JMP rotatogreet_moveon\n", tapdata, addr);
    }

    smcaddr = sym_get("rotatogreet_writesetbyte");
    sym_define("rog_smc1_lo", smcaddr+1);
    sym_define("rog_smc1_hi", smcaddr+2);

    for (j=0; j<8; j++)
    {
        snprintf(tmp, sizeof(tmp),
            "rotatogreet_drawframe%u:\n"
            "    LDA $%04x\n"
            "    STA rog_smc1_lo\n"
            "    LDA $%04x\n"
            "    STA rog_smc1_hi\n",
            j,
            sym_get("rog_linetab_lo"),
            sym_get("rog_linetab_hi"));
        assemble(tmp, tapdata, addr);

        for (i=0; i<47; i++)
        {
            int txroffs = 0;
            if (rotatogreet_frames[j][i].mapi == -1)
            {
                if ((i==0) || (rotatogreet_frames[j][i-1].mapi != -1))
                {
                    assemble("    LDX #0\n", tapdata, addr);
                }
                snprintf(tmp, sizeof(tmp),
                         "    TXA\n"
                         "    JSR rotatogreet_writesetbyte\n"
                         "    %s rotatogreet_moveon\n", (i==46) ? "JMP" : "JSR");
                assemble(tmp, tapdata, addr);
            }
            else
            {
                txroffs = rotatogreet_frames[j][i].txrow * (228/6);
                snprintf(tmp, sizeof(tmp),
                    "    LDA #>rotatogreet_texture\n"
                    "    CLC\n"
                    "    ADC #$%02x\n"
                    "    STA ZPROGTROWLO\n"
                    "    LDA #<rotatogreet_texture\n"
                    "    ADC #$%02x\n"
                    "    STA ZPROGTROWHI\n"
                    "    %s rotatogreet_map%u\n", txroffs&0xff, (txroffs>>8)&0xff, (i==46) ? "JMP" : "JSR", rotatogreet_frames[j][i].mapi);
                assemble(tmp, tapdata, addr);
            }
        }
    }

    sym_define("rotatogreet_lineinframe", *addr);
    for (i=0; i<47; i++)
    {
        int txroffs = 0;

        snprintf(tmp, sizeof(tmp),
            "    LDA $%04x\n"
            "    STA rog_smc1_lo\n"
            "    LDA $%04x\n"
            "    STA rog_smc1_hi\n"
            "    LDA $%02x\n"
            "    STA ZPTMP\n"
            "    LDA $%02x\n"
            "    STA ZPTMP2\n"
            "    LDA #0\n"
            "    TAY\n"
            "    STA (ZPTMP),Y\n",
            sym_get("rog_linetab_lo") + i,
            sym_get("rog_linetab_hi") + i,
            sym_get("ZPROGLFRML") + i,
            sym_get("ZPROGLFRMH") + i);
        assemble(tmp, tapdata, addr);


        if (rotatogreet_frames[0][i].mapi == -1)
        {
            if ((i==0) || (rotatogreet_frames[0][i-1].mapi != -1))
            {
                assemble("    LDX #0\n", tapdata, addr);
            }
            snprintf(tmp, sizeof(tmp),
                     "    TXA\n"
                     "    JSR rotatogreet_writesetbyte\n"
                     "    %s rotatogreet_moveon\n", (i==46) ? "JMP" : "JSR");
            assemble(tmp, tapdata, addr);
        }
        else
        {
            txroffs = rotatogreet_frames[0][i].txrow * (228/6);
            snprintf(tmp, sizeof(tmp),
                "    LDA #>rotatogreet_texture\n"
                "    CLC\n"
                "    ADC #$%02x\n"
                "    STA ZPROGTROWLO\n"
                "    LDA #<rotatogreet_texture\n"
                "    ADC #$%02x\n"
                "    STA ZPROGTROWHI\n"
                "    %s rotatogreet_map%u\n", txroffs&0xff, (txroffs>>8)&0xff, (i==46) ? "JMP" : "JSR", rotatogreet_frames[0][i].mapi);
            assemble(tmp, tapdata, addr);
        }
    }

    sym_define("rotatogreet_frouts", *addr);
    for (i=0; i<8; i++)
    {
        snprintf(tmp, sizeof(tmp), "rotatogreet_drawframe%u", i);
        smcaddr = sym_get(tmp);
        tapdata[(*addr)++] = smcaddr & 0xff;
        tapdata[(*addr)++] = smcaddr >> 8;
    }
}

int main(int argc, const char *argv[])
{
    int i;
    uint16_t addr = TAP_START;

    srand(time(NULL));
    sym_define("ZPWIPETAB",    0); /* 28 */
    sym_define("ZPWIPEPOS",   28); /* 28 */
    sym_define("ZPTMP",       28*2);
    sym_define("ZPTMP2",      28*2+1);
    sym_define("ZPTMP3",      28*2+2);
    sym_define("ZPTMP4",      28*2+3);
    sym_define("ZPTMP5",      28*2+4);
    sym_define("ZPTMP6",      28*2+5);
    sym_define("ZPROGLFRML",  28*2+6); /* 47 */
    sym_define("ZPROGLFRMH",  28*2+6 + 47); /* 47 */
    sym_define("ZPROGFSLSH",  28*2+6); /* re-use the above for the forward slash graphic */
    sym_define("ZPROGBSLSH",  28*2+12); /* re-use the above for the back slash graphic */
    sym_define("ZPSWFRAME",    3);
    sym_define("ZPSWFRAME2",   4);
    sym_define("ZPSINPOS1",    5);
    sym_define("ZPSINPOS2",    6);
    sym_define("ZPSWTIME",     7);
    sym_define("ZPROGTROWLO",  8);
    sym_define("ZPROGTROWHI",  9);
    sym_define("ZPROGACCUM",  10);
    sym_define("ZPROGSCPOS",  11);
    sym_define("ZPROGSCHLF",  12);
    sym_define("ZPROGSPCCT",  13);
    sym_define("ZPROG10PLO",  14);
    sym_define("ZPROG10PHI",  15);
    sym_define("ZPROG10PCT",  16);
    sym_define("ZPROGMLEND",  17);

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
             "    JSR rotatogreet\n"
             "demoend:\n"
             "    JMP demoend\n", tapdata, &addr);

    gen_scrwipe(&addr);
    gen_stripewobbler(&addr);
    gen_rotatogreet(&addr);

    /* Define a handy table for text screen access */
    sym_define("scrtab", addr);
    resolve_pending(tapdata, false);

    for (i=0; i<28; i++)
    {
        tapdata[addr++] = (0xbb80 + (i*40)) & 0xff;
        tapdata[addr++] = (0xbb80 + (i*40)) >> 8;
    }

    resolve_and_remove_temporary_syms(tapdata);
    resolve_pending(tapdata, true);
    tapdata_used = addr - TAP_START;
    printf("tapdata_used is: %u (%u bytes free)\n", tapdata_used, MAX_TAPADDR-addr);
    write_tapdata();
    printf("SUCCESS!\n");
    return EXIT_SUCCESS;
}
