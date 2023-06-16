#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "noproblem.h"
#include "asm6502.h"
#include "music.h"
#include "musicdata.h"

extern uint8_t tapdata[65536];

#ifndef PLY_CFG_ConfigurationIsPresent
#define PLY_CFG_UseHardwareSounds  1
#define PLY_CFG_UseRetrig  1
#define PLY_CFG_NoSoftNoHard  1
#define PLY_CFG_NoSoftNoHard_Noise  1
#define PLY_CFG_SoftOnly  1
#define PLY_CFG_SoftOnly_Noise  1
#define PLY_CFG_SoftToHard  1
#define PLY_CFG_SoftToHard_Noise  1
#define PLY_CFG_SoftToHard_Retrig  1
#define PLY_CFG_HardOnly  1
#define PLY_CFG_HardOnly_Noise  1
#define PLY_CFG_HardOnly_Retrig  1
#define PLY_CFG_SoftAndHard  1
#define PLY_CFG_SoftAndHard_Noise  1
#define PLY_CFG_SoftAndHard_Retrig  1
#endif

//Agglomerates the hardware sound configuration flags, because they are treated the same in this player.
#ifdef PLY_CFG_SoftToHard
#define PLY_AKY_USE_SoftAndHard_Agglomerated 1
#endif
#ifdef PLY_CFG_SoftAndHard
#define PLY_AKY_USE_SoftAndHard_Agglomerated 1
#endif
#ifdef PLY_CFG_HardToSoft
#define PLY_AKY_USE_SoftAndHard_Agglomerated 1
#endif
#ifdef PLY_CFG_HardOnly
#define PLY_AKY_USE_SoftAndHard_Agglomerated 1
#endif

#ifdef PLY_CFG_SoftToHard_Noise
#define PLY_AKY_USE_SoftAndHard_Noise_Agglomerated 1
#endif
#ifdef PLY_CFG_SoftAndHard_Noise
#define PLY_AKY_USE_SoftAndHard_Noise_Agglomerated 1
#endif
#ifdef PLY_CFG_HardToSoft_Noise
#define PLY_AKY_USE_SoftAndHard_Noise_Agglomerated 1
#endif

#ifdef PLY_AKY_USE_SoftAndHard_Noise_Agglomerated
#define PLY_AKY_USE_Noise 1
#endif
#ifdef PLY_CFG_NoSoftNoHard_Noise
#define PLY_AKY_USE_Noise 1
#endif
#ifdef PLY_CFG_SoftOnly_Noise
#define PLY_AKY_USE_Noise 1
#endif

