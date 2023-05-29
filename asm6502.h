#ifndef _ASM6502_H__
#define _ASM6502_H__

void assemble(const char *src, uint8_t *outbuffer, uint16_t *asmaddr);
void sym_define(char *symname, uint16_t addr);
void resolve_pending(uint8_t *outbuffer, bool mustresolve);
void dump_syms(FILE *f);

#endif /* _ASM6502_H__ */
