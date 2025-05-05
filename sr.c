#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

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
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet
                          MUST BE SET TO 6 when submitting assignment */
#define SEQSPACE 12      /* the min sequence space for GBN must be at least windowsize + 1 */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for ( i=0; i<20; i++ )
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

static struct pkt buffer[SEQSPACE];  /* array for storing packets waiting for ACK */
static struct pkt bufferB [SEQSPACE];  /* array for storing packets waiting for ACK */
static int windowfirst, windowlast;    /* array indexes of the first/last packet awaiting ACK */
static int windowcount;                /* the number of packets currently awaiting an ACK */
static int A_nextseqnum;               /* the next sequence number to be used by the sender */
static float timers[SEQSPACE];         /* array of timers for each packet */
static int isAcked[SEQSPACE];          /*track whether packet has been acked*/
static bool recieved[SEQSPACE];         /*track whether packet has been received*/

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message)
{
  struct pkt sendpkt;
  int i;

  if (((A_nextseqnum - windowfirst + SEQSPACE) % SEQSPACE) < WINDOWSIZE){
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");
     
    /* create packet */
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for ( i=0; i<20 ; i++ )
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    buffer[sendpkt.seqnum] = sendpkt; /* store packet in buffer*/
    isAcked[sendpkt.seqnum] = 0; /*mark packet as not acked*/
    timers[sendpkt.seqnum] = RTT;

    /* get next sequence number, wrap back to 0 */
    printf("Anext seq num %d\n", A_nextseqnum);

    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
    printf("seqspace \n", SEQSPACE);
    printf("Anext seq num  inced %d\n", A_nextseqnum);



    /* send out packet */
    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3 (A, sendpkt);

    if (windowfirst == A_nextseqnum) { /*start timer if first packet in window*/
      starttimer(A,RTT);
  };

  }
  /* if blocked,  window is full */
  else {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;
  }
}


/* called from layer 3, when a packet arrives for layer 4
   In this practical this will always be an ACK as B never sends data.
*/
void A_input(struct pkt packet)
{

  /* if received ACK is not corrupted */
  if (!IsCorrupted(packet)) {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d is received\n",packet.acknum);
    total_ACKs_received++;

    /*check in window*/
    if(((packet.acknum - windowfirst + SEQSPACE) % SEQSPACE) < (A_nextseqnum - windowfirst + SEQSPACE) % SEQSPACE){
      /* check if new ACK or duplicate */
      if (!isAcked[packet.acknum]) {
          isAcked[packet.acknum] = 1; /*mark packet as acked*/
          new_ACKs++;

          if (TRACE > 0)
            printf("----A: ACK %d is not a duplicate\n",packet.acknum);

          while ((windowfirst != A_nextseqnum) && isAcked[windowfirst]) {
            timers[windowfirst] = NOTINUSE;
            windowfirst = (windowfirst + 1) % SEQSPACE;
          }
          
          stoptimer(A);
          if (windowfirst != A_nextseqnum)
            starttimer(A, RTT);

        }
        else
          if (TRACE > 0)
        printf ("----A: duplicate ACK received, do nothing!\n");
  }}
  else
    if (TRACE > 0)
      printf ("----A: corrupted ACK is received, do nothing!\n");
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
  int i;

  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

  for(i=0; i<SEQSPACE; i++) {
    if(isAcked[i] == 0 && timers[i] != NOTINUSE){
      timers[i] -= RTT;
      if (timers[i] <= 0) {
        if (TRACE > 0)
          printf ("---A: resending packet %d\n", buffer[i].seqnum);
        tolayer3(A, buffer[i]);
        packets_resent++;
        timers[i] = RTT; 
      }
    }
  }
}


/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{ int i;
  /* initialise A's window, buffer and sequence number */
  A_nextseqnum = 0;  /* A starts with seq num 0, do not change this */
  windowfirst = 0;
  windowlast = -1;   /* windowlast is where the last packet sent is stored.
		     new packets are placed in winlast + 1
		     so initially this is set to -1
		   */
  windowcount = 0;
  total_ACKs_received = 0;
  new_ACKs = 0;
      for (i = 0; i < SEQSPACE; i++) {
        isAcked[i] = 1;         /*start things acked*/
        timers[i] = NOTINUSE;    /*start timers off*/
    }
}

/********* Receiver (B)  variables and procedures ************/

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  int i;

  /* if not corrupted and received packet is in order */
  if  (!IsCorrupted(packet)) {
    if(!recieved[packet.seqnum]) {
      recieved[packet.seqnum] = 1;
      bufferB[packet.seqnum] = packet;
      if (TRACE > 0)
      printf("----B: packet %d is correctly received, send ACK!\n",packet.seqnum);
      
      /* deliver to receiving application */
      tolayer5(B, packet.payload);

      packets_received++;

    }
      /* create packet */
  sendpkt.seqnum = NOTINUSE;
  sendpkt.acknum = packet.seqnum;

  /* we don't have any data to send.  fill payload with 0's */
  for ( i=0; i<20 ; i++ )
    sendpkt.payload[i] = '0';

  /* computer checksum */
  sendpkt.checksum = ComputeChecksum(sendpkt);

  /* send out packet */
  tolayer3 (B, sendpkt);
  }
  else {
    /* packet is corrupted or out of order resend last ACK */
    if (TRACE > 0)
      printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
  }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
  int i;
  for (i = 0; i < SEQSPACE; i++) {
      recieved[i] = 0;
  }
}

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