void gen_music(uint16_t *addr)
{
    uint16_t smcaddr;
    int i;

    assemble(musicdata, tapdata, addr);
    for (i=0; i<NUM_ENTRIES(musfixups); i++)
    {
        smcaddr = sym_get(musfixups[i].srcsym);
        sym_define(musfixups[i].dstsym, smcaddr + musfixups[i].offset);
    }

    resolve_and_remove_temporary_syms(tapdata);

    sym_define("VIA_ORA", 0x030f);
    sym_define("VIA_PCR", 0x030c);

//PLY_AKY_START
//            JMP PLY_AKY_PLAY
    assemble("PLY_AKY_INIT:\n"
             "            LDA #>Main_Subsong0\n"
             "            STA ptDATA\n"
             "            LDA #<Main_Subsong0\n"
             "            STA ptDATA_hi\n"
             //Skips the header.
             "            LDY #01\n"                                          //Skips the format version.
             "            LDA (ptDATA),Y\n"                                   //Channel count.
             "            STA ACCA\n"
             "            CLC\n"
             "            LDA ptDATA\n"
             "            ADC #2\n"
             "            STA ptDATA\n"
             "            LDA ptDATA_hi\n"
             "            ADC #00\n"
             "            STA ptDATA_hi\n"
             "PLY_AKY_INIT_SKIPHEADERLOOP:\n"                                 //There is always at least one PSG to skip.
             "            CLC\n"
             "            LDA ptDATA\n"
             "            ADC #4\n"
             "            STA ptDATA\n"
             "            LDA ptDATA_hi\n"
             "            ADC #00\n"
             "            STA ptDATA_hi\n"
             "            LDA ACCA\n"
             "            SEC\n"
             "            SBC #03\n"                                             //A PSG is three channels.
             "            BEQ PLY_AKY_INIT_SKIPHEADEREND\n"
             "            STA ACCA\n"
             "            BCS PLY_AKY_INIT_SKIPHEADERLOOP\n"                     //Security in case of the PSG channel is not a multiple of 3.
             "PLY_AKY_INIT_SKIPHEADEREND:\n"
             "            LDA ptDATA\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_OVER_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_OVER_pl5\n"              //ptData now points on the Linker.
             "            LDA #$18\n"        // CLC
             "            STA PLY_AKY_CHANNEL1_REGISTERBLOCKLINESTATE_OPCODE\n"
             "            STA PLY_AKY_CHANNEL2_REGISTERBLOCKLINESTATE_OPCODE\n"
             "            STA PLY_AKY_CHANNEL3_REGISTERBLOCKLINESTATE_OPCODE\n"
             "            LDA #$01\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_pl1\n"
             "            LDA #$00\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_pl5\n"
             "            RTS\n"
             //       Plays the music. It must have been initialized before.
             "PLY_AKY_START:\n"
             "PLY_AKY_PLAY:\n"
             "PLY_AKY_PATTERNFRAMECOUNTER:\n"
             "            LDA #$01\n"
             "            STA ptDATA\n"
             "            LDA #$00\n"
             "            STA ptDATA_hi\n"
             "            LDA ptDATA\n"
             "            BNE .boop\n"
             "            DEC ptDATA_hi\n"
             ".boop:\n"
             "            DEC ptDATA\n"
             "            LDA ptDATA\n"
             "            ORA ptDATA_hi\n"
             "            BEQ PLY_AKY_PATTERNFRAMECOUNTER_OVER\n"
             "            LDA ptDATA\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_pl5\n"
             "            JMP PLY_AKY_CHANNEL1_WAITBEFORENEXTREGISTERBLOCK\n"
             "PLY_AKY_PATTERNFRAMECOUNTER_OVER:\n"
             //The pattern is not over.
             "PLY_AKY_PTLINKER:\n"
             "            LDA #$AC:\n"                                            //Points on the Pattern of the linker.
             "            STA pt2_DT:\n"
             "            LDA #$AC:\n"
             "            STA pt2_DT_hi\n"
             "            LDY #00\n"                                             //Gets the duration of the Pattern, or 0 if end of the song.
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA_hi\n"
             "            ORA ptDATA\n"
             "            BNE PLY_AKY_LINKERNOTENDSONG\n"
             //End of the song. Where to loop?
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             //We directly point on the frame counter of the pattern to loop to.
             "            STA pt2_DT_hi\n"
             "            LDA ptDATA\n"
             "            STA pt2_DT\n"
             //Gets the duration again. No need to check the end of the song,
             //we know it contains at least one pattern.
             "            LDY #00\n"
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA_hi\n"
             "PLY_AKY_LINKERNOTENDSONG:\n"
             "            LDA ptDATA\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_pl5\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA PLY_AKY_CHANNEL1_PTTRACK_pl1\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA PLY_AKY_CHANNEL1_PTTRACK_pl5\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA PLY_AKY_CHANNEL2_PTTRACK_pl1\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA PLY_AKY_CHANNEL2_PTTRACK_pl5\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA PLY_AKY_CHANNEL3_PTTRACK_pl1\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA PLY_AKY_CHANNEL3_PTTRACK_pl5\n"
             "            CLC\n"
             "            LDA pt2_DT\n"
             "            ADC #08\n"                                             // fix pt2_DT value
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_OVER_pl1\n"
             "            LDA pt2_DT_hi\n"
             "            ADC #00\n"
             "            STA PLY_AKY_PATTERNFRAMECOUNTER_OVER_pl5\n"
             //Resets the RegisterBlocks of the channel >1. The first one is skipped so there is no need to do so.
             "            LDA #01\n"
             "            STA PLY_AKY_CHANNEL2_WAITBEFORENEXTREGISTERBLOCK_pl1\n"
             "            STA PLY_AKY_CHANNEL3_WAITBEFORENEXTREGISTERBLOCK_pl1\n"
             "            JMP PLY_AKY_CHANNEL1_WAITBEFORENEXTREGISTERBLOCK_OVER\n", tapdata, addr);

    smcaddr = sym_get("PLY_AKY_PATTERNFRAMECOUNTER");
    sym_define("PLY_AKY_PATTERNFRAMECOUNTER_pl1", smcaddr+1);
    sym_define("PLY_AKY_PATTERNFRAMECOUNTER_pl5", smcaddr+5);

    smcaddr = sym_get("PLY_AKY_PATTERNFRAMECOUNTER_OVER");
    sym_define("PLY_AKY_PATTERNFRAMECOUNTER_OVER_pl1", smcaddr+1);
    sym_define("PLY_AKY_PATTERNFRAMECOUNTER_OVER_pl5", smcaddr+5);

    resolve_and_remove_temporary_syms(tapdata);

    // =====================================
    //Reading the Tracks.
    // =====================================
    assemble("PLY_AKY_CHANNEL1_WAITBEFORENEXTREGISTERBLOCK:\n"
             "            LDA #01\n"                                             //Frames to wait before reading the next RegisterBlock. 0 = finished.
             "            STA ACCA\n"
             "            DEC ACCA\n"
             "            BNE PLY_AKY_CHANNEL1_REGISTERBLOCK_PROCESS\n"
             "PLY_AKY_CHANNEL1_WAITBEFORENEXTREGISTERBLOCK_OVER:\n"
             //This RegisterBlock is finished. Reads the next one from the Track.
             //Obviously, starts at the initial state.
             "            LDA #$18\n"                                            // Carry clear (return to initial state)
             "            STA PLY_AKY_CHANNEL1_REGISTERBLOCKLINESTATE_OPCODE\n"
             "PLY_AKY_CHANNEL1_PTTRACK:\n"
             "            LDA #$AC\n"                                            //Points on the Track.
             "            STA pt2_DT\n"
             "            LDA #$AC\n"
             "            STA pt2_DT_hi\n"

             "            LDY #00\n"                                             //Gets the duration.
             "            LDA (pt2_DT),Y\n"
             "            STA ACCA\n"
             "            INY\n"                                                 //Reads the RegisterBlock address.
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA_hi\n"
             "            CLC\n"
             "            LDA pt2_DT\n"
             "            ADC #03\n"
             "            STA PLY_AKY_CHANNEL1_PTTRACK_pl1\n"
             "            LDA pt2_DT_hi\n"
             "            ADC #00\n"
             "            STA PLY_AKY_CHANNEL1_PTTRACK_pl5\n"

             "            LDA ptDATA\n"
             "            STA PLY_AKY_CHANNEL1_PTREGISTERBLOCK_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_CHANNEL1_PTREGISTERBLOCK_pl5\n"
             //A is the duration of the block.
             "PLY_AKY_CHANNEL1_REGISTERBLOCK_PROCESS:\n"
             //Processes the RegisterBlock, whether it is the current one or a new one.
             "            LDA ACCA\n"
             "            STA PLY_AKY_CHANNEL1_WAITBEFORENEXTREGISTERBLOCK_pl1\n"

             "PLY_AKY_CHANNEL2_WAITBEFORENEXTREGISTERBLOCK:\n"
             "            LDA #$01\n"                                            //Frames to wait before reading the next RegisterBlock. 0 = finished.
             "            STA ACCA\n"
             "            DEC ACCA\n"
             "            BNE PLY_AKY_CHANNEL2_REGISTERBLOCK_PROCESS\n"
             "PLY_AKY_CHANNEL2_WAITBEFORENEXTREGISTERBLOCK_OVER:\n"
             //This RegisterBlock is finished. Reads the next one from the Track.
             //Obviously, starts at the initial state.
             "            LDA #$18\n"                                            // Carry clear (return to initial state)
             "            STA PLY_AKY_CHANNEL2_REGISTERBLOCKLINESTATE_OPCODE\n"
             "PLY_AKY_CHANNEL2_PTTRACK:\n"
             "            LDA #$AC\n"                                            //Points on the Track.
             "            STA pt2_DT\n"
             "            LDA #$AC\n"
             "            STA pt2_DT_hi\n"

             "            LDY #00\n"                                             //Gets the duration.
             "            LDA (pt2_DT),Y\n"
             "            STA ACCA\n"
             "            INY\n"                                                 //Reads the RegisterBlock address.
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA_hi\n"
             "            CLC\n"
             "            LDA pt2_DT\n"
             "            ADC #03\n"
             "            STA PLY_AKY_CHANNEL2_PTTRACK_pl1\n"
             "            LDA pt2_DT_hi\n"
             "            ADC #00\n"
             "            STA PLY_AKY_CHANNEL2_PTTRACK_pl5\n"

             "            LDA ptDATA\n"
             "            STA PLY_AKY_CHANNEL2_PTREGISTERBLOCK_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_CHANNEL2_PTREGISTERBLOCK_pl5\n"
             //A is the duration of the block.
             "PLY_AKY_CHANNEL2_REGISTERBLOCK_PROCESS:\n"
             //Processes the RegisterBlock, whether it is the current one or a new one.
             "            LDA ACCA\n"
             "            STA PLY_AKY_CHANNEL2_WAITBEFORENEXTREGISTERBLOCK_pl1\n"

             "PLY_AKY_CHANNEL3_WAITBEFORENEXTREGISTERBLOCK:\n"
             "            LDA #$01\n"                                            //Frames to wait before reading the next RegisterBlock. 0 = finished.
             "            STA ACCA\n"
             "            DEC ACCA\n"
             "            BNE PLY_AKY_CHANNEL3_REGISTERBLOCK_PROCESS\n"
             "PLY_AKY_CHANNEL3_WAITBEFORENEXTREGISTERBLOCK_OVER:\n"
             //This RegisterBlock is finished. Reads the next one from the Track.
             //Obviously, starts at the initial state.
             "            LDA #$18\n"                                            // Carry clear (return to initial state)
             "            STA PLY_AKY_CHANNEL3_REGISTERBLOCKLINESTATE_OPCODE\n"

             "PLY_AKY_CHANNEL3_PTTRACK:\n"
             "            LDA #$AC\n"                                            //Points on the Track.
             "            STA pt2_DT\n"
             "            LDA #$AC\n"
             "            STA pt2_DT_hi\n"

             "            LDY #00\n"                                             //Gets the duration.
             "            LDA (pt2_DT),Y\n"
             "            STA ACCA\n"
             "            INY\n"                                                 //Reads the RegisterBlock address.
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA\n"
             "            INY\n"
             "            LDA (pt2_DT),Y\n"
             "            STA ptDATA_hi\n"
             "            CLC\n"
             "            LDA pt2_DT\n"
             "            ADC #03\n"
             "            STA PLY_AKY_CHANNEL3_PTTRACK_pl1\n"
             "            LDA pt2_DT_hi\n"
             "            ADC #00\n"
             "            STA PLY_AKY_CHANNEL3_PTTRACK_pl5\n"

             "            LDA ptDATA\n"
             "            STA PLY_AKY_CHANNEL3_PTREGISTERBLOCK_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_CHANNEL3_PTREGISTERBLOCK_pl5\n"
             //A is the duration of the block.
             "PLY_AKY_CHANNEL3_REGISTERBLOCK_PROCESS:\n"
             //Processes the RegisterBlock, whether it is the current one or a new one.
             "            LDA ACCA\n"
             "            STA PLY_AKY_CHANNEL3_WAITBEFORENEXTREGISTERBLOCK_pl1\n", tapdata, addr);

    sym_define(".F_SET_REG", 0xFF);
    sym_define(".F_INACTIVE", 0xDD);
    sym_define(".F_WRITE_DATA", 0xFD);

             // =====================================
             //Reading the RegisterBlock.
             // =====================================
    assemble("            LDA #08\n"
             "            STA volumeRegister\n"                                  // first volume register
             "            LDX #00\n"                                             // first frequency register
             "            LDY #00\n"                                             // Y = 0 (all the time)
             // Register 7 with default values: fully sound-open but noise-close.
             //R7 has been shift twice to the left, it will be shifted back as the channels are treated.
             "            LDA #$E0\n"
             "            STA r7\n"
             //Channel 1
             "PLY_AKY_CHANNEL1_PTREGISTERBLOCK:\n"
             "            LDA #$AC\n"                                            //Points on the data of the RegisterBlock to read.
             "            STA ptDATA\n"
             "            LDA #$AC\n"
             "            STA ptDATA_hi\n"
             "PLY_AKY_CHANNEL1_REGISTERBLOCKLINESTATE_OPCODE:\n"
             "            CLC\n"                                                 // Clear C (initial STATE) / Set C (non initial STATE)
             "            JSR PLY_AKY_READREGISTERBLOCK\n"
             "            LDA #$38\n"                                            // opcode for SEC (no more initial state)
             "            STA PLY_AKY_CHANNEL1_REGISTERBLOCKLINESTATE_OPCODE\n"
             "            LDA ptDATA\n"                                          //This is new pointer on the RegisterBlock.
             "            STA PLY_AKY_CHANNEL1_PTREGISTERBLOCK_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_CHANNEL1_PTREGISTERBLOCK_pl5\n"

             //Channel 2
             //Shifts the R7 for the next channels.
             "            LSR r7\n"
             "PLY_AKY_CHANNEL2_PTREGISTERBLOCK:\n"
             "            LDA #$AC\n"                                            //Points on the data of the RegisterBlock to read.
             "            STA ptDATA\n"
             "            LDA #$AC\n"
             "            STA ptDATA_hi\n"
             "PLY_AKY_CHANNEL2_REGISTERBLOCKLINESTATE_OPCODE:\n"
             "            CLC\n"                                                 // Clear C (initial STATE) / Set C (non initial STATE)
             "            JSR PLY_AKY_READREGISTERBLOCK\n"
             "            LDA #$38\n"                                            // opcode for SEC (no more initial state)
             "            STA PLY_AKY_CHANNEL2_REGISTERBLOCKLINESTATE_OPCODE\n"
             "            LDA ptDATA\n"                                          //This is new pointer on the RegisterBlock.
             "            STA PLY_AKY_CHANNEL2_PTREGISTERBLOCK_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_CHANNEL2_PTREGISTERBLOCK_pl5\n"

             //Channel 3
             //Shifts the R7 for the next channels.
             "            ROR r7\n"
             "PLY_AKY_CHANNEL3_PTREGISTERBLOCK:\n"
             "            LDA #$AC\n"                                            //Points on the data of the RegisterBlock to read.
             "            STA ptDATA\n"
             "            LDA #$AC\n"
             "            STA ptDATA_hi\n"
             "PLY_AKY_CHANNEL3_REGISTERBLOCKLINESTATE_OPCODE:\n"
             "            CLC\n"                                                 // Clear C (initial STATE) / Set C (non initial STATE)
             "            JSR PLY_AKY_READREGISTERBLOCK\n"
             "            LDA #$38\n"                                            // opcode for SEC (no more initial state)
             "            STA PLY_AKY_CHANNEL3_REGISTERBLOCKLINESTATE_OPCODE\n"
             "            LDA ptDATA\n"                                          //This is new pointer on the RegisterBlock.
             "            STA PLY_AKY_CHANNEL3_PTREGISTERBLOCK_pl1\n"
             "            LDA ptDATA_hi\n"
             "            STA PLY_AKY_CHANNEL3_PTREGISTERBLOCK_pl5\n"

             //Almost all the channel specific registers have been sent. Now sends the remaining registers (6, 7, 11, 12, 13).
             //Register 7. Note that managing register 7 before 6/11/12 is done on purpose.
             "            LDA #07\n"                                             // Register 7
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA r7\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

#ifdef PLY_AKY_USE_Noise
             "            LDA #06\n"                                             // Register 6
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA PLY_AKY_PSGREGISTER6\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"
#endif

#ifdef PLY_CFG_UseHardwareSounds
             "            LDA #11 \n"                                            // Register 11
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA PLY_AKY_PSGREGISTER11\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA #12\n"                                             // Register 12
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA PLY_AKY_PSGREGISTER12\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "PLY_AKY_PSGREGISTER13_CODE:\n"
             "            LDA PLY_AKY_PSGREGISTER13:\n"
             "PLY_AKY_PSGREGISTER13_RETRIG:\n"
             "            CMP #$FF\n"                                            //If IsRetrig?, force the R13 to be triggered.
             "            BEQ PLY_AKY_PSGREGISTER13_END\n"
             "            STA PLY_AKY_PSGREGISTER13_RETRIG_pl1\n"

             "            LDA #13\n"                                             // Register 13
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA PLY_AKY_PSGREGISTER13\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"
             "PLY_AKY_PSGREGISTER13_END:\n"
#endif
             "PLY_AKY_EXIT:\n"
             "            RTS\n", tapdata, addr);
             // -----------------------------------------------------------------------------
             //Generic code interpreting the RegisterBlock
             // IN:   PtData = First Byte
             //       Carry = 0 = initial state, 1 = non-initial state.
             // -----------------------------------------------------------------------------
    assemble("PLY_AKY_READREGISTERBLOCK:\n"
             "            PHP\n"                                                 // save c
             "            LDA (ptDATA),Y\n"
             "            STA ACCA\n"
             "            INC ptDATA\n"
             "            BNE .boop1\n"
             "            INC ptDATA_hi\n"
             ".boop1:\n"
             "            PLP\n"                                                 // restore c
             "            BCC .boop2:\n"
             "            JMP PLY_AKY_RRB_NONINITIALSTATE\n"
             ".boop2:\n"
             //Initial state.
             "            ROR ACCA\n"
             "            BCC .boop3\n"
             "            JMP PLY_AKY_RRB_IS_SOFTWAREONLYORSOFTWAREANDHARDWARE\n"
             ".boop3:\n"

             "            ROR ACCA\n"
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "            BCS PLY_AKY_RRB_IS_HARDWAREONLY\n"
#endif
             // -----------------------------------------------------------------------------
             //Generic code interpreting the RegisterBlock - Initial state.
             // IN:   ptData = Points after the first byte.
             //       ACCA (A) = First byte, twice shifted to the right (type removed).
             //       r7 = Register 7. All sounds are open (0) by default, all noises closed (1).
             //       volumeRegister = Volume register.
             //       X = LSB frequency register.
             //       Y used
             //
             // OUT:  ptData MUST point after the structure.
             //       r7 = updated (ONLY bit 2 and 5).
             //       volumeRegister = Volume register increased of 1 (*** IMPORTANT! The code MUST increase it, even if not using it! ***)
             //       X = LSB frequency register, increased of 2.
             //       Y = 0
             // -----------------------------------------------------------------------------
             "PLY_AKY_RRB_IS_NOSOFTWARENOHARDWARE:\n"
             //No software no hardware.
             "            ROR ACCA\n"                                           //Noise?
             "            BCC PLY_AKY_RRB_NIS_NOSOFTWARENOHARDWARE_READVOLUME\n"
             //There is a noise. Reads it.
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER6\n"
             "            INC ptDATA\n"
             "            BNE .boop557\n"
             "            INC ptDATA_hi\n"
             ".boop557:\n"
             //Opens the noise channel.
             "            LDA r7\n"
             "            AND #%11011111\n"                                      // reset bit 5 (open)
             "            STA r7\n"

             "PLY_AKY_RRB_NIS_NOSOFTWARENOHARDWARE_READVOLUME:\n"
             //The volume is now in b0-b3.
             //and %1111      //No need, the bit 7 was 0.

             //Sends the volume.
             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA ACCA\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC volumeRegister\n"                                  //Increases the volume register.
             "            INX\n"                                                 //Increases the frequency register.
             "            INX\n"

             //Closes the sound channel.
             "            LDA r7\n"
             "            ORA #%00000100\n"                                      // set bit 2 (close)
             "            STA r7\n"
             "            RTS\n"
             // -------------------------------------
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "PLY_AKY_RRB_IS_HARDWAREONLY:\n"
             //Retrig?
             "            ROR ACCA\n"
             "            BCC PLY_AKY_RRB_IS_HO_NORETRIG\n"
             "            LDA ACCA\n"
             "            ORA #%10000000\n"
             "            STA ACCA\n"
             "            STA PLY_AKY_PSGREGISTER13_RETRIG_pl1\n"                  //A value to make sure the retrig is performed, yet A can still be use.
             "PLY_AKY_RRB_IS_HO_NORETRIG:\n"
             //Noise?
             "            ROR ACCA\n"
             "            BCC PLY_AKY_RRB_IS_HO_NONOISE\n"
             //Reads the noise.
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER6\n"
             "            INC ptDATA\n"
             "            BNE .boop611\n"
             "            INC ptDATA_hi\n"
             ".boop611:\n"
             //Opens the noise channel.
             "            LDA r7\n"
             "            AND #%11011111\n"                                      // reset bit 5 (open)
             "            STA r7\n"
             "PLY_AKY_RRB_IS_HO_NONOISE:\n"
             //The envelope.
             "            LDA ACCA\n"
             "            AND #15\n"
             "            STA PLY_AKY_PSGREGISTER13\n"
             //Copies the hardware period.
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER11\n"
             "            INC ptDATA\n"
             "            BNE .boop627\n"
             "            INC ptDATA_hi\n"
             ".boop627:\n"
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER12\n"
             "            INC ptDATA\n"
             "            BNE .boop633\n"
             "            INC ptDATA_hi\n"
             ".boop633:\n"
             //Closes the sound channel.
             "            LDA r7\n"
             "            ORA #%00000100\n"                                      // set bit 2 (close)
             "            STA r7\n"

             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA #$FF\n"                                            //Value (volume to 16)
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC volumeRegister\n"                                  //Increases the volume register.
             "            INX\n"                                                 //Increases the frequency register (mandatory!).
             "            INX\n"
             "            RTS\n"
#endif
             // -------------------------------------
             "PLY_AKY_RRB_IS_SOFTWAREONLYORSOFTWAREANDHARDWARE:\n"
             //Another decision to make about the sound type.
             "            ROR ACCA\n"
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "            BCC .boop665:\n"
             "            JMP PLY_AKY_RRB_IS_SOFTWAREANDHARDWARE\n"
             ".boop665:\n"
#endif
             //Software only. Structure: 0vvvvntt.
             //Noise?
             "            ROR ACCA\n"
             "            BCC PLY_AKY_RRB_IS_SOFTWAREONLY_NONOISE\n"
             //Noise. Reads it.
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER6\n"
             "            INC ptDATA\n"
             "            BNE .boop677\n"
             "            INC ptDATA_hi\n"
             ".boop677:\n"
             //Opens the noise channel.
             "            LDA r7\n"
             "            AND #%11011111\n"                                      // reset bit 5 (open)
             "            STA r7\n"

             "PLY_AKY_RRB_IS_SOFTWAREONLY_NONOISE:\n"
             //Reads the volume (now b0-b3).
             //Note: we do NOT peform a "and %1111" because we know the bit 7 of the original byte is 0, so the bit 4 is currently 0. Else the hardware volume would be on!
             //Sends the volume.
             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA ACCA\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC volumeRegister\n"                                  //Increases the volume register.

             //Sends the LSB software frequency.
             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             //Reads the software period.
             "            LDA (ptDATA),Y\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC ptDATA\n"
             "            BNE .boop722\n"
             "            INC ptDATA_hi\n"
             ".boop722:\n"
             "            INX\n"                                                //Increases the frequency register.

             //Sends the MSB software frequency.
             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             //Reads the software period.
             "            LDA (ptDATA),Y\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC ptDATA\n"
             "            BNE .boop744\n"
             "            INC ptDATA_hi\n"
             ".boop744:\n"
             "            INX\n"                                                 //Increases the frequency register.
             "            RTS\n"
             // -------------------------------------
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "PLY_AKY_RRB_IS_SOFTWAREANDHARDWARE:\n"
             //Retrig?
             "            ROR ACCA\n"
#ifdef PLY_CFG_UseRetrig                                      //CONFIG SPECIFIC
             "            BCC PLY_AKY_RRB_IS_SAH_NORETRIG\n"
             "            LDA ACCA\n"
             "            ORA #%10000000\n"
             "            STA PLY_AKY_PSGREGISTER13_RETRIG_pl1\n"                  //A value to make sure the retrig is performed, yet A can still be use.
             "            STA ACCA\n"
             "PLY_AKY_RRB_IS_SAH_NORETRIG:\n"
#endif
             //Noise?
             "            ROR ACCA\n"
#ifdef PLY_AKY_USE_SoftAndHard_Noise_Agglomerated             //CONFIG SPECIFIC
             "            BCC PLY_AKY_RRB_IS_SAH_NONOISE\n"
              //Reads the noise.

             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER6\n"
             "            INC ptDATA\n"
             "            BNE .boop771\n"
             "            INC ptDATA_hi\n"
             ".boop771:\n"
             //Opens the noise channel.
             "            LDA r7\n"
             "            AND #%11011111\n"                                      // reset bit 5 (open noise)
             "            STA r7\n"

             "PLY_AKY_RRB_IS_SAH_NONOISE:\n"
#endif
             //The envelope.
             "            LDA ACCA\n"
             "            AND #15\n"
             "            STA PLY_AKY_PSGREGISTER13\n"

             //Sends the LSB software frequency.
             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             //Reads the software period.
             "            LDA (ptDATA),Y\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC ptDATA\n"
             "            BNE .boop803\n"
             "            INC ptDATA_hi\n"
             ".boop803:\n"
             "            INX\n"                                                 //Increases the frequency register.

             //Sends the MSB software frequency.
             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             //Reads the software period.
             "            LDA (ptDATA),Y\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC ptDATA\n"
             "            BNE .boop825\n"
             "            INC ptDATA_hi\n"
             ".boop825:\n"
             "            INX    \n"                                            //Increases the frequency register.

             //Sets the hardware volume.
             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA #$FF\n"                                            //Value (volume to 16).
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC volumeRegister\n"                                  //Increases the volume register.

             //Copies the hardware period.
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER11\n"
             "            INC ptDATA\n"
             "            BNE .boop851\n"
             "            INC ptDATA_hi\n"
             ".boop851:\n"
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER12\n"
             "            INC ptDATA\n"
             "            BNE .boop857\n"
             "            INC ptDATA_hi\n"
             ".boop857:\n"
             "            RTS\n"
#endif
             // -------------------------------------
             //Manages the loop. This code is put here so that no jump needs to be coded when its job is done.
             "PLY_AKY_RRB_NIS_NOSOFTWARENOHARDWARE_LOOP:\n"
             //Loops. Reads the next pointer to this RegisterBlock.
             "            LDA (ptDATA),Y\n"
             "            STA ACCA\n"
             "            INC ptDATA\n"
             "            BNE .boop869\n"
             "            INC ptDATA_hi\n"
             ".boop869:\n"
             //Makes another iteration to read the new data.
             //Since we KNOW it is not an initial state (because no jump goes to an initial state), we can directly go to the right branching.
             //Reads the first byte.
             "            LDA (ptDATA),Y\n"
             "            STA ptDATA_hi\n"
             "            LDA ACCA\n"
             "            STA ptDATA\n"
             "            LDA (ptDATA),Y\n"
             "            STA ACCA\n"
             "            INC ptDATA\n"
             "            BNE .boop882\n"
             "            INC ptDATA_hi\n"
             ".boop882:\n"
             // -----------------------------------------------------------------------------
             //Generic code interpreting the RegisterBlock - Non initial state. See comment about the Initial state for the registers ins/outs.
             // -----------------------------------------------------------------------------
             "PLY_AKY_RRB_NONINITIALSTATE:\n"
             "            ROR ACCA\n"
             "            BCS PLY_AKY_RRB_NIS_SOFTWAREONLYORSOFTWAREANDHARDWARE\n"
             "            ROR ACCA\n"
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "            BCC .boop893\n"
             "            JMP PLY_AKY_RRB_NIS_HARDWAREONLY\n"
             ".boop893:\n"
#endif
             //No software, no hardware, OR loop.
             "            LDA ACCA\n"
             "            STA ACCB\n"
             "            AND #03\n"                                             //Bit 3:loop?/volume bit 0, bit 2: volume?
             "            CMP #02\n"                                             //If no volume, yet the volume is >0, it means loop.
             "            BEQ PLY_AKY_RRB_NIS_NOSOFTWARENOHARDWARE_LOOP\n"
             //No loop: so "no software no hardware".
             //Closes the sound channel.
             "            LDA r7\n"
             "            ORA #%00000100\n"                                      // set bit 2 (close sound)
             "            STA r7\n"
             //Volume? bit 2 - 2.
             "            LDA ACCB\n"
             "            ROR\n"
             "            BCC PLY_AKY_RRB_NIS_NOVOLUME\n"
             "            AND #15\n"
             "            STA ACCA\n"

             //Sends the volume.
             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA ACCA\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "PLY_AKY_RRB_NIS_NOVOLUME:\n"
             //Sadly, have to lose a bit of CPU here, as this must be done in all cases.
             "            INC volumeRegister\n"                                  //Next volume register.
             "            INX\n"                                                 //Next frequency registers.
             "            INX\n"

             //Noise? Was on bit 7, but there has been two shifts. We can't use A, it may have been modified by the volume AND.
             "            LDA #%00100000\n"                                      // bit 7-2
             "            BIT ACCB\n"
             "            BNE .boop939\n"
             "            RTS\n"
             ".boop939:\n"
             //Noise.
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER6\n"
             "            INC ptDATA\n"
             "            BNE .boop946\n"
             "            INC ptDATA_hi\n"
             ".boop946:\n"
             //Opens the noise channel.
             "            LDA r7\n"
             "            AND #%11011111\n"                                      // reset bit 5 (open noise)
             "            STA r7\n"
             "            RTS\n", tapdata, addr);

             // -------------------------------------
    assemble("PLY_AKY_RRB_NIS_SOFTWAREONLYORSOFTWAREANDHARDWARE:\n"
             //Another decision to make about the sound type.
             "            ROR ACCA\n"
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "            BCC .boop960\n"
             "            JMP PLY_AKY_RRB_NIS_SOFTWAREANDHARDWARE\n"
             ".boop960:\n"
#endif
             //Software only. Structure: mspnoise lsp v  v  v  v  (0  1).
             "            LDA ACCA\n"
             "            STA ACCB\n"
             //Gets the volume (already shifted).
             "            AND #15\n"
             "            STA ACCA\n"
             //Sends the volume.
             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA ACCA\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC volumeRegister\n"                                   //Increases the volume register.
             //LSP? (Least Significant byte of Period). Was bit 6, but now shifted.
             "            LDA #%00010000\n"                                      // bit 6-2
             "            BIT ACCB\n"
             "            BEQ PLY_AKY_RRB_NIS_SOFTWAREONLY_NOLSP\n"

             //Sends the LSB software frequency.
             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA (ptDATA),Y\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC ptDATA\n"
             "            BNE .boop1007\n"
             "            INC ptDATA_hi\n"
             ".boop1007:\n"
                                                                // frequency register is not incremented on purpose.
             "PLY_AKY_RRB_NIS_SOFTWAREONLY_NOLSP:\n"
             //MSP AND/OR (Noise and/or new Noise)? (Most Significant byte of Period).
             "            LDA #%00100000\n"                                      // bit 7-2
             "            BIT ACCB\n"
             "            BNE PLY_AKY_RRB_NIS_SOFTWAREONLY_MSPANDMAYBENOISE\n"
             //Bit of loss of CPU, but has to be done in all cases.
             "            INX\n"
             "            INX\n"
             "            RTS\n"
             // -------------------------------------
             "PLY_AKY_RRB_NIS_SOFTWAREONLY_MSPANDMAYBENOISE:\n"
             //MSP and noise?, in the next byte. nipppp (n = newNoise? i = isNoise? p = MSB period).
             "            LDA (ptDATA),Y\n"                                      //Useless bits at the end, not a problem.
             "            STA ACCA\n"
             "            INC ptDATA\n"
             "            BNE .boop1026\n"
             "            INC ptDATA_hi\n"
             ".boop1026:\n"
             //Sends the MSB software frequency.
             "            INX\n"                                                 //Was not increased before.

             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA ACCA\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INX\n"                                                 //Increases the frequency register.
             "            ROL ACCA\n"                                            //Carry is isNoise?
             "            BCS .boop1048\n"
             "            RTS\n"
             //Opens the noise channel.
             ".boop1048:\n"
             "            LDA r7\n"                                              // reset bit 5 (open)
             "            AND #%11011111\n"
             "            STA r7\n"
             //Is there a new noise value? If yes, gets the noise.
             "            ROL ACCA\n"
             "            BCS .boop1057\n"
             "            RTS\n"
             ".boop1057:\n"
             //Gets the noise.
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER6\n"
             "            INC ptDATA\n"
             "            BNE .boop1064\n"
             "            INC ptDATA_hi\n"
             ".boop1064:\n"
             "            RTS\n"
             // -------------------------------------
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "PLY_AKY_RRB_NIS_HARDWAREONLY:\n"
             //Gets the envelope (initially on b2-b4, but currently on b0-b2). It is on 3 bits, must be encoded on 4. Bit 0 must be 0.
             "            ROL ACCA\n"
             "            LDA ACCA\n"
             "            STA ACCB\n"
             "            AND #14\n"
             "            STA PLY_AKY_PSGREGISTER13\n"
             //Closes the sound channel.
             "            LDA r7\n"
             "            ORA #%00000100\n"                                      // set bit 2 (close)
             "            STA r7\n"
             //Hardware volume.
             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA #$FF\n"                                            //Value (16, hardware volume).
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC volumeRegister\n"                                  //Increases the volume register.
             "            INX\n"                                                //Increases the frequency register.
             "            INX\n"

             //LSB for hardware period? Currently on b6.
             "            LDA ACCB\n"
             "            ROL\n"
             "            ROL\n"
             "            STA ACCA\n"
             "            BCC PLY_AKY_RRB_NIS_HARDWAREONLY_NOLSB\n"

             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER11\n"
             "            INC ptDATA\n"
             "            BNE .boop1110\n"
             "            INC ptDATA_hi\n"
             ".boop1110:\n"
             "PLY_AKY_RRB_NIS_HARDWAREONLY_NOLSB:\n"
             //MSB for hardware period?
             "            ROL ACCA\n"
             "            BCC PLY_AKY_RRB_NIS_HARDWAREONLY_NOMSB\n"

             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER12\n"
             "            INC ptDATA\n"
             "            BNE .boop1121:\n"
             "            INC ptDATA_hi\n"
             ".boop1121:\n"
             "PLY_AKY_RRB_NIS_HARDWAREONLY_NOMSB:\n"
             //Noise or retrig?
             "            ROL ACCA\n"
             "            BCC .boop1127\n"
             "            JMP PLY_AKY_RRB_NIS_HARDWARE_SHARED_NOISEORRETRIG_ANDSTOP\n"
             ".boop1127:\n"
             "            RTS\n"
#endif
             // -------------------------------------
#ifdef PLY_AKY_USE_SoftAndHard_Agglomerated                   //CONFIG SPECIFIC
             "PLY_AKY_RRB_NIS_SOFTWAREANDHARDWARE:\n"
             //Hardware volume.
             "            LDA volumeRegister\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA #$FF\n"                                            //Value (16 = hardware volume).
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC volumeRegister\n"                                  //Increases the volume register.
             //LSB of hardware period?
             "            ROR ACCA\n"
             "            BCC PLY_AKY_RRB_NIS_SAHH_AFTERLSBH\n"
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER11\n"
             "            INC ptDATA\n"
             "            BNE .boop1157\n"
             "            INC ptDATA_hi\n"
             ".boop1157:\n"
             "PLY_AKY_RRB_NIS_SAHH_AFTERLSBH:\n"
             //MSB of hardware period?
             "            ROR ACCA\n"
             "            BCC PLY_AKY_RRB_NIS_SAHH_AFTERMSBH\n"
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER12\n"
             "            INC ptDATA\n"
             "            BNE .boop1167\n"
             "            INC ptDATA_hi\n"
             ".boop1167:\n"
             "PLY_AKY_RRB_NIS_SAHH_AFTERMSBH:\n"
             //LSB of software period?
             "            LDA ACCA\n"
             "            ROR\n"
             "            BCC PLY_AKY_RRB_NIS_SAHH_AFTERLSBS\n"
             "            STA ACCB\n"

             //Sends the LSB software frequency.
             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA (ptDATA),Y\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC ptDATA\n"
             "            BNE .boop1193\n"
             "            INC ptDATA_hi\n"
             ".boop1193:\n"
                                                                // frequency register not increased on purpose.
             "            LDA ACCB\n"
             "PLY_AKY_RRB_NIS_SAHH_AFTERLSBS:\n"
             //MSB of software period?
             "            ROR\n"
             "            BCC PLY_AKY_RRB_NIS_SAHH_AFTERMSBS\n"
             "            STA ACCB\n"

             //Sends the MSB software frequency.
             "            INX\n"

             //LDA FreqRegister
             "            STX VIA_ORA\n"
             "            LDA #>.F_SET_REG\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            LDA (ptDATA),Y\n"
             "            STA VIA_ORA\n"
             "            LDA #>.F_WRITE_DATA\n"
             "            STA VIA_PCR\n"
             "            LDA #>.F_INACTIVE\n"
             "            STA VIA_PCR\n"

             "            INC ptDATA\n"
             "            BNE .boop1222\n"
             "            INC ptDATA_hi\n"
             ".boop1222:\n"
             "            DEX\n"                                                 //Yup. Will be compensated below.

             "            LDA ACCB\n"
             "PLY_AKY_RRB_NIS_SAHH_AFTERMSBS:\n"
             //A bit of loss of CPU, but this has to be done every time!
             "            INX\n"
             "            INX\n"

             //New hardware envelope?
             "            ROR\n"
             "            STA ACCA\n"
             "            BCC PLY_AKY_RRB_NIS_SAHH_AFTERENVELOPE\n"
             "            LDA (ptDATA),Y\n"
             "            STA PLY_AKY_PSGREGISTER13\n"
             "            INC ptDATA\n"
             "            BNE .boop1240:\n"
             "            INC ptDATA_hi\n"
             ".boop1240:\n"
             "PLY_AKY_RRB_NIS_SAHH_AFTERENVELOPE:\n"
             //Retrig and/or noise?
             "            LDA ACCA\n"
             "            ROR\n"
             "            BCS .boop1247\n"
             "            RTS\n"
             ".boop1247:\n"
#endif

#ifdef PLY_CFG_UseHardwareSounds                              //CONFIG SPECIFIC
             //This code is shared with the HardwareOnly. It reads the Noise/Retrig byte, interprets it and exits.
             "PLY_AKY_RRB_NIS_HARDWARE_SHARED_NOISEORRETRIG_ANDSTOP:\n"
             //Noise or retrig. Reads the next byte.
             "            LDA (ptDATA),Y\n"
             "            INC ptDATA\n"
             "            BNE .boop1258\n"
             "            INC ptDATA_hi\n"
             ".boop1258:\n"
             //Retrig?
             "            ROR\n"
#ifdef PLY_CFG_UseRetrig                                      //CONFIG SPECIFIC
             "            BCC PLY_AKY_RRB_NIS_S_NOR_NORETRIG\n"
             "            ORA #%10000000\n"
             "            STA PLY_AKY_PSGREGISTER13_RETRIG_pl1\n"                  //A value to make sure the retrig is performed, yet A can still be use.
             "PLY_AKY_RRB_NIS_S_NOR_NORETRIG:\n"
#endif
#ifdef PLY_AKY_USE_SoftAndHard_Noise_Agglomerated             //CONFIG SPECIFIC
             //Noise? If no, nothing more to do.
             "            ROR\n"
             "            STA ACCA\n"
             "            BCS .boop1273\n"
             "            RTS\n"
             ".boop1273:\n"
             //Noise. Opens the noise channel.
             "            LDA r7\n"
             "            AND #%11011111\n"                                      // reset bit 5 (open)
             "            STA r7\n"
             "            LDA ACCA\n"
             //Is there a new noise value? If yes, gets the noise.
             "            ROR\n"
             "            BCS .boop1283\n"
             "            RTS\n"
             ".boop1283:\n"                     //Sets the noise.
             "            STA PLY_AKY_PSGREGISTER6\n"
#endif
             "            RTS\n"
#endif
             , tapdata, addr);

    smcaddr = sym_get("PLY_AKY_CHANNEL1_PTTRACK");
    sym_define("PLY_AKY_CHANNEL1_PTTRACK_pl1", smcaddr+1);
    sym_define("PLY_AKY_CHANNEL1_PTTRACK_pl5", smcaddr+5);

    smcaddr = sym_get("PLY_AKY_CHANNEL2_PTTRACK");
    sym_define("PLY_AKY_CHANNEL2_PTTRACK_pl1", smcaddr+1);
    sym_define("PLY_AKY_CHANNEL2_PTTRACK_pl5", smcaddr+5);

    smcaddr = sym_get("PLY_AKY_CHANNEL3_PTTRACK");
    sym_define("PLY_AKY_CHANNEL3_PTTRACK_pl1", smcaddr+1);
    sym_define("PLY_AKY_CHANNEL3_PTTRACK_pl5", smcaddr+5);

    smcaddr = sym_get("PLY_AKY_CHANNEL1_WAITBEFORENEXTREGISTERBLOCK");
    sym_define("PLY_AKY_CHANNEL1_WAITBEFORENEXTREGISTERBLOCK_pl1", smcaddr+1);

    smcaddr = sym_get("PLY_AKY_CHANNEL2_WAITBEFORENEXTREGISTERBLOCK");
    sym_define("PLY_AKY_CHANNEL2_WAITBEFORENEXTREGISTERBLOCK_pl1", smcaddr+1);

    smcaddr = sym_get("PLY_AKY_CHANNEL3_WAITBEFORENEXTREGISTERBLOCK");
    sym_define("PLY_AKY_CHANNEL3_WAITBEFORENEXTREGISTERBLOCK_pl1", smcaddr+1);

    smcaddr = sym_get("PLY_AKY_CHANNEL1_PTREGISTERBLOCK");
    sym_define("PLY_AKY_CHANNEL1_PTREGISTERBLOCK_pl1", smcaddr+1);
    sym_define("PLY_AKY_CHANNEL1_PTREGISTERBLOCK_pl5", smcaddr+5);

    smcaddr = sym_get("PLY_AKY_CHANNEL2_PTREGISTERBLOCK");
    sym_define("PLY_AKY_CHANNEL2_PTREGISTERBLOCK_pl1", smcaddr+1);
    sym_define("PLY_AKY_CHANNEL2_PTREGISTERBLOCK_pl5", smcaddr+5);

    smcaddr = sym_get("PLY_AKY_CHANNEL3_PTREGISTERBLOCK");
    sym_define("PLY_AKY_CHANNEL3_PTREGISTERBLOCK_pl1", smcaddr+1);
    sym_define("PLY_AKY_CHANNEL3_PTREGISTERBLOCK_pl5", smcaddr+5);

    smcaddr = sym_get("PLY_AKY_PSGREGISTER13_RETRIG");
    sym_define("PLY_AKY_PSGREGISTER13_RETRIG_pl1", smcaddr+1);

    resolve_and_remove_temporary_syms(tapdata);
}