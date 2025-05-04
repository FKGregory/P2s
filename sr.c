#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "gbn.h"
#include <string.h>


/* ******************************************************************
   Go Back N protocol.  Adapted from J.F.Kurose
   ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.2  

   Network properties:
   - one way network delay averages five time units (longer if there
   are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
   or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
   (although some can be lost).

   Modifications: 
   - removed bidirectional GBN code and other code not used by prac. 
   - fixed C style to adhere to current programming style
   - added GBN implementation
**********************************************************************/

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet */
#define SEQSPACE 2*WINDOWSIZE    /* the min sequence space for SR must be at least 2*windowsize */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */
#define MAX_TIME 1e9 /* crazy timer value to indicate timer not set*/

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver  
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your 
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/

int ComputeChecksum(struct pkt packet) /*unchanged*/
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for ( i=0; i<20; i++ ) 
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet) /*unchanged*/
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

static struct pkt buffer[SEQSPACE];  /* array for storing packets waiting for ACK */
static bool isAcked[SEQSPACE]; /*track whether packet has been acked*/
/*static float timers[SEQSPACE]; make per packet timers*/
static int ABase ;           /* the sequence number of the first packet in the window */
static int A_nextseqnum;     /* the sequence number of the next packet to be sent */
static float timers[SEQSPACE]; /* array of timers for each packet */


/*Side A Output functionality*/
void A_output(struct msg message){
    struct pkt sendpkt;
  /* if not blocked waiting on ACK */
  if (((A_nextseqnum - ABase + SEQSPACE) % SEQSPACE) < WINDOWSIZE){

    if (TRACE > 1)
    printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

    /* create packet */
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    memset(sendpkt.payload, 0, sizeof(sendpkt.payload));  /* Clear payload*/
    memcpy(sendpkt.payload, message.data, 20);
    sendpkt.checksum = ComputeChecksum(sendpkt);
    timers[sendpkt.seqnum] = RTT;
    buffer[sendpkt.seqnum] = sendpkt; /* store packet in buffer*/
    isAcked[sendpkt.seqnum] = false; /*mark packet as not acked*/

    /* send out packet */
    if (TRACE > 0)
    printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3 (A, sendpkt);

    if (ABase == A_nextseqnum) { /*start timer if first packet in window*/
        starttimer(A,RTT);
    };

    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;  /*increment sequence number*/
  } else{
        if (TRACE > 0) {
          printf("----A: New message arrives, send window is full\n");
            /*future me pls make it buffer here */
        }
    }
}   


/* called from layer 3, when a packet arrives for layer 4 
   In this practical this will always be an ACK as B never sends data.
*/
void A_input(struct pkt packet) /*NEED TO CODE A WAY TO DEAL WITH DUPLICATE ACKS*/
{
    bool has_unacked;
    int acknum;
    float min_remaining;
    int i;

  /* if received ACK is not corrupted */ 
  if (!IsCorrupted(packet)) {
    if (TRACE > 0)
     printf("----A: uncorrupted ACK %d is received\n",packet.acknum);
    acknum = packet.acknum;  /*get the ack number from the packet*/

    if(!isAcked[acknum]){
        isAcked[acknum] = true; /*mark packet as acked*/
        timers[acknum] = MAX_TIME; /*stop timer for this packet*/
        if(TRACE > 0){
          printf("----A: ACK %d is not a duplicate\n",packet.acknum);
        }

    }else{if(TRACE>0){
      printf ("----A: duplicate ACK received, do nothing!\n");}
    }
    while(isAcked[ABase]){ /* check if the base packet is acked*/
      ABase = (ABase + 1) % SEQSPACE;
    }
        stoptimer(A);
        min_remaining = RTT;
        has_unacked = false;
        for (i = 0; i < SEQSPACE; i++) {
           if (!isAcked[i] && timers[i] != MAX_TIME) {
            if (timers[i] < min_remaining){
              min_remaining = timers[i];
              has_unacked = true;
            }
          }
       }

      if (has_unacked){
        starttimer(A, min_remaining);}

      }else {
    if (TRACE > 0)
    printf ("----A: corrupted ACK is received, do nothing!\n");
  }
}


