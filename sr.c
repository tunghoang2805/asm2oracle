#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT 16.0
#define WINDOWSIZE 6
#define SEQSPACE (2 * WINDOWSIZE)
#define NOTINUSE (-1)

int ComputeChecksum(struct pkt packet) {
    int checksum = 0;
    int i;
    checksum = packet.seqnum;
    checksum += packet.acknum;
    for (i = 0; i < 20; i++)
        checksum += (int)(packet.payload[i]);
    return checksum;
}

bool IsCorrupted(struct pkt packet) {
    return packet.checksum != ComputeChecksum(packet);
}

/********* Sender (A) ************/
static struct pkt buffer[WINDOWSIZE];
static int windowfirst, windowlast;
static int windowcount;
static int A_nextseqnum;
static bool acked[WINDOWSIZE];
static int timer_seq;

void A_output(struct msg message) {
    struct pkt sendpkt;
    int i;

    if (windowcount < WINDOWSIZE) {
        sendpkt.seqnum = A_nextseqnum;
        sendpkt.acknum = NOTINUSE;
        for (i = 0; i < 20; i++)
            sendpkt.payload[i] = message.data[i];
        sendpkt.checksum = ComputeChecksum(sendpkt);

        windowlast = (windowlast + 1) % WINDOWSIZE;
        buffer[windowlast] = sendpkt;
        acked[windowlast] = false;
        windowcount++;

        tolayer3(A, sendpkt);

        if (windowcount == 1) {
            starttimer(A, RTT);
            timer_seq = sendpkt.seqnum;
        }

        A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
    } else {
        window_full++;
    }
}

void A_input(struct pkt packet) {
    int i, j;
    int seqfirst = buffer[windowfirst].seqnum;

    if (!IsCorrupted(packet)) {
        total_ACKs_received++;

        for (i = 0; i < windowcount; i++) {
            int idx = (windowfirst + i) % WINDOWSIZE;
            if (buffer[idx].seqnum == packet.acknum) {
                if (!acked[idx]) {
                    new_ACKs++;
                    acked[idx] = true;
                    bool need_new_timer = (packet.acknum == timer_seq);

                    while (windowcount > 0 && acked[windowfirst]) {
                        acked[windowfirst] = false;
                        windowfirst = (windowfirst + 1) % WINDOWSIZE;
                        windowcount--;
                    }

                    if (windowcount == 0) {
                        stoptimer(A);
                    } else if (need_new_timer) {
                        stoptimer(A);
                        for (j = 0; j < windowcount; j++) {
                            int idx = (windowfirst + j) % WINDOWSIZE;
                            if (!acked[idx]) {
                                timer_seq = buffer[idx].seqnum;
                                starttimer(A, RTT);
                                break;
                            }
                        }
                    }
                }
                break;
            }
        }
    }
}

void A_timerinterrupt(void) {
    int i;
    for (i = 0; i < windowcount; i++) {
        int idx = (windowfirst + i) % WINDOWSIZE;
        if (buffer[idx].seqnum == timer_seq && !acked[idx]) {
            tolayer3(A, buffer[idx]);
            packets_resent++;
            starttimer(A, RTT);
            break;
        }
    }
}

void A_init(void) {
    A_nextseqnum = 0;
    windowfirst = 0;
    windowlast = -1;
    windowcount = 0;
    timer_seq = 0;
    int i;
    for (i = 0; i < WINDOWSIZE; i++)
        acked[i] = false;
}

/********* Receiver (B) ************/
static int B_nextseqnum;
static int rcv_base;
static bool received[WINDOWSIZE];
static struct pkt rcv_buffer[WINDOWSIZE];

void B_input(struct pkt packet) {
    struct pkt sendpkt;
    int i, offset;

    if (!IsCorrupted(packet)) {
        packets_received++;
        offset = (packet.seqnum - rcv_base + SEQSPACE) % SEQSPACE;

        if (offset < WINDOWSIZE && !received[offset]) {
            rcv_buffer[offset] = packet;
            received[offset] = true;

            if (offset == 0) {
                while (received[0]) {
                    tolayer5(B, rcv_buffer[0].payload);
                    for (i = 0; i < WINDOWSIZE-1; i++) {
                        received[i] = received[i+1];
                        rcv_buffer[i] = rcv_buffer[i+1];
                    }
                    received[WINDOWSIZE-1] = false;
                    rcv_base = (rcv_base + 1) % SEQSPACE;
                }
            }
        }

        sendpkt.acknum = packet.seqnum;
        sendpkt.seqnum = B_nextseqnum;
        B_nextseqnum = (B_nextseqnum + 1) % 2;
        for (i = 0; i < 20; i++)
            sendpkt.payload[i] = '0';
        sendpkt.checksum = ComputeChecksum(sendpkt);
        tolayer3(B, sendpkt);
    }
}

void B_init(void) {
    rcv_base = 0;
    B_nextseqnum = 1;
    int i;
    for (i = 0; i < WINDOWSIZE; i++)
        received[i] = false;
}

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
