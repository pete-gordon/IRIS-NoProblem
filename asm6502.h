#ifndef _ASM6502_H__
#define _ASM6502_H__

#define MAX_SYMLEN (32)

void assemble(const char *src, uint8_t *outbuffer, uint16_t *asmaddr);
void sym_define(char *symname, uint16_t addr);
uint16_t sym_get(char *symname);
bool sym_set(char *symname);
void resolve_pending(uint8_t *outbuffer, bool mustresolve);
void resolve_and_remove_temporary_syms(uint8_t *outbuffer);
void dump_syms(FILE *f);

#endif /* _ASM6502_H__ */
