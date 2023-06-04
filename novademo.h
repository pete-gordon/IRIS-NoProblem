#ifndef _NOVADEMO_H__
#define _NOVADEMO_H__

#define NUM_ENTRIES(x) ((sizeof(x))/sizeof((x)[0]))

#define TAP_FILE "./novademo.tap"
#define SYM_FILE "./novademo.sym"
#define TAP_START (0x400)
#define MAX_TAPADDR (0x9800)
#define MAX_TAPSIZE (MAX_TAPADDR - TAP_START)

#endif /* _NOVADEMO_H__ */
