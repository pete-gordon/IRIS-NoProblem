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
#include "bouncy_tables.h"

uint8_t tapdata[65536];
uint32_t tapdata_used = 0;



void write_tapdata(void)
{
    FILE *f;
    uint8_t tap_header[] = { 0x16, 0x16, 0x16, 0x16, 0x24, 0x00, 0x00, 0x80, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const char *name = "NO PROBLEM";

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
             "    JSR zptmp_set_to_a000\n"
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
    char tmp[1024];
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
             "    JSR zptmp_add40\n"
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
             "    LDA rog_linetab_hi\n"
             "    CMP #$BF\n"
             "    BEQ .tryhi\n"
             "    BCS .finished\n"
             "    BCC .rolloutloop\n"
             ".tryhi:\n"
             "    LDA rog_linetab_lo\n"
             "    CMP #$40\n"
             "    BCC .rolloutloop\n"
             ".finished:\n"
             "    RTS\n", tapdata, addr);

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
        const char *str = "GREETINGS:DEFENCE FORCE:ATE BIT:BITSHIFTERS:DEKADENCE:DARKAGE:LOONIES:EPHIDRENA:RMC CAVE DWELLERS::IO PRINT:";

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
    resolve_and_remove_temporary_syms(tapdata);
}

void gen_noproblem(uint16_t *addr)
{
    FILE *f;
    uint8_t *noprobbmp = NULL;
    size_t noprobbmpsize = 0;
    const char *fontmap = "N0VSYC?PRITEBLOUAM";
    const char *strings[] = { "N0 VSYNC?", "N0 SPRITES?", "N0 SENSIBLE COLOUR", " ATTRIBUTE SYSTEM?", "N0 PROBLEM" };
    uint8_t preshift_font[18*2*10*2]; /* 18 letters, 2 chars per row, 10 rows, *2 for both shifts */
    int pitch, i, j, x, y, s, sx;
    char tmp[1024];
    uint8_t colourwipe[] = { 17, 17, 18, 17, 18, 18, 19, 19, 23, 23, 23, 23, 23, 23, 23, 19, 19, 18, 18, 17, 18, 17, 17, 16, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

    /* Address on screen to write each string */
    uint16_t saddr[] = { 0xa000 + 4 + 1600,
                         0xa000 + 5 + 1600 + 40*40,
                         0xa000 + 6 + 1600 + 40*40*2 - 8*40,
                         0xa000 + 5 + 1600 + 40*40*2 + 8*40,
                         0xa000 + 7 + 1600 + 40*40*3 };

    /* wipe location for each string */
    uint16_t waddr[] = { 0xa000 + 1600 - 280,
                         0xa000 + 1600 + 40*40 - 280,
                         0xa000 + 1600 + 40*40*2 - 8*40,
                         0xa000 + 1600 + 40*40*3 - 280 };

    f = fopen("novsync.bmp", "rb");
    if (NULL == f)
    {
        fprintf(stderr, "Failed to open novsync.bmp\n");
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    noprobbmpsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    noprobbmp = malloc(noprobbmpsize);
    if (NULL == noprobbmp)
    {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        exit(EXIT_FAILURE);
    }

    fread(noprobbmp, noprobbmpsize, 1, f);
    fclose(f);
    f = NULL;

    sym_define("nop_wipecols", *addr);
    memcpy(&tapdata[*addr], colourwipe, sizeof(colourwipe));
    (*addr) += sizeof(colourwipe);

    

    pitch = (((162+7)/8) + 3) & 0xfffffffc;
    for (j=0; j<(162/9); j++)
    {
        for (y=0; y<10; y++)
        {
            preshift_font[j*40+y*2]   = (noprobbmp[(9-y)*pitch+0x3e] >> 2) & 0x3f;
            preshift_font[j*40+y*2+1] = ((noprobbmp[(9-y)*pitch+0x3e] & 3) << 4) | ((noprobbmp[(9-y)*pitch+0x3f] >> 4) & 0x8);

            preshift_font[j*40+20+y*2  ] = preshift_font[j*40+y*2] >> 3;
            preshift_font[j*40+20+y*2+1] = ((preshift_font[j*40+y*2] & 7) << 3) | (preshift_font[j*40+y*2+1] >> 3);

            preshift_font[j*40+y*2]    |= 0x40;
            preshift_font[j*40+y*2+1]  |= 0x40;
            preshift_font[j*40+y*2+20] |= 0x40;
            preshift_font[j*40+y*2+21] |= 0x40;
        }
        for (y=0; y<10; y++)
        {
            for (x=0; x<pitch-1; x++)
                noprobbmp[y*pitch+x+0x3e] = (noprobbmp[y*pitch+x+0x3f] << 1) | (noprobbmp[y*pitch+x+0x40] >> 7);
        }
    }

    for (s=0; s<NUM_ENTRIES(strings); s++)
    {
        for (sx=6, i=0; strings[s][i]; sx+=9, i++)
        {
            for (j=0; fontmap[j]; j++)
            {
                if (fontmap[j] == strings[s][i])
                    break;
            }

            if (!fontmap[j])
                continue;

            snprintf(tmp, sizeof(tmp), "nopf_chr%u_shf%u", j, sx%6);
            if (sym_set(tmp))
                continue;

            sym_define(tmp, *addr);
            memcpy(&tapdata[*addr], &preshift_font[(j*40)+(((sx%6)!=0) ? 20 : 0)], 20);
            *addr += 20;
        }
    }

    assemble("nop_pchr:\n"
             "    LDX #9\n"
             "    LDY #0\n"
             ".loop:\n"
             "    LDA (ZPTMP3),Y\n"
             "    ORA (ZPTMP),Y\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA (ZPTMP3),Y\n"
             "    ORA (ZPTMP),Y\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    JSR nop_pchr_moveon\n"
             "    BPL .loop\n"
             "    LDX #1\n"
             ".loop2:\n"
             "    LDA ZPTMP5\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA ZPTMP6\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    JSR nop_pchr_moveon\n"
             "    BPL .loop2\n", tapdata, addr);

    snprintf(tmp, sizeof(tmp),
             ".finishpchr:\n"
             "    LDA ZPTMP\n"
             ".nop_pchr_smc:\n"
             "    SEC\n"
             "    SBC #%u\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    SBC #%u\n"
             "    STA ZPTMP2\n"
             "    LDA ZPTMP6\n"
             "    STA ZPTMP5\n"
             "    RTS\n", ((38*12)-2) & 0xff, ((38*12)-2) >> 8);
    assemble(tmp, tapdata, addr);

    assemble("nop_pchr_spc:\n"
             "    LDX #9\n"
             "    LDY #0\n"
             ".spcattrloop:\n"
             "    LDA (ZPTMP),Y\n"
             "    CMP #$40\n"
             "    BNE .notthis1\n"
             "    LDA #7\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    BNE .notthis2\n"
             ".notthis1:\n"
             "    INY\n"
             "    LDA (ZPTMP),Y\n"
             "    CMP #$40\n"
             "    BNE .notthis2\n"
             "    LDA #7\n"
             "    STA (ZPTMP),Y\n"
             ".notthis2:\n"
             "    INY\n"
             "    JSR nop_pchr_moveon\n"
             "    BPL .spcattrloop\n"
             "    LDX #1\n"
             "    LDY #20\n"
             ".spcloop:\n"
             "    LDA ZPTMP5\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA ZPTMP6\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    JSR nop_pchr_moveon\n"
             "    BPL .spcloop\n"
             "    BMI .finishpchr\n", tapdata, addr);

    assemble("nop_pchr_moveon:\n"
             "    LDA ZPTMP\n"
             "    CLC\n"
             "    ADC #38\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    DEX\n"
             "    RTS\n", tapdata, addr);

    sym_define("nop_waddrtab", *addr);
    for (i=0; i<NUM_ENTRIES(waddr); i++)
    {
        tapdata[(*addr)++] = waddr[i] & 0xff;
        tapdata[(*addr)++] = waddr[i] >> 8;
    }

    assemble("nop_wipein:\n"
             "    LDA nop_waddrtab,X\n"
             "    STA ZPNPADDRLO\n"
             "    INX\n"
             "    LDA nop_waddrtab,X\n"
             "    STA ZPNPADDRHI\n"
             "    LDX #27\n"
             "    LDA #0\n"
             ".clrwipetab:\n"
             "    STA ZPWIPETAB,X\n"
             "    DEX\n"
             "    BPL .clrwipetab\n"

             "nop_wipein_moveitalong:\n"
             "    LDX #0\n"
             "    LDY #27\n"
             ".moveitloop:\n"
             "    LDA ZPWIPETABP1,X\n"
             "    STA ZPWIPETAB,X\n"
             "    LDA ZPWIPETABM1,Y\n"
             "    STA ZPWIPETAB,Y\n"
             "    INX\n"
             "    DEY\n"
             "    CPY #14\n"
             "    BNE .moveitloop\n"
             "    LDX ZPNPCOUNT\n"
             "    LDA nop_wipecols,X\n"
             "    STA ZPWIPETABM1,Y\n"
             "    STA ZPWIPETAB,Y\n"
             "    INX\n"
             "    STX ZPNPCOUNT\n"
             "    TXA\n"
             "    CMP ZPNPFIN\n"
             "    BNE .notdone\n"
             "    RTS\n"
             ".notdone:\n"
             "    LDA ZPNPADDRLO\n"
             "    STA ZPNPTMP1\n"
             "    LDA ZPNPADDRHI\n"
             "    STA ZPNPTMP2\n"
             "    LDX #0\n"
             "    LDY #0\n"
             ".copywipe:\n"
             "    LDA ZPWIPETAB,X\n"
             "    STA (ZPNPTMP1),Y\n"
             "    LDA #>ZPNPTMP1\n"
             "    JSR zpany_add40\n"
             "    INX\n"
             "    CPX #28\n"
             "    BNE .copywipe\n"
             "    LDY #8\n"
             ".delay2:\n"
             "    LDX #0\n"
             ".delay:\n"
             "    DEX\n"
             "    BNE .delay\n"
             "    DEY\n"
             "    BNE .delay2\n"
             "    JMP nop_wipein_moveitalong\n", tapdata, addr);

    for (s=0; s<NUM_ENTRIES(strings); s++)
    {
        if (s != 3)
        {
            int reals = (s<3) ? s : (s-1);

            snprintf(tmp, sizeof(tmp),
                "nop_printstr%u:\n"
                "    LDA #14\n"
                "    STA ZPNPFIN\n"
                "    LDA #0\n"
                "    STA ZPNPCOUNT\n"
                "    LDX #%u\n"
                "    JSR nop_wipein\n", reals, reals*2);
            assemble(tmp, tapdata, addr);
        }
        assemble("    LDA #$40\n"
                 "    STA ZPTMP5\n"
                 "    STA ZPTMP6\n", tapdata, addr);
        snprintf(tmp, sizeof(tmp), 
            "    LDA #$%x\n"
            "    STA ZPTMP\n"
            "    LDA #$%x\n"
            "    STA ZPTMP2\n", saddr[s]&0xff, saddr[s]>>8);
        assemble(tmp, tapdata, addr);
        for (sx=6, i=0; strings[s][i]; sx+=9, i++)
        {
            for (j=0; fontmap[j]; j++)
            {
                if (fontmap[j] == strings[s][i])
                    break;
            }

            if (!fontmap[j])
            {
                /* Move on 9 px */
                snprintf(tmp, sizeof(tmp),
                    "    LDA #%u\n"
                    "    STA .nop_pchr_smc\n"
                    "    JSR nop_pchr_spc\n", ((sx%6)==0) ? 0x18 : 0x38);
                assemble(tmp, tapdata, addr);
                continue;
            }

            if (j==1)
            {
                assemble("    LDA #$47\n"
                         "    STA ZPTMP5\n"
                         "    LDA #$7f\n"
                         "    STA ZPTMP6\n", tapdata, addr);
            }

            if ((strings[s][i+1] == 0) && ((sx%6)==0))
            {
                assemble("    LDA #$78\n"
                         "    STA ZPTMP6\n", tapdata, addr);
            }

            snprintf(tmp, sizeof(tmp),
                "    LDA #>nopf_chr%u_shf%u\n"
                "    STA ZPTMP3\n"
                "    LDA #<nopf_chr%u_shf%u\n"
                "    STA ZPTMP4\n"
                "    LDA #%u\n"
                "    STA .nop_pchr_smc\n"
                "    JSR nop_pchr\n", j, sx%6, j, sx%6, ((sx%6)==0) ? 0x18 : 0x38);
            assemble(tmp, tapdata, addr);
        }

        if (s != 2)
        {
            snprintf(tmp, sizeof(tmp),
                "    LDA #%u\n"
                "    STA ZPNPFIN\n"
                "    JSR nop_wipein_moveitalong\n"
                "    JSR nop_waitabit\n"
                "    JMP nop_waitabit\n", sizeof(colourwipe));
            assemble(tmp, tapdata, addr);
        }
    }

    resolve_and_remove_temporary_syms(tapdata);

    assemble("nop_waitabit:\n"
             "    LDY #0\n"
             ".delay1:\n"
             "    LDX #0\n"
             ".delay2:\n"
             "    NOP\n"
             "    NOP\n"
             "    NOP\n"
             "    DEX\n"
             "    BNE .delay2\n"
             "    DEY\n"
             "    BNE .delay1\n"
             "    RTS\n", tapdata, addr);

    assemble("noproblem:\n"
             "    JSR zptmp_set_to_a000\n"
             "    LDX #200\n"
             ".loop1:\n"
             "    LDA #$40\n"
             "    LDY #39\n"
             ".loop2:\n"
             "    STA (ZPTMP),Y\n"
             "    DEY\n"
             "    BNE .loop2\n"
             "    LDA #7\n"
             "    STA (ZPTMP),Y\n"
             "    JSR zptmp_add40\n"
             "    DEX\n"
             "    BNE .loop1\n"
             "    JSR nop_waitabit\n"
             "    JSR nop_printstr0\n"
             "    JSR nop_printstr1\n"
             "    JSR nop_printstr2\n"
             "    JSR nop_printstr3\n"
             "    JSR nop_waitabit\n"
             "    JMP nop_waitabit\n", tapdata, addr);
    resolve_and_remove_temporary_syms(tapdata);
}

void gen_bouncy(uint16_t *addr)
{
    int i;
    uint16_t ballbot;
    char tmp[1024];

    sym_define("balltab", *addr);
    memcpy(&tapdata[*addr], balltab, sizeof(balltab));
    (*addr) += sizeof(balltab);

    sym_define("ballrows", *addr);
    for (i=0; i<BALL_H; i++)
        tapdata[(*addr)++] = ballidx[i] * 8;

    sym_define("floorlines", *addr);
    memcpy(&tapdata[*addr], &linestab[0][0], sizeof(linestab));
    (*addr) += sizeof(linestab);

    ballbot = (BALL_H-1) * 40;

    assemble("bouncy_draw_reflection:\n"
             "    LDA #$C8\n"
             "    STA bdraw_smc\n"
             "    LDA ZPBLYPOS\n"
             "    LSR\n"
             "    LSR\n"
             "    STA ZPBLLSTRY\n"
             "    LDA #200\n"
             "    SEC\n"
             "    SBC ZPBLLSTRY\n"
             "    CLC\n"
             "    STA ZPTMP\n"
             "    LDA #0\n"
             "    STA ZPTMP2\n"
             "    LDX #3\n"
             ".loopo1:\n"
             "    ROL ZPTMP\n"
             "    ROL ZPTMP2\n"
             "    DEX\n"
             "    BNE .loopo1\n"
             "    LDA ZPTMP\n"
             "    STA ZPTMP5\n"
             "    LDA ZPTMP2\n"
             "    ADC #$A0\n"
             "    STA ZPTMP6\n"
             "    LDX #2\n"
             ".loopo2:\n"
             "    ROL ZPTMP\n"
             "    ROL ZPTMP2\n"
             "    DEX\n"
             "    BNE .loopo2\n"
             "    LDA ZPTMP\n"
             "    ADC ZPTMP5\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC ZPTMP6\n"
             "    STA ZPTMP2\n", tapdata, addr);

    snprintf(tmp, sizeof(tmp),
             "    LDA ZPBLXPOS\n"
             "    LSR\n"
             "    STA ZPTMP5\n"
             "    LDA ZPTMP\n"
             "    CLC\n"
             "    ADC #%u\n"
             "    STA ZPTMP3\n"
             "    LDA ZPTMP2\n"
             "    ADC #%u\n"
             "    STA ZPTMP4\n"

             "    LDA ZPTMP5\n"
             "    ADC ZPTMP\n"
             "    STA ZPBLLSTRLO\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPBLLSTRHI\n"
             "    JMP bdraw_continue\n", ballbot&0xff, ballbot>>8);
    assemble(tmp, tapdata, addr);

    resolve_and_remove_temporary_syms(tapdata);

    ballbot = (BALL_H*2-1) * 40;

    assemble("bouncy_draw:\n"
             "    LDA #$EA\n"
             "    STA bdraw_smc\n"
             "    LDA ZPBLYPOS\n"
             "    LSR\n"
             "    STA ZPBLLASTY\n"
             "    CLC\n"
             "    STA ZPTMP\n"
             "    LDA #0\n"
             "    STA ZPTMP2\n"
             "    LDX #3\n"
             ".loopo1:\n"
             "    ROL ZPTMP\n"
             "    ROL ZPTMP2\n"
             "    DEX\n"
             "    BNE .loopo1\n"
             "    LDA ZPTMP\n"
             "    STA ZPTMP5\n"
             "    LDA ZPTMP2\n"
             "    ADC #$A0\n"
             "    STA ZPTMP6\n"
             "    LDX #2\n"
             ".loopo2:\n"
             "    ROL ZPTMP\n"
             "    ROL ZPTMP2\n"
             "    DEX\n"
             "    BNE .loopo2\n"
             "    LDA ZPTMP\n"
             "    ADC ZPTMP5\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC ZPTMP6\n"
             "    STA ZPTMP2\n", tapdata, addr);

    snprintf(tmp, sizeof(tmp),
             "    LDA ZPBLXPOS\n"
             "    LSR\n"
             "    STA ZPTMP5\n"
             "    LDA ZPTMP\n"
             "    CLC\n"
             "    ADC #%u\n"
             "    STA ZPTMP3\n"
             "    LDA ZPTMP2\n"
             "    ADC #%u\n"
             "    STA ZPTMP4\n", ballbot&0xff, ballbot>>8);
    assemble(tmp, tapdata, addr);
    assemble("    LDA ZPTMP5\n"
             "    ADC ZPTMP\n"
             "    STA ZPBLLASTLO\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPBLLASTHI\n"

             "bdraw_continue:\n"
             "    LDA ZPBLXPOS\n"
             "    AND #1\n"
             "    ASL\n"
             "    ASL\n" // C = 0
             "    STA ZPBLTABOFF\n"

             "    LDY #0\n"
             "bouncy_drawloop:\n"
             "    STY ZPBLTABPOS\n"
             "    LDA #>ZPTMP\n"
             "    JSR cmp_screenend\n"
             "    BCS .clipit\n"
             "    LDA #>ZPTMP3\n"
             "    JSR cmp_screenend\n"
             "    BCC .noclip_bh\n"
             "    LDA #$60\n"
             "    BNE .clipcrapdone\n"
             ".noclip_bh:\n"
             "    LDA #$91\n"
             ".clipcrapdone:\n"
             "    STA bdraw_plotit_smc\n"
             "    LDA ballrows,Y\n"
             "    CLC\n"
             "    ADC ZPBLTABOFF\n"
             "    TAX\n"

             "    LDY ZPTMP5\n"

             "    LDA balltab,X\n"
             "    STA ZPBLTMP\n"
             "    BEQ .noaddoaddo\n"
             "    STX ZPBLTMP2\n"
             "    TAX\n"
             ".loopbonk:\n"
             "    LDA #$40\n"
             "    JSR bdraw_plotit\n"
             "    INY\n"
             "    DEX\n"
             "    BNE .loopbonk\n"
             "    LDX ZPBLTMP2\n"
             ".noaddoaddo:\n"
             "    INX\n"

             "    LDA #$40\n"
             "    JSR bdraw_plotit\n"
             "    INY\n"
             "    LDA balltab,X\n"
             "    JSR bdraw_plotit\n"
             "    INY\n"
             "    INX\n"
             "    STX ZPBLROWDAT\n"
             "    LDA balltab,X\n"
             "    BEQ .skipmid\n"
             "    TAX\n"
             "    LDA #$7f\n"
             "bouncy_drawrow:\n"
             "    JSR bdraw_plotit\n"
             "    INY\n"
             "    DEX\n"
             "    BNE bouncy_drawrow\n"
             ".skipmid:\n"
             "    LDX ZPBLROWDAT\n"
             "    INX\n"
             "    LDA balltab,X\n"
             "    JSR bdraw_plotit\n"
             "    INY\n"
             "    LDX ZPBLTMP\n"
             ".clroclro:\n"
             "    LDA #$40\n"
             "    JSR bdraw_plotit\n"
             "    INY\n"
             "    DEX\n"
             "    BPL .clroclro\n"
             ".clipit:\n"
             "    LDA #>ZPTMP3\n"
             "    JSR zpany_sub40\n"
             "    JSR zptmp_add40\n"
             "    LDY ZPBLTABPOS\n"
             "bdraw_smc:\n"
             "    NOP\n"
             "    INY\n", tapdata, addr);
    snprintf(tmp, sizeof(tmp),
             "    CPY #%u\n"
             "    BNE bouncy_drawloop\n"
             "    RTS\n", BALL_H);
             
    assemble(tmp, tapdata, addr);
    assemble("bdraw_plotit:\n"
             "    STA (ZPTMP),Y\n"
             "bdraw_plotit_smc:\n"
             "    STA (ZPTMP3),Y\n"
             "    RTS\n", tapdata, addr);

    assemble("bouncy:\n"
             "    LDY #>ZPBLTMP2\n"
             "    LDA #0\n"
             ".clrzp:\n"
             "    STA $00,Y\n"
             "    DEY\n"
             "    BNE .clrzp\n"
             "    LDA #>floorlines\n"
             "    STA ZPBLFLLLO\n"
             "    LDA #<floorlines\n"
             "    STA ZPBLFLLHI\n"
             "    LDA #0\n"
             "    STA ZPBLXPOS\n"
             "    STA ZPBLYPOS\n"
             "    JSR zptmp_set_to_a000\n"
             "    LDX #120\n"
             "    LDA #7\n"
             "    STA ZPTMP3\n"
             "    STA ZPTMP5\n"
             "    LDA #16\n"
             "    STA ZPTMP4\n"
             "    JSR bouncy_clr_part_nolines\n"
             "    LDX #40\n"
             "    LDA #20\n"
             "    STA ZPTMP4\n"
             "    LDA #0\n"
             "    STA ZPBLFLTABO\n"
             "    STA ZPBLFLLINE\n"
             "    JSR bouncy_clr_part\n"
             "    LDX #40\n"
             "    LDA #6\n"
             "    STA ZPTMP3\n"
             "    LDA #5\n"
             "    STA ZPTMP5\n"
             "    JSR bouncy_clr_part\n"
             "    LDA #$78\n"
             "    STA ZPTMP\n"
             "    LDA #$A0\n"
             "    STA ZPTMP2\n"

             "    LDA #6\n"
             "    STA ZPBLXPOS\n"
             "    LDA #10\n"
             "    STA ZPBLYPOS\n"
             "bouncy_loop:\n"
             "    JSR bouncy_draw\n"
             "    JSR bouncy_draw_reflection\n"
             "    JSR bouncy_movelines\n", tapdata, addr);

    snprintf(tmp, sizeof(tmp),
             "    LDA ZPBLYPOS\n"
             "    CLC\n"
             "    ADC ZPBLDY\n"
             "    STA ZPBLYPOS\n"
             "    CMP #%u\n"
             "    BCC .nobounce\n"
             "    LDA #%u\n"
             "    STA ZPBLYPOS\n"
             "    LDA #$EE\n"
             "    STA ZPBLDY\n"
             ".nobounce:\n"
             "    LDY ZPBLDY\n"
             "    CPY #10\n"
             "    BEQ .tvel\n"
             "    INY\n"
             "    STY ZPBLDY\n"
             ".tvel:\n", (160-BALL_H*2)*2, (160-BALL_H*2)*2);
    assemble(tmp, tapdata, addr);

    assemble("    LDA ZPBLYPOS\n"
             "    LDY ZPBLXPOS\n"
             "    LDA ZPBLDX\n"
             "    BNE .goleft\n"
             "    INY\n"
             "    INY\n"
             ".goleft:\n"
             "    DEY\n"
             "    STY ZPBLXPOS\n"
             "    CPY #4\n"
             "    BEQ .pongit\n"
             "    CPY #55\n"
             "    BNE .dontpongit\n"
             ".pongit:\n"
             "    LDA ZPBLDX\n"
             "    EOR #1\n"
             "    STA ZPBLDX\n"
             ".dontpongit:\n"
             /*"    LDX #20\n"
             "bouncy_delay:\n"
             "    DEX\n"
             "    BNE bouncy_delay\n"*/, tapdata, addr);

    snprintf(tmp, sizeof(tmp),
             "    LDA ZPBLYPOS\n"
             "    LSR\n"
             "    STA ZPTMP3\n"
             "    CMP ZPBLLASTY\n"
             "    BEQ bouncy_loop\n"
             "    BCS .clrtop\n"

             "    LDA ZPBLLASTLO\n"
             "    CLC\n"
             "    ADC #%u\n"
             "    STA ZPTMP\n"
             "    LDA ZPBLLASTHI\n"
             "    ADC #%u\n"
             "    STA ZPTMP2\n", (BALL_H*80)&0xff, (BALL_H*80)>>8);
    assemble(tmp, tapdata, addr);
    assemble("    LDA ZPBLLASTY\n"
             "    SEC\n"
             "    SBC ZPTMP3\n"
             "    TAX\n"
             "    LSR\n"
             "    STA ZPBLTMP\n"
             ".clearrows_bot:\n"
             "    LDA #>ZPTMP\n"
             "    JSR zpany_sub40\n", tapdata, addr);
    snprintf(tmp, sizeof(tmp),
             "    LDY #%u\n", (((BALL_W*2)+5)/6)+1);
    assemble(tmp, tapdata, addr);
    assemble(".clearrow_bot:\n"
             "    LDA #$40\n"
             "    STA (ZPTMP),Y\n"
             "    CPX ZPBLTMP\n"
             "    BCC .nope2\n"
             "    LDA #>ZPBLLSTRLO\n"
             "    JSR cmp_screenend\n"
             "    BCS .nope2\n"
             "    LDA #$40\n"
             "    STA (ZPBLLSTRLO),Y\n"
             ".nope2:\n"
             "    DEY\n"
             "    BPL .clearrow_bot\n"
             "    LDA #>ZPBLLSTRLO\n"
             "    JSR zpany_add40\n"
             "    DEX\n"
             "    BNE .clearrows_bot\n"
             "    JMP bouncy_loop\n"
             ".clrtop:\n"
             "    LDA ZPBLLASTLO\n"
             "    STA ZPTMP\n"
             "    LDA ZPBLLASTHI\n"
             "    STA ZPTMP2\n", tapdata, addr);
    snprintf(tmp, sizeof(tmp),
             "    LDA ZPBLLSTRLO\n"
             "    ADC #%u\n"
             "    STA ZPBLLSTRLO\n"
             "    LDA ZPBLLSTRHI\n"
             "    ADC #%u\n"
             "    STA ZPBLLSTRHI\n", (BALL_H*40)&0xff, (BALL_H*40)>>8);
    assemble(tmp, tapdata, addr);
    assemble("    LDA ZPTMP3\n"
             "    SEC\n"
             "    SBC ZPBLLASTY\n"
             "    TAX\n"
             "    LSR\n"
             "    STA ZPBLTMP\n", tapdata, addr);

    snprintf(tmp, sizeof(tmp),
             ".clearrows:\n"
             "    LDA #>ZPBLLSTRLO\n"
             "    JSR zpany_sub40\n"
             "    LDY #%u\n"
             ".clearrow:\n"
             "    LDA #$40\n"
             "    STA (ZPTMP),Y\n"
             "    CPX ZPBLTMP\n"
             "    BCC .nope4\n"
             "    LDA #>ZPBLLSTRLO\n"
             "    JSR cmp_screenend\n"
             "    BCS .nope4\n"
             "    LDA #$40\n"
             "    STA (ZPBLLSTRLO),Y\n"
             ".nope4:\n"
             "    DEY\n"
             "    BPL .clearrow\n"
             "    JSR zptmp_add40\n"
             "    DEX\n"
             "    BNE .clearrows\n", (((BALL_W*2)+5)/6)+1);
    assemble(tmp, tapdata, addr);
    assemble("    JMP bouncy_loop\n", tapdata, addr);

    assemble("bouncy_movelines:\n"
             "    LDA ZPBLFLFRM\n"
             "    CLC\n"
             "    ADC #12\n"
             "    CMP #192\n"
             "    BNE .okeydoke\n"
             "    LDA #0\n"
             ".okeydoke:\n"
             "    STA ZPBLFLFRM\n"
             "    ADC #>floorlines\n"
             "    STA ZPBLFLLLO\n"
             "    LDA #<floorlines\n"
             "    ADC #0\n"
             "    STA ZPBLFLLHI\n"
             "    LDA #0\n"
             "    STA ZPBLFLTABO\n"
             "    STA ZPBLFLLINE\n"
             "    LDA #$C0\n"
             "    STA ZPTMP\n"
             "    LDA #$B2\n"
             "    STA ZPTMP2\n"
             "    LDA #7\n"
             "    STA ZPTMP5\n"
             "    STA ZPTMP6\n"
             "    LDX #40\n"
             "    JSR bouncy_mvl_part\n"
             "    LDA #5\n"
             "    STA ZPTMP5\n"
             "    LDA #6\n"
             "    STA ZPTMP6\n"
             "    LDX #40\n"
             "bouncy_mvl_part:\n"
             "    LDY ZPBLFLTABO\n"
             "    LDA (ZPBLFLLLO),Y\n"
             "    CMP ZPBLFLLINE\n"
             "    BNE .notspecialmovo\n"
             "    INY\n"
             "    STY ZPBLFLTABO\n"
             "    LDY #0\n"
             "    LDA ZPTMP5\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA #17\n"
             "    STA (ZPTMP),Y\n"
             "    BNE .donespecialmovo\n"
             ".notspecialmovo:\n"
             "    LDY #0\n"
             "    LDA ZPTMP6\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA #20\n"
             "    STA (ZPTMP),Y\n"
             ".donespecialmovo:\n"
             "    LDY ZPBLFLLINE\n"
             "    INY\n"
             "    STY ZPBLFLLINE\n"
             "    JSR zptmp_add40\n"
             "    DEX\n"
             "    BNE bouncy_mvl_part\n"
             "    RTS\n", tapdata, addr);

    assemble("bouncy_clr_part:\n"
             "    LDY ZPBLFLTABO\n"
             "    LDA (ZPBLFLLLO),Y\n"
             "    CMP ZPBLFLLINE\n"
             "    BNE bouncy_clr_part_nolines\n"
             "    INY\n"
             "    STY ZPBLFLTABO\n"
             "    LDY #0\n"
             "    LDA ZPTMP5\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA #17\n"
             "    STA (ZPTMP),Y\n"
             "    BNE .donespecial\n"
             "bouncy_clr_part_nolines:\n"
             "    LDY #0\n"
             "    LDA ZPTMP3\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    LDA ZPTMP4\n"
             "    STA (ZPTMP),Y\n"
             ".donespecial:\n"
             "    LDY ZPBLFLLINE\n"
             "    INY\n"
             "    STY ZPBLFLLINE\n"
             "    LDY #2\n"
             "    LDA #$40\n"
             ".bouncy_clr2:\n"
             "    STA (ZPTMP),Y\n"
             "    INY\n"
             "    CPY #40\n"
             "    BNE .bouncy_clr2\n"
             "    JSR zptmp_add40\n"
             "    DEX\n"
             "    BNE bouncy_clr_part\n"
             "    RTS\n", tapdata, addr);


    resolve_and_remove_temporary_syms(tapdata);
}


int main(int argc, const char *argv[])
{
    int i;
    uint16_t addr = TAP_START;

    srand(time(NULL));
    /* zero page defines */
    sym_define("ZPWIPEPOS",    0); /* 28 */
    sym_define("ZPWIPETABM1" ,27);
    sym_define("ZPWIPETAB",   28); /* 28 */
    sym_define("ZPWIPETABP1" ,29);
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

    sym_define("ZPBLTABPOS",   3);
    sym_define("ZPBLROWDAT",   4);
    sym_define("ZPBLXPOS",     5);
    sym_define("ZPBLYPOS",     6);
    sym_define("ZPBLTABOFF",   7);
    sym_define("ZPBLDX",       8);
    sym_define("ZPBLDY",       9);
    sym_define("ZPBLLASTLO",  10);
    sym_define("ZPBLLASTHI",  11);
    sym_define("ZPBLLASTY",   12);
    sym_define("ZPBLLSTRLO",  13);
    sym_define("ZPBLLSTRHI",  14);
    sym_define("ZPBLLSTRY",   15);
    sym_define("ZPBLFLTABO",  16);
    sym_define("ZPBLFLLINE",  17);
    sym_define("ZPBLFLLLO",   18);
    sym_define("ZPBLFLLHI",   19);
    sym_define("ZPBLFLFRM",   20);
    sym_define("ZPBLTMP",     21);
    sym_define("ZPBLTMP2",    22);

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
    sym_define("ZPNPADDRLO",  18);
    sym_define("ZPNPADDRHI",  19);
    sym_define("ZPNPCOUNT",   20);
    sym_define("ZPNPFIN",     21);
    sym_define("ZPNPTMP1",    22);
    sym_define("ZPNPTMP2",    23);
    
    sym_define("ZPSTORY",    24);

    /* Demo entry point */
    assemble("demostart:\n"
             "    SEI\n"
             "    LDA #19\n"  /* yellow paper */
             "    STA ZPTMP5\n"
             "    JSR scrwipe\n"
             "    LDA #16\n"  /* black paper */
             "    STA ZPTMP5\n"
             "    JSR scrwipe\n"
             "    LDA #30\n" /* switch to hires */
             "    STA $BB80\n"
             "    JSR noproblem\n"
             "    JSR stripewobbler\n"
             "    JSR rotatogreet\n"
             "    JSR bouncy\n"
             "demoend:\n"
             "    JMP demoend\n"
             "zptmp_set_to_a000:\n"
             "    LDA #$00\n"
             "    STA ZPTMP\n"
             "    LDA #$A0\n"
             "    STA ZPTMP2\n"
             "    RTS\n"
             "zptmp_add40:\n"
             "    CLC\n"
             "zptmp_add40_noclc:\n"
             "    LDA ZPTMP\n"
             "    ADC #40\n"
             "    STA ZPTMP\n"
             "    LDA ZPTMP2\n"
             "    ADC #0\n"
             "    STA ZPTMP2\n"
             "    RTS\n"
             "zpany_add40:\n"
             "    STY ZPSTORY\n"
             "    TAY\n"
             "    LDA $00,Y\n"
             "    CLC\n"
             "    ADC #40\n"
             "    STA $00,Y\n"
             "    INY\n"
             "    LDA $00,Y\n"
             "    ADC #0\n"
             "    STA $00,Y\n"
             "    LDY ZPSTORY\n"
             "    RTS\n"
             "zpany_sub40:\n"
             "    STY ZPSTORY\n"
             "    TAY\n"
             "    LDA $00,Y\n"
             "    SEC\n"
             "    SBC #40\n"
             "    STA $00,Y\n"
             "    INY\n"
             "    LDA $00,Y\n"
             "    SBC #0\n"
             "    STA $00,Y\n"
             "    LDY ZPSTORY\n"
             "    RTS\n"
             "cmp_screenend:\n"
             "    STY ZPSTORY\n"
             "    TAY\n"
             "    LDA $01,Y\n"
             "    CMP #$BF\n"
             "    BEQ .try_lo\n"
             "    LDY ZPSTORY\n"
             "    RTS\n"
             ".try_lo:\n"
             "    LDA $00,Y\n"
             "    LDY ZPSTORY\n"
             "    CMP #$40\n"
             "    RTS\n"
             , tapdata, &addr);

    gen_scrwipe(&addr);
    gen_stripewobbler(&addr);
    gen_rotatogreet(&addr);
    gen_noproblem(&addr);
    gen_bouncy(&addr);

    /* Define a handy table for text screen access (currently only used by scrwipe)*/
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
