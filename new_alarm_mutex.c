/*
 * alarm_cond.c
 *
 * This is an enhancement to the alarm_mutex.c program, which
 * used only a mutex to synchronize access to the shared alarm
 * list. This version adds a condition variable. The alarm
 * thread waits on this condition variable, with a timeout that
 * corresponds to the earliest timer request. If the main thread
 * enters an earlier timeout, it signals the condition variable
 * so that the alarm thread will wake up and process the earlier
 * timeout first, requeueing the later request.
 */

#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    char                message[64];
    int                 id;
    int                 thread; 
} alarm_t;

/* initialize all the mutexes and conditionals */
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t display_1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;

/* initialize the alarm list */
alarm_t *alarm_list = NULL;

/* iniliatize all the counters */
int current_alarm = 0;

int thread1count = 0;
int thread2count = 0;
int thread3count = 0;

int thread1 = 0;
int thread2 = 0;
int thread3 = 0;

int display_thread_count = 0;

/* set current alarm node to NULL */
alarm_t *curr_alarm = NULL;

/*
 * Insert alarm entry on list, in order.
 */
void alarm_insert (alarm_t *alarm)
{
    /* declare status */
    int status;

    /* declare the last and next alarm */
    alarm_t **last, *next;

    /*
     * LOCKING PROTOCOL:
     * 
     * This routine requires that the caller have locked the
     * alarm_mutex!
     */


    last = &alarm_list;
    next = *last;

    /* as long as next is not NULL and the id of next is greater or equal */
    /* the link of the alarm becomes the next one and the last node becomes the alarm */
    while (next != NULL) {
        if (next->id >= alarm->id) {
            alarm->link = next;
            *last = alarm;
            break;
        }
        last = &next->link;
        next = next->link;
    }
    /*
     * If we reached the end of the list, insert the new alarm
     * there.  ("next" is NULL, and "last" points to the link
     * field of the last item, or to the list header.)
     */
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }
#ifdef DEBUG
    printf ("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf ("%d[\"%s\"]   ", next->id, next->message);
    printf ("]\n");
#endif
    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */
    if (current_alarm == 0 || alarm->id < current_alarm) {
        current_alarm = alarm->id;
        status = pthread_cond_signal (&alarm_cond);
        if (status != 0)
            err_abort (status, "Signal cond");
    }
}


/* Display 1 start routine */
void *display_thread(void *thread_id)
{
      int status;
      alarm_t *alarm;
      time_t now;

      /*
      * Loop forever, processing alarms. The display thread will
      * be disintegrated when the process exits.
      */
      while(1)
      {
        status = pthread_mutex_lock(&alarm_mutex);
          if (status != 0)
          {
            err_abort(status, "Lock mutex");
          }

          /* 
          * Wait for the alarm thread to signal when an alarm is ready to be
          * procced.
          */

            alarm = curr_alarm;
            status = pthread_cond_wait(&display_1, &alarm_mutex);

          /* display thread has received the alarm */
          printf("Display Thread %d: Received Alarm Request at time: %d %s\n", alarm->thread, alarm->seconds, alarm->message);
          
          now = time (NULL);
          /* print a message every 5 seconds */
          do {
                printf("Alarm(%d) Printed by Alarm Display Thread 1 at %ld %s\n", \
                    alarm->id, time (NULL), alarm->message);
                sleep(5);
            } while (alarm->time > time(NULL));

          /* current alarm has expired */
        if(alarm->thread == 1) {
            thread1count -= 1;
          } else if (alarm->thread == 2) {
            thread2count -= 1;
          } else {
            thread3count -= 1;
          }
        /* print a message to say the alarm thread has ecpired*/ 
        printf("Display Thread %d: Alarm Expired at %ld: "
          "%d %s\n", alarm->thread, time (NULL), alarm->seconds, alarm->message);
        printf("Alarm Thread Removed Alarm( %d at %ld : %d.\n", alarm->id, time(NULL), alarm->seconds);
      status = pthread_mutex_unlock(&alarm_mutex);
          if (status != 0)
          err_abort(status, "unlock mutex");

     }	
}


