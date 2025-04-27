#ifndef SR_H
#define SR_H

#include "emulator.h"

/* External function declarations for Selective Repeat sender (A) */
extern void A_init(void);
extern void A_input(struct pkt packet);
extern void A_output(struct msg message);
extern void A_timerinterrupt(void);

/* External function declarations for Selective Repeat receiver (B) */
extern void B_init(void);
extern void B_input(struct pkt packet);

/* Bidirectional communication support (not used in base implementation) */
#define BIDIRECTIONAL 0 /* 0 = A->B 1 = A<->B */
extern void B_output(struct msg message);  // Reserved for future extension
extern void B_timerinterrupt(void);        // Reserved for future extension

#endif /* SR_H */
