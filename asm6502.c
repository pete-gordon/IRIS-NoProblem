
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "noproblem.h"
#include "asm6502.h"

#define MAX_SYMBOLS (131072)
#define MAX_REFS (32)

enum
{
    PRT_ABS16,
    PRT_ABS8,
    PRT_REL,
    PRT_HI,
    PRT_LO,
    MAX_PENDREF_TYPES
};

struct symbol
{
    char name[MAX_SYMLEN];
    uint16_t address;
};

struct pendingref
{
    char name[MAX_SYMLEN];
    int num[MAX_PENDREF_TYPES];
    uint16_t ref[MAX_PENDREF_TYPES][MAX_REFS];
};

enum
{
  AM_IMP=0,
  AM_IMM,
  AM_ZP,
  AM_ZPX,
  AM_ZPY,
  AM_ABS,
  AM_ABX,
  AM_ABY,
  AM_ZIX,
  AM_ZIY,
  AM_REL,
  AM_IND
};

#define AMF_IMP (1<<AM_IMP)
#define AMF_IMM (1<<AM_IMM)
#define AMF_ZP  (1<<AM_ZP)
#define AMF_ZPX (1<<AM_ZPX)
#define AMF_ZPY (1<<AM_ZPY)
#define AMF_ABS (1<<AM_ABS)
#define AMF_ABX (1<<AM_ABX)
#define AMF_ABY (1<<AM_ABY)
#define AMF_ZIX (1<<AM_ZIX)
#define AMF_ZIY (1<<AM_ZIY)
#define AMF_REL (1<<AM_REL)
#define AMF_IND (1<<AM_IND)

struct asminf
{
  char *name;
  int16_t imp, imm, zp, zpx, zpy, abs, abx, aby, zix, ziy, rel, ind;
};

static int num_syms = 0;
static struct symbol syms[MAX_SYMBOLS];
static int num_pending = 0;
static struct pendingref pendsyms[MAX_SYMBOLS];