/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status;

    pthread_t display1thread; /* Display thread 1 */
    pthread_t display2thread; /* Display thread 2 */
    pthread_t display3thread; /* Display thread 3 */

    curr_alarm = NULL;
    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits. Lock the mutex
     * at the start -- it will be unlocked during condition
     * waits, so the main thread can insert alarms.
    */

    while (1) {
        status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");

        int created = 0;

        /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;

        while (alarm_list == NULL) {
            status = pthread_cond_wait (&alarm_cond, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
        }

        /*
         * If the alarm list is not empty, print the id of the alarm list
         */
        alarm = alarm_list;
        alarm_list = alarm->link;

        /*
         * If the alarm list is not empty, wait until an alarm is
         * added. 
         */
        if (alarm != NULL) {
          status = pthread_mutex_unlock (&alarm_mutex);
			    if (status != 0)
			        err_abort (status, "Unlock mutex");

          curr_alarm = alarm;

            /*
            * If the number of existing display alarm threads is less
            * than 3; a new display alarm thread is created and 
            * assigned that alarm to the newly created display alarm thread. 
            */

            if(display_thread_count < 3)
              display_thread_count++;
              created = 1;
             {
              if(thread1 == 0) 
              {
                  // update booleans for if the thread exists as well as increase the count
                status = pthread_create (
	                &display1thread, NULL, display_thread, NULL);
                thread1 = 1;
                thread1count += 1;
                alarm->thread = (int)display1thread;

                printf("\nAlarm Thread Created New Display Alarm Thread %d For Alarm(%i) at %ld: %d %s\n",
                              alarm->thread, alarm->id, time(NULL), alarm->seconds, alarm->message);  

              } else if(thread2 == 0){
                status = pthread_create (
	                &display2thread, NULL, display_thread, NULL);
                thread2 = 1;
                thread2count += 1;
                alarm->thread = (int)display2thread;
                
                printf("\nAlarm Thread Created New Display Alarm Thread %d For Alarm(%i) at %ld: %d %s\n",
                              alarm->thread, alarm->id, time(NULL), alarm->seconds, alarm->message); 
      
              } else if (thread3 == 0){
                status = pthread_create (
	                &display3thread, NULL, display_thread, NULL);
                thread3 = 1;
                thread3count += 1;
                alarm->thread = (int)display3thread;
                printf("\nAlarm Thread Created New Display Alarm Thread %d For Alarm(%i) at %ld: %d %s\n",
                              (int)display3thread, alarm->id, time(NULL), alarm->seconds, alarm->message);
              }
            }

            /*
            * If the number of existing display alarm threads is atleat
            * than 3; the alarm gets assigned to an existing display alarm thread 
            * which has the smallest number of alarms assigned to it 
            */
           
            if(display_thread_count >= 3 && created == 0) {
              if(thread1count <= thread2count && thread1count <= thread3count) {
                printf("Alarm Thread Display Alarm Thread1 Assigned to Display Alarm(%i) at %ld: %d %s\n",
                              alarm->id, time(NULL), alarm->seconds, alarm->message);
		            alarm->thread = (int)display1thread;

              } else if(thread2count <= thread3count && thread2count <= thread1count) {
                printf("Alarm Thread Display Alarm Thread2 Assigned to Display Alarm(%i) at %ld: %d %s\n",
                                alarm->id, time(NULL), alarm->seconds, alarm->message);
                alarm->thread = (int)display2thread;

              } else {
                printf("Alarm Thread Display Alarm Thread3 Assigned to Display Alarm(%i) at %ld: %d %s\n",
                                alarm->id, time(NULL), alarm->seconds, alarm->message);
                alarm->thread = (int)display3thread;
              }
            }


         /* wait until the alarm_mutex is added */ 
          status = pthread_mutex_lock (&alarm_mutex);
          if (status != 0)
            err_abort (status, "unlock mutex");

          /* wait until display_1 is added */ 
          status = pthread_cond_signal(&display_1);
		      if (status != 0)
	    	 	  err_abort(status, "Signal cond");

          status = pthread_cond_signal(&display_1);
		      if (status != 0)
	    	 	  err_abort(status, "Signal cond");

          /* if the expiry time of that alarm has been reached 
          * and the display alarm thread does not have any more (display1thread == 0), 
          * then terminate that display alarm thread.  */

          if (--display1thread == 0)
          {     
                printf("\nAlarm Thread Terminated Display Thread %i at %ld", (int)display1thread, time(NULL));
                thread1 = 0;
                display_thread_count -= 1;
                pthread_cancel(display1thread);

          }

          if (--display2thread == 0)
          {
                printf("\nAlarm Thread Terminated Display Thread %i at %ld", (int)display1thread, time(NULL));
                thread2 = 0;
                display_thread_count -= 1;
                pthread_cancel(display2thread);

          }

          if (--display3thread == 0)
          {
                printf("\nAlarm Thread Terminated Display Thread %i at %ld", (int)display1thread, time(NULL));
                thread3 = 0;
                display_thread_count -= 1;
                pthread_cancel(display3thread);

          }
        }
#ifdef DEBUG
            printf ("[waiting: %d(%ld)\"%s\"]\n", alarm->id,
                alarm->time - time (NULL), alarm->message);
            printf ("[current: %d(%ld)\"%s\"]\n", curr_alarm->id,
                curr_alarm->time - time (NULL), curr_alarm->message);    
#endif
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "unlock mutex");
    }
}

