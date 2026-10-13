
/* clkhandler.c - clkhandler */

#include <xinu.h>

/*------------------------------------------------------------------------
 * clkhandler - high level clock interrupt handler
 *------------------------------------------------------------------------
 */
void	clkhandler()
{
	static	uint32	count1000 = 1000;	/* Count to 1000 ms	*/
	static	uint32	priority_boost_timer = PRIORITY_BOOST_PERIOD;	/* Timer to track when to boost */

	ctr1000++;		/* Track the ms passed */

	/* Increment runtime of current process */
	proctab[currpid].runtime++;
	proctab[currpid].queue_time++;

	/* Decrement the priority booster timer and call priority boost if it reached 0 */
	if((--priority_boost_timer) <= 0) {

		/* Boost priority of all user processes */
		boost_flag = TRUE;

		/* Reset the local timer for the next boost */
		priority_boost_timer = PRIORITY_BOOST_PERIOD;
	}

	/* Decrement the ms counter, and see if a second has passed */
	if((--count1000) <= 0) {

		/* One second has passed, so increment seconds count */

		clktime++;

		/* Reset the local ms counter for the next second */

		count1000 = 1000;
	}

	/* Handle sleeping processes if any exist */

	if(!isempty(sleepq)) {

		/* Decrement the delay for the first process on the	*/
		/*   sleep queue, and awaken if the count reaches zero	*/

		if((--queuetab[firstid(sleepq)].qkey) <= 0) {
			wakeup();
		}
	}

	/* Decrement the preemption counter, and reschedule when the */
	/*   remaining time reaches zero			     */

	if((--preempt) <= 0) {
		preempt = QUANTUM;
		resched();
	}
}
