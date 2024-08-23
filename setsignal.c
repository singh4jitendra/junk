#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>

#include "setsignal.h"

/*
 ******************************************************************************
 *
 * Name:        setsignal
 *
 * Description: This function initializes a sigaction structure and makes
 *              a call to sigaction().    (50128)
 *
 * Parameters:  sig_no           - The number of the signal being altered.
 *              sig_handler      - Pointer to signal handler function.
 *              old_action       - Pointer to store information about the
 *                                 old signal handler.
 *              sig_flag         - Flags used by the sa_flag field of the
 *                                 sigaction structure.
 *              num_sigs_in_mask - Number of signals to add to the sa_mask
 *                                 field of the sigaction structure.  These
 *                                 signals will be blocked while the
 *                                 signal handler for this signal is being
 *                                 executed.
 *              ...              - Signal numbers to add to the signal mask.
 *                                 The number of signals included here should
 *                                 equal the value of num_sigs_in_mask.
 *
 * Returns: 0 if successful, -1 on an error
 ******************************************************************************
*/
int setsignal(int sig_no, void (*sig_handler)(int),
   struct sigaction *old_action, int sig_flag, unsigned int num_sigs_in_mask,
   ...)
{
   struct sigaction new_action;
   va_list ap;
   int count;
   int sig_to_mask;

   new_action.sa_handler = sig_handler;
   sigemptyset(&new_action.sa_mask);
   new_action.sa_flags = sig_flag;

   if (num_sigs_in_mask > 0) {
      va_start(ap, num_sigs_in_mask);

      /* loop to get all of the signal numbers to add to the mask */
      for (count = 0; count < num_sigs_in_mask; count++) {
         sig_to_mask = va_arg(ap, int);
         sigaddset(&new_action.sa_mask, sig_to_mask);
      }

      va_end(ap);
   }

   return sigaction(sig_no, &new_action, old_action);
}
