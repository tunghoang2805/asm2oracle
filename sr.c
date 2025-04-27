#include <stdio.h>
#include <stdlib.h>
#include "emulator.h"
#include "sr.h"

#define RTT 16.0
#define WINDOWSIZE 4
#define SEQSPACE 8  // Must be 2*WINDOWSIZE
#define NOTINUSE (-1)

// Sender state
static struct pkt buffer[WINDOWSIZE];
static bool acked[WINDOWSIZE];  // Tracks ACK status
static int window_base = 0;
static int next_seq = 0;
static int window_count = 0;

// Receiver state
static struct pkt recv_window[WINDOWSIZE];
static bool received[WINDOWSIZE] = {false};
static int expected_seq = 0;

int ComputeChecksum(struct pkt packet) {
    int checksum = packet.seqnum + packet.acknum;
    for(int i=0; i<20; i++)
        checksum += packet.payload[i];
    return checksum;
}

bool IsCorrupted(struct pkt packet) {
    return packet.checksum != ComputeChecksum(packet);
}

//================= SENDER (A) FUNCTIONS =================//
void A_output(struct msg message) {
    if(window_count < WINDOWSIZE) {
        struct pkt packet;
        packet.seqnum = next_seq;
        packet.acknum = NOTINUSE;
        for(int i=0; i<20; i++)
            packet.payload[i] = message.data[i];
        packet.checksum = ComputeChecksum(packet);
        
        int index = next_seq % WINDOWSIZE;
        buffer[index] = packet;
        acked[index] = false;
        
        tolayer3(A, packet);
        if(window_count == 0) 
            starttimer(A, RTT);
        
        next_seq = (next_seq + 1) % SEQSPACE;
        window_count++;
    }
}

void A_input(struct pkt packet) {
    if(!IsCorrupted(packet)) {
        int ack_seq = packet.acknum;
        if(ack_seq >= window_base || ack_seq < (window_base + WINDOWSIZE) % SEQSPACE) {
            int index = ack_seq % WINDOWSIZE;
            if(!acked[index]) {
                acked[index] = true;
                // Slide window to first unACKed packet
                while(acked[window_base % WINDOWSIZE] && window_count > 0) {
                    window_base = (window_base + 1) % SEQSPACE;
                    window_count--;
                }
                if(window_count > 0)
                    starttimer(A, RTT);
                else
                    stoptimer(A);
            }
        }
    }
}

void A_timerinterrupt() {
    for(int i=0; i<WINDOWSIZE; i++) {
        if(!acked[i] && buffer[i].seqnum >= window_base && 
           buffer[i].seqnum < (window_base + WINDOWSIZE) % SEQSPACE) {
            tolayer3(A, buffer[i]);
        }
    }
    starttimer(A, RTT);
}

void A_init() {
    window_base = 0;
    next_seq = 0;
    window_count = 0;
    for(int i=0; i<WINDOWSIZE; i++)
        acked[i] = false;
}

//================= RECEIVER (B) FUNCTIONS =================//
void B_input(struct pkt packet) {
    if(!IsCorrupted(packet)) {
        int seq = packet.seqnum;
        struct pkt ack_pkt;
        
        // Send ACK for this packet
        ack_pkt.acknum = seq;
        ack_pkt.checksum = ComputeChecksum(ack_pkt);
        tolayer3(B, ack_pkt);

        if(seq >= expected_seq && seq < (expected_seq + WINDOWSIZE) % SEQSPACE) {
            int index = seq % WINDOWSIZE;
            if(!received[index]) {
                recv_window[index] = packet;
                received[index] = true;
                
                // Deliver in-order packets
                while(received[expected_seq % WINDOWSIZE]) {
                    tolayer5(B, recv_window[expected_seq % WINDOWSIZE].payload);
                    received[expected_seq % WINDOWSIZE] = false;
                    expected_seq = (expected_seq + 1) % SEQSPACE;
                }
            }
        }
    }
}

void B_init() {
    expected_seq = 0;
    for(int i=0; i<WINDOWSIZE; i++)
        received[i] = false;
}