//                                          imp   imm    zp   zpx   zpy   abs   abx   aby   zix   ziy   rel   ind
static struct asminf asmtab[] = { { "BRK", 0x00,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ORA",   -1, 0x09, 0x05, 0x15,   -1, 0x0d, 0x1d, 0x19, 0x01, 0x11,   -1,   -1 },
                                  { "ASL", 0x0a,   -1, 0x06, 0x16,   -1, 0x0e, 0x1e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PHP", 0x08,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BPL",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x10,   -1 },
                                  { "CLC", 0x18,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "JSR",   -1,   -1,   -1,   -1,   -1, 0x20,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "AND",   -1, 0x29, 0x25, 0x35,   -1, 0x2d, 0x3d, 0x39, 0x21, 0x31,   -1,   -1 },
                                  { "BIT",   -1,   -1, 0x24,   -1,   -1, 0x2c,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ROL", 0x2a,   -1, 0x26, 0x36,   -1, 0x2e, 0x3e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PLP", 0x28,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BMI",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x30,   -1 },
                                  { "SEC", 0x38,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "RTI", 0x40,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "EOR",   -1, 0x49, 0x45, 0x55,   -1, 0x4d, 0x5d, 0x59, 0x41, 0x51,   -1,   -1 },
                                  { "LSR", 0x4a,   -1, 0x46, 0x56,   -1, 0x4e, 0x5e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PHA", 0x48,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "JMP",   -1,   -1,   -1,   -1,   -1, 0x4c,   -1,   -1,   -1,   -1,   -1, 0x6c },
                                  { "BVC",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x50,   -1 },
                                  { "CLI", 0x58,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "RTS", 0x60,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ADC",   -1, 0x69, 0x65, 0x75,   -1, 0x6d, 0x7d, 0x79, 0x61, 0x71,   -1,   -1 },
                                  { "ROR", 0x6a,   -1, 0x66, 0x76,   -1, 0x6e, 0x7e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PLA", 0x68,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BVS",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x70,   -1 },
                                  { "SEI", 0x78,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "STY",   -1,   -1, 0x84, 0x94,   -1, 0x8c,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "STA",   -1,   -1, 0x85, 0x95,   -1, 0x8d, 0x9d, 0x99, 0x81, 0x91,   -1,   -1 },
                                  { "STX",   -1,   -1, 0x86, 0x96,   -1, 0x8e,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "DEY", 0x88,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TXA", 0x8a,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BCC",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x90,   -1 },
                                  { "TYA", 0x98,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TXS", 0x9a,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "LDY",   -1, 0xa0, 0xa4, 0xb4,   -1, 0xac, 0xbc,   -1,   -1,   -1,   -1,   -1 },
                                  { "LDA",   -1, 0xa9, 0xa5, 0xb5,   -1, 0xad, 0xbd, 0xb9, 0xa1, 0xb1,   -1,   -1 },
                                  { "LDX",   -1, 0xa2, 0xa6,   -1, 0xb6, 0xae,   -1, 0xbe,   -1,   -1,   -1,   -1 },
                                  { "TAY", 0xa8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TAX", 0xaa,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BCS",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0xb0,   -1 },
                                  { "CLV", 0xb8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TSX", 0xba,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "CPY",   -1, 0xc0, 0xc4,   -1,   -1, 0xcc,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "CMP",   -1, 0xc9, 0xc5, 0xd5,   -1, 0xcd, 0xdd, 0xd9, 0xc1, 0xd1,   -1,   -1 },
                                  { "DEC",   -1,   -1, 0xc6, 0xd6,   -1, 0xce, 0xde,   -1, 0xc1,   -1,   -1,   -1 },
                                  { "INY", 0xc8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "DEX", 0xca,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BNE",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0xd0,   -1 },
                                  { "CLD", 0xd8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "CPX",   -1, 0xe0, 0xe4,   -1,   -1, 0xec,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "SBC",   -1, 0xe9, 0xe5, 0xf5,   -1, 0xed, 0xfd, 0xf9, 0xe1, 0xf1,   -1,   -1 },
                                  { "INC",   -1,   -1, 0xe6, 0xf6,   -1, 0xee, 0xfe,   -1,   -1,   -1,   -1,   -1 },
                                  { "INX", 0xe8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "NOP", 0xea,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BEQ",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0xf0,   -1 },
                                  { "SED", 0xf8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },

                                  // Illegal opcodes
//                                          imp   imm    zp   zpx   zpy   abs   abx   aby   zix   ziy   rel   ind
                                  { "ANC",   -1, 0x0b,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "SAX",   -1,   -1, 0x87,   -1, 0x97, 0x8f,   -1,   -1, 0x83,   -1,   -1,   -1 },
                                  { "ARR",   -1, 0x6b,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ALR",   -1, 0x4b,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "LAX",   -1, 0xab, 0xa7,   -1, 0xb7, 0xaf,   -1, 0xbf, 0xa3, 0xb3,   -1,   -1 },
                                  { "AHX",   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x9f,   -1, 0x93,   -1,   -1 },
                                  { "AXS",   -1, 0xcb,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "DCP",   -1,   -1, 0xc7, 0xd7,   -1, 0xcf, 0xdf, 0xdb, 0xc3, 0xd3,   -1,   -1 },
                                  { "DOP",   -1, 0x80, 0x04, 0x14,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ISC",   -1,   -1, 0xe7, 0xf7,   -1, 0xef, 0xff, 0xfb, 0xe3, 0xf3,   -1,   -1 },
                                  { "JAM", 0x02,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "LAS",   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0xbb,   -1,   -1,   -1,   -1 },
                                  { "RLA",   -1,   -1, 0x27, 0x37,   -1, 0x2f, 0x3f, 0x3b, 0x23, 0x33,   -1,   -1 },
                                  { "RRA",   -1,   -1, 0x67, 0x77,   -1, 0x6f, 0x7f, 0x7b, 0x63, 0x73,   -1,   -1 },
                                  { "SLO",   -1,   -1, 0x07, 0x17,   -1, 0x0f, 0x1f, 0x1b, 0x03, 0x13,   -1,   -1 },
                                  { "SRE",   -1,   -1, 0x47, 0x57,   -1, 0x4f, 0x5f, 0x5b, 0x43, 0x53,   -1,   -1 },
                                  { "SHX",   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x9e,   -1,   -1,   -1,   -1 },
                                  { "SHY",   -1,   -1,   -1,   -1,   -1,   -1, 0x9c,   -1,   -1,   -1,   -1,   -1 },
                                  { "TOP",   -1,   -1,   -1,   -1,   -1, 0x0c, 0x1c,   -1,   -1,   -1,   -1,   -1 },
                                  { "XAA",   -1, 0x8b,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TAS",   -1, 0x9b,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },

                                  { NULL, } };

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

int hexit(char c)
{
  if (isnum(c)) return c-'0';
  if ((c >= 'a') && (c <= 'f')) return c-('a'-10);
  if ((c >= 'A') && (c <= 'F')) return c-('A'-10);
  return -1;
}

void sym_define(char *symname, uint16_t addr)
{
    int i;

    for (i=0; i<num_syms; i++)
    {
        if (strncmp(symname, syms[i].name, MAX_SYMLEN) == 0)
        {
            fprintf(stderr, "Duplicate symbol '%s'\n", symname);
            exit(EXIT_FAILURE);
        }
    }

    if (num_syms >= MAX_SYMBOLS)
    {
        fprintf(stderr, "Too many symbols\n");
        exit(EXIT_FAILURE);
    }

    strncpy(syms[num_syms].name, symname, MAX_SYMLEN);
    syms[num_syms].name[MAX_SYMLEN-1] = 0;
    syms[num_syms++].address = addr;
}

uint16_t sym_get(char *symname)
{
    int i;

    for (i=0; i<num_syms; i++)
    {
        if (strncmp(symname, syms[i].name, MAX_SYMLEN) == 0)
        {
            return syms[i].address;
        }
    }

    fprintf(stderr, "Undefined symbol '%s'\n", symname);
    exit(EXIT_FAILURE);
    return 0;
}

bool sym_set(char *symname)
{
    int i;
    for (i=0; i<num_syms; i++)
    {
        if (strncmp(symname, syms[i].name, MAX_SYMLEN) == 0)
        {
            return true;
        }
    }

    return false;
}

void dump_syms(FILE *f)
{
    int i;

    for (i=0; i<num_syms; i++)
    {
        fprintf(f, "%04x %s\n", syms[i].address, syms[i].name);
    }
}

void resolve_and_remove_temporary_syms(uint8_t *outbuffer)
{
    int p, s, pt, r;
    int rel;

    p = 0;
    while (p < num_pending)
    {
        if (pendsyms[p].name[0] != '.')
        {
            p++;
            continue;
        }

        for (s=0; s<num_syms; s++)
        {
            if (strcmp(syms[s].name, pendsyms[p].name) == 0)
                break;
        }

        if (s == num_syms)
        {
            fprintf(stderr, "Undefined symbol '%s'\n", pendsyms[p].name);
            exit(EXIT_FAILURE);
        }

        for (pt=0; pt<MAX_PENDREF_TYPES; pt++)
        {
            for (r=0; r<pendsyms[p].num[pt]; r++)
            {
                switch (pt)
                {
                    case PRT_ABS16:
                        outbuffer[pendsyms[p].ref[pt][r]] = (syms[s].address & 0xff);
                        outbuffer[pendsyms[p].ref[pt][r]+1] = (syms[s].address >> 8);
                        break;

                    case PRT_ABS8:
                    case PRT_LO:
                        outbuffer[pendsyms[p].ref[pt][r]] = (syms[s].address & 0xff);
                        break;

                    case PRT_HI:
                        outbuffer[pendsyms[p].ref[pt][r]] = (syms[s].address >> 8);
                        break;

                    case PRT_REL:
                        rel = ((int)syms[s].address)-((int)(pendsyms[p].ref[pt][r]+1));
                        if ((rel < -128) || (rel > 127))
                        {
                            fprintf(stderr, "Branch out of range\n");
                            exit(EXIT_FAILURE);
                        }

                        outbuffer[pendsyms[p].ref[pt][r]] = rel & 0xff;
                        break;
                }
            }
        }

        if (p < (num_pending-1))
            memmove(&pendsyms[p], &pendsyms[p+1], sizeof(struct pendingref) * (num_pending - (p+1)));

        num_pending--;
    }

    s = 0;
    while (s < num_syms)
    {
        if (syms[s].name[0] != '.')
        {
            s++;
            continue;
        }

        if (s < (num_syms-1))
            memmove(&syms[s], &syms[s+1], sizeof(struct symbol) * (num_syms - (s+1)));
        num_syms--;
    }
}

void resolve_pending(uint8_t *outbuffer, bool mustresolve)
{
    int p, s, pt, r;
    int rel;

    p = 0;
    while (p < num_pending)
    {
        for (s=0; s<num_syms; s++)
        {
            if (strcmp(syms[s].name, pendsyms[p].name) == 0)
                break;
        }

        if (s == num_syms)
        {
            p++;
            continue;
        }

        for (pt=0; pt<MAX_PENDREF_TYPES; pt++)
        {
            for (r=0; r<pendsyms[p].num[pt]; r++)
            {
                switch (pt)
                {
                    case PRT_ABS16:
                        outbuffer[pendsyms[p].ref[pt][r]] = (syms[s].address & 0xff);
                        outbuffer[pendsyms[p].ref[pt][r]+1] = (syms[s].address >> 8);
                        break;

                    case PRT_ABS8:
                    case PRT_LO:
                        outbuffer[pendsyms[p].ref[pt][r]] = (syms[s].address & 0xff);
                        break;

                    case PRT_HI:
                        outbuffer[pendsyms[p].ref[pt][r]] = (syms[s].address >> 8);
                        break;

                    case PRT_REL:
                        rel = ((int)syms[s].address)-((int)(pendsyms[p].ref[pt][r]+1));
                        if ((rel < -128) || (rel > 127))
                        {
                            fprintf(stderr, "Branch out of range\n");
                            exit(EXIT_FAILURE);
                        }

                        outbuffer[pendsyms[p].ref[pt][r]] = rel & 0xff;
                        break;
                }
            }
        }

        if (p < (num_pending-1))
            memmove(&pendsyms[p], &pendsyms[p+1], sizeof(struct pendingref) * (num_pending - (p+1)));

        num_pending--;
    }

    if ((mustresolve) && (num_pending > 0))
    {
        fprintf(stderr, "Unresolved symbol references:\n");
        for (p=0; p<num_pending; p++)
        {
            fprintf(stderr, "  '%s'\n", pendsyms[p].name);
        }
        exit(EXIT_FAILURE);
    }
}

static bool getnum(uint16_t *num, const char *buf, int *off)
{
    int i, j;
    uint16_t v;

    i = *off;
    while (isspace(buf[i])) i++;

    if (buf[i] == '%')
    {
        // Binary
        i++;
        if (!isbin(buf[i]))
            return false;

        v = 0;
        while (isbin(buf[i]))
        {
            v = (v<<1) | (buf[i]-'0');
            i++;
        }

        *num = v;
        *off = i;
        return true;
    }

    if (buf[i] == '$')
    {
        // Hex
        i++;
        if (!ishex(buf[i]))
            return false;

        v = 0;
        for( ;; )
        {
            j = hexit(buf[i]);
            if( j == -1 ) break;
            v = (v<<4) | j;
            i++;
        }

        *num = v;
        *off = i;
        return true;
    }

    if (!isnum(buf[i]))
        return false;

    v = 0;
    while (isnum(buf[i]))
    {
        v = (v*10) + (buf[i]-'0');
        i++;
    }

    *num = v;
    *off = i;
    return true;
}

bool try_symref(const char *src, int *offs, uint16_t *symaddr, struct pendingref **pref)
{
    int i, len;
    char gotsym[MAX_SYMLEN];

    (*symaddr) = 0;
    (*pref) = NULL;

    while (isspace(src[*offs]))
    {
        if (src[*offs] == '\n')
            return false;
        (*offs)++;
    }

    if (!issymstart(src[*offs]))
        return false;

    i = *offs;
    while (issymchar(src[i]))
        i++;

    len = i-(*offs);
    if (len > (MAX_SYMLEN-1))
        len = (MAX_SYMLEN-1);
    strncpy(gotsym, &src[*offs], len+1);
    gotsym[len] = 0;
    (*offs) = i;

    for (i=0; i<num_syms; i++)
    {
        if (strcmp(gotsym, syms[i].name) == 0)
        {
            (*symaddr) = syms[i].address;
            return true;
        }
    }

    for (i=0; i<num_pending; i++)
    {
        if (strcmp(gotsym, pendsyms[i].name) == 0)
        {
            (*pref) = &pendsyms[i];
            return true;
        }
    }

    if (num_pending >= MAX_SYMBOLS)
    {
        fprintf(stderr, "Too many symbols!\n");
        exit(EXIT_FAILURE);
    }

    memset(&pendsyms[num_pending], 0, sizeof(struct pendingref));
    strcpy(pendsyms[num_pending].name, gotsym);
    (*pref) = &pendsyms[num_pending++];
    return true;
}

/*
- AM_IMP=0,
- AM_IMM,
  AM_ZP,   <-- AM_ABS
  AM_ZPX,  <-- AM_ABX
  AM_ZPY,  <-- AM_ABY
  AM_ABS, nnnn
  AM_ABX, nnnn,x
  AM_ABY, nnnn,y
- AM_ZIX, (nn,x)
- AM_ZIY, (nn),y
  AM_REL,   <-- AM_ABS
- AM_IND  (nnnn)
*/
static bool decode_operand(const char *ptr, int *type, uint16_t *val, struct pendingref **pref, int *preftyp)
{
    int i;
    uint16_t v;
    
    (*pref) = NULL;
    (*preftyp) = MAX_PENDREF_TYPES;
    (*val) = 0;

    i=0;
    while (isspace(ptr[i]))
    {
        if (ptr[i] == '\n')
            break;
        i++;
    }

    // Implied
    if ((ptr[i] == 0) || (ptr[i] == '\n'))
    {
        (*type) = AM_IMP;
        return true;
    }

    // Immediate
    if (ptr[i] == '#')
    {
        char c;

        c = ptr[++i];
        if ((c == '<') || (c == '>'))
        {
            i++;
            if (!try_symref(ptr, &i, &v, pref))
            {
                fprintf(stderr, "Symbol expected\n");
                exit(EXIT_FAILURE);
            }

            if (c == '<')
                v >>= 8;
            else
                v &= 0xff;

            if (*(pref))
            {
                *(preftyp) = ((c == '<') ? PRT_HI : PRT_LO);
            }
        }
        else
        {
            if (!getnum(&v, ptr, &i))
                return false;
        }

        (*type) = AM_IMM;
        (*val)  = v;
        return true;
    }

    // ZIX, ZIY, IND
    if (ptr[i] == '(')
    {
        i++;

        if (!try_symref(ptr, &i, &v, pref))
        {
            if (!getnum(&v, ptr, &i))
                return false;
        }

        if (strncasecmp(&ptr[i], ",X)", 3) == 0)
        {
            (*type) = AM_ZIX;
            (*val)  = v&0xff;

            if (pref)
                *(preftyp) = PRT_ABS8;
            return true;
        }

        if (strncasecmp(&ptr[i], "),Y", 3) == 0)
        {
            (*type) = AM_ZIY;
            (*val)  = v&0xff;

            if (pref)
                *(preftyp) = PRT_ABS8;
            return true;
        }

        if (ptr[i] == ')')
        {
            (*type) = AM_IND;
            (*val)  = v;

            if (pref)
                *(preftyp) = PRT_ABS16;
            return true;
        }

        return false;
    }

    if (!try_symref(ptr, &i, &v, pref))
    {
        if (!getnum(&v, ptr, &i))
            return false;
    }

    if (strncasecmp(&ptr[i], ",X", 2) == 0)
    {
        (*type) = AM_ABX;
        (*val)  = v;

        if (pref)
            *(preftyp) = PRT_ABS16;
        return true;
    }

    if (strncasecmp(&ptr[i], ",Y", 2) == 0)
    {
        (*type) = AM_ABY;
        (*val)  = v;

        if (pref)
            *(preftyp) = PRT_ABS16;
        return true;
    }

    (*type) = AM_ABS;
    (*val)  = v;

    if (pref)
        *(preftyp) = PRT_ABS16;
    return true;
}

char *isolated_line(const char *src)
{
    int len;
    static char line[256];

    len = 0;
    while ((src[len] != 0) && (src[len] != '\n'))
        len++;

    if (len > 255)
        len = 255;

    memcpy(line, src, len);
    line[len] = 0;
    return line;
}

static bool assemble_line(const char *src, int offs, uint8_t *outbuffer, uint16_t *asmaddr)
{
    int j, k, amode;
    unsigned short val;
    char addsym[MAX_SYMLEN];
    uint16_t addsymaddr;
    struct pendingref *pref = NULL;
    int preftyp = MAX_PENDREF_TYPES;
    const char *thisline = &src[offs];
 
    while ((src[offs] == ' ') || (src[offs] == '\t'))
        offs++;

    // Starts with a label?
    j = offs;
    addsym[0] = 0;
    addsymaddr = *asmaddr;
    if (issymstart(src[j]))
    {
        while (issymchar(src[j]))
            j++;

        if (src[j] == ':')
        {
            int symlen = j-offs;

            if (symlen >= MAX_SYMLEN)
                symlen = MAX_SYMLEN-1;

            memcpy(addsym, &src[offs], symlen);
            addsym[symlen] = 0;

            offs = j+1;
            while ((src[offs] == ' ') || (src[offs] == '\t'))
                offs++;
        }
    }

    if ((src[offs] == 0) || (src[offs] == '\n'))
    {
        if (addsym[0])
        {
            sym_define(addsym, addsymaddr);
            resolve_pending(outbuffer, false);
        }
        return true;
    }

    for (j=0; asmtab[j].name; j++)
    {
        if (strncasecmp(asmtab[j].name, &src[offs], 3) == 0)
            break;
    }

    if (!asmtab[j].name)
    {
        fprintf(stderr, "Unrecognised opcode '%c%c%c': '%s'\n", src[offs], src[offs+1], src[offs+2], isolated_line(thisline));
        return false;
    }

    offs += 3;
    if (!decode_operand(&src[offs], &amode, &val, &pref, &preftyp))
    {
        fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline));
        return false;
    }

//    printf("%s: amode is %u\n", isolated_line(thisline), amode);

    switch (amode)
    {
        case AM_IMP:
            if (asmtab[j].imp == -1) { fprintf(stderr, "Operand expected: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].imp;
            break;

        case AM_IMM:
            if (asmtab[j].imm == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].imm;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val;                
            break;

        case AM_ABS:
            if (asmtab[j].rel != -1)
            {
                outbuffer[(*asmaddr)++] = asmtab[j].rel;

                if (NULL != pref)
                {
                    if (pref->num[PRT_REL] >= MAX_REFS)
                    {
                        fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                        exit(EXIT_FAILURE);
                    }

                    pref->ref[PRT_REL][pref->num[PRT_REL]++] = (*asmaddr);
                    k = 0;
                }
                else
                {
                    k = ((int)val)-((int)((*asmaddr)+1));
                    if ((k < -128) || (k > 127))
                    {
                        fprintf(stderr, "Branch out of range: '%s' (address was decoded as 0x%04x)\n", isolated_line(thisline), val);
                        return false;
                    }
                }

                outbuffer[(*asmaddr)++] = k&0xff;
                break;
            }

            if ((asmtab[j].zp != -1) && ((val&0xff00)==0) && (NULL == pref))
            {
                outbuffer[(*asmaddr)++] = asmtab[j].zp;
                outbuffer[(*asmaddr)++] = val;
                break;
            }

            if (asmtab[j].abs == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].abs;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val&0xff;
            outbuffer[(*asmaddr)++] = (val>>8)&0xff;
            break;

        case AM_IND:
            if (asmtab[j].ind == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].ind;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val&0xff;
            outbuffer[(*asmaddr)++] = (val>>8)&0xff;
            break;

        case AM_ZPX:
            if (asmtab[j].zpx == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].zpx;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val;
            break;

        case AM_ZPY:
            if (asmtab[j].zpy == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].zpy;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val;
            break;

        case AM_ZIX:
            if (asmtab[j].zix == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].zix;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val;
            break;

        case AM_ZIY:
            if (asmtab[j].ziy == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].ziy;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val;
            break;

        case AM_ABX:
            if ((asmtab[j].zpx != -1) && ((val&0xff00)==0) && (NULL == pref))
            {
                outbuffer[(*asmaddr)++] = asmtab[j].zpx;
                outbuffer[(*asmaddr)++] = val;
                break;
            }

            if (asmtab[j].abx == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].abx;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }

            outbuffer[(*asmaddr)++] = val&0xff;
            outbuffer[(*asmaddr)++] = (val>>8)&0xff;
            break;

        case AM_ABY:
            if ((asmtab[j].zpy != -1) && ((val&0xff00)==0 ) && (NULL == pref))
            {
                outbuffer[(*asmaddr)++] = asmtab[j].zpy;
                outbuffer[(*asmaddr)++] = val;
                break;
            }

            if (asmtab[j].aby == -1) { fprintf(stderr, "Illegal operand: '%s'\n", isolated_line(thisline)); return false; }
            outbuffer[(*asmaddr)++] = asmtab[j].aby;

            if (NULL != pref)
            {
                if (pref->num[preftyp] >= MAX_REFS)
                {
                    fprintf(stderr, "Too many pending references to '%s'\n", pref->name);
                    exit(EXIT_FAILURE);
                }

                pref->ref[preftyp][pref->num[preftyp]++] = (*asmaddr);
            }
            outbuffer[(*asmaddr)++] = val&0xff;
            outbuffer[(*asmaddr)++] = (val>>8)&0xff;
            break;
    }

    if (addsym[0])
    {
        sym_define(addsym, addsymaddr);
        resolve_pending(outbuffer, false);
    }

    return true;
}


void assemble(const char *src, uint8_t *outbuffer, uint16_t *asmaddr)
{
    int offs=0;

    while (src[offs])
    {
        if (((*asmaddr) < TAP_START) || ((*asmaddr) >= MAX_TAPADDR))
        {
            fprintf(stderr, "Out of space!\n");
            exit(EXIT_FAILURE);
        }

        if (!assemble_line(src, offs, outbuffer, asmaddr))
            exit(EXIT_FAILURE);

        while ((src[offs] != 0) && (src[offs] != '\n'))
            offs++;

        if (src[offs] == '\n')
            offs++;
    }

}
