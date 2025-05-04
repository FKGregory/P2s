# include "emulator.h"
#include "sr.h"
#include "sr.c"
#include "emulator.c"
int main() {
    init();           // sets up event list, inputs, etc.

    while (1) {
        struct event *eventptr = evlist;    // get next event
        if (eventptr == NULL)
            break;

        evlist = evlist->next;   // remove this event

        time = eventptr->evtime;
        if (eventptr->evtype == FROM_LAYER5) {
            // Call A_output or B_output depending on entity
        } else if (eventptr->evtype == FROM_LAYER3) {
            // Call A_input or B_input depending on entity
        } else if (eventptr->evtype == TIMER_INTERRUPT) {
            // Call A_timerinterrupt or B_timerinterrupt
        }

        free(eventptr);  // clean up event
    }

    // Print stats
    return 0;
}