void alarm_edit (alarm_t *alarm)
{
    int status;
    alarm_t **last, *next;

    /*
     * LOCKING PROTOCOL:
     * 
     * This routine requires that the caller have locked the
     * alarm_mutex!
     */

    last = &alarm_list;
    next = *last;

    /* as long as next is not NULL and the id of next alarm is greater or equal
    * copy the meassage of next to be the message of alarm
    */
    while (next != NULL) {
        if (next->id == alarm->id) {
            strcpy(alarm->message, next->message);
            alarm->seconds = next->seconds;
            *last = next;
            break;
        }
        last = &next->link;
        next = next->link;
    }
    /*
     * If we reached the end of the list, then there was
     * not the correct id to change
     */
    if (next == NULL) {
        err_abort (status, "Not found");
    }
 #ifdef DEBUG
    printf ("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf ("%d(%d)[\"%s\"] ", next->seconds, next->id, next->message);
    printf ("]\n");
 #endif
}

int main (int argc, char *argv[])
{
  /* declare all variables */
    char request[100];
    char command;
    int status;
    char line[128];
    alarm_t *alarm;
	const char delim[2] = "(";
	char *token;
	pthread_t thread;
	const char delim1[2] = "(";
    const char delim2[2] = ")";
    char *comm;
    char *id;  
    char *num;
    char *p;
    int isDigit = 0;

    /* crearte the thread */ 
    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");


    /*
      * Loop forever, to get user input. Allocate memory for
      * the alarms being inputed
      */

    while (1) {
        printf ("alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        if (sscanf (line, "%s %d %64[^\n]", 
            request, &alarm->seconds, alarm->message) < 3) {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        } else {
            time_t t = time(NULL);
			
            /*Split input request by delimeter1*/
		    comm = strtok(request, delim1); 
			num = strtok(NULL, delim1);
		
			/*Check if string contains ')' and if so, split by delimeter2*/
			if (comm != NULL && num != NULL && strchr(num, ')') != NULL) id = strtok(num, delim2);
			
			/*Check if thread  was inputted*/
			if (id == NULL || strlen(id) == 0) fprintf(stderr, "Bad Command\n");
			
			/*Check if command is either Change_Alarm or Start_Alarm*/
			else if (strcmp(comm, "Change_Alarm") != 0 && strcmp(comm, "Start_Alarm") != 0 ) fprintf(stderr, "Bad Command\n");
			
			else {	
				/*Check if is all numbers*/
				for (p = id; *p; p++) {
				   if (!isdigit(*p)) {
					   printf("Bad Command\n");
					   isDigit = 1;
					   break;
				   }				   
			   }
			   /*Convert thread id to int*/
			   if (isDigit == 0) {
            int numid = atoi(id);
            alarm->id = numid;
          if (alarm->id >= 0) {
            if(strcmp(request, "Start_Alarm") == 0) {

              status = pthread_mutex_lock (&alarm_mutex);
              if (status != 0)
                err_abort (status, "Lock print mutex");

              alarm->time = time (NULL) + alarm->seconds;
              /*
              * Insert the new alarm into the list of alarms,
              * sorted by Message Type
              */
              alarm_insert(alarm);
              printf("Alarm(%d) Inserted by Main Thread %ld Into Alarm List at %ld: %s\n", 
              alarm->id, (long)pthread_self(), time (NULL), alarm->message );
              status = pthread_mutex_unlock (&alarm_mutex);
              
              if (status != 0)
                err_abort (status, "Unlock mutex");

            } 
            else if (strcmp(request, "Change_Alarm") == 0) {

              status = pthread_mutex_lock (&alarm_mutex);
              if (status != 0)
                err_abort (status, "Lock print mutex");

              alarm_edit(alarm);

              printf("Alarm(%d) Changed at at %ld: %s\n", alarm->id, time (NULL), alarm->message);
              status = pthread_mutex_unlock (&alarm_mutex);
              if (status != 0)
                err_abort (status, "Unlock mutex");

            }
          }		   
				}
			}          
    }
  }
}