/* called when A's timer goes off */
void A_timerinterrupt(struct pkt packet){ 
    bool has_unacked;
    float min_remaining;
    int i;
    int seq;
    
    seq = packet.seqnum;

    has_unacked = false;
    min_remaining = RTT;

    if (TRACE > 0){
      printf("----A: time out,resend packets!\n");
      for(i = 0; i < SEQSPACE; i++){
          if (!isAcked[i] && timers[i] != MAX_TIME) {
            timers[i] -= RTT;
            if (timers[i] <= 0) {
                if (TRACE > 0){
                  printf("---A: resending packet %d\n", buffer[seq].seqnum);}
                tolayer3(A, buffer[i]);
                timers[i] = RTT; 
            }
          }
      } stoptimer(A);
      for (i = 0; i < SEQSPACE; i++) {
          if (!isAcked[i] && timers[i] != MAX_TIME) {
              if (timers[i] < min_remaining) {
                  min_remaining = timers[i];
                  has_unacked = true;
              }
          }
        }
  if (has_unacked){
    starttimer(A, min_remaining);}
}};

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
   int i;
  ABase = 0; /*set first seq num to 0*/
  A_nextseqnum = 0; /*set next seq num to 0*/
    for (i = 0; i < SEQSPACE; i++){
        isAcked[i] = false; /*mark all packets as not acked*/
        timers[i] = MAX_TIME; /*set all timers to max time*/
    }
}


/********* Receiver (B)  variables and procedures ************/

static bool isAckedB[SEQSPACE]; /*track whether packet has been acked*/
static int expectedseqnum; /* the sequence number of the next packet to be received */
static struct pkt bufferB[SEQSPACE]; /* array for storing packets waiting for ACK */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
    struct pkt ackpkt;
    if (!IsCorrupted(packet)) {               /*If packet not corrupted*/
        int seq = packet.seqnum;              /*Extract sequence number*/

        if (!isAckedB[seq]) {                 /*If this is the first time receiving it*/
            isAckedB[seq] = true;             /*Mark as received*/
            bufferB[seq] = packet;            /*Store packet in buffer*/
        }
        if (TRACE > 0){
          printf("----B: packet %d is correctly received, send ACK!\n",packet.seqnum);}

        /*Deliver in-order packets starting from expectedseqnum*/
        while (isAckedB[expectedseqnum]) {
          tolayer5(B, bufferB[expectedseqnum].payload);
          isAckedB[expectedseqnum] = false;
          expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
          /*if (expectedseqnum == seq) {
            // deliver the actual received packet so simulator counts it
            tolayer5(B, packet.payload);
        } else {
            // deliver buffered packet
            tolayer5(B, bufferB[expectedseqnum].payload);
        }
        isAckedB[expectedseqnum] = false;
        expectedseqnum = (expectedseqnum + 1) % SEQSPACE;*/
        }

        /*Send ACK for this packet*/
        ackpkt.seqnum = 0;                    /*Not used*/
        ackpkt.acknum = seq;        /*ACK the received packet*/
        memset(ackpkt.payload, 0, sizeof(ackpkt.payload)); /*Clear payload*/
        ackpkt.checksum = ComputeChecksum(ackpkt); /*Calculate checksum*/

        tolayer3(B, ackpkt);                  /*Send ACK*/
}
    else {                                   /*if packet corrupted*/
        if (TRACE > 0)                       /*Print trace message*/
          printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
    }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
  int i;
  expectedseqnum = 0;

  for (i = 0; i < SEQSPACE; i++) {
    isAckedB[i] = false;  /*Mark all packets as not received*/
    memset(&bufferB[i], 0, sizeof(struct pkt)); /*init buffer b*/
}}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)  
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}