// /*
//  * alarm_mutex.c
//  *
//  * This is an enhancement to the alarm_thread.c program, which
//  * created an "alarm thread" for each alarm command. This new
//  * version uses a single alarm thread, which reads the next
//  * entry in a list. The main thread places new requests onto the
//  * list, in order of absolute expiration time. The list is
//  * protected by a mutex, and the alarm thread sleeps for at
//  * least 1 second, each iteration, to ensure that the main
//  * thread can lock the mutex to add new work to the list.
//  */
#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
} alarm_t;

typedef struct alarm_thread_list {
	struct alarm_thread *link;
	pthread_t thread_id;
	int id;
} alarm_thread_t;



pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t alarm_cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t alarm_cond2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t alarm_cond3 = PTHREAD_COND_INITIALIZER;

int display1_count = 0;
int display2_count = 0;
int display3_count = 0;
alarm_t *alarm_list = NULL;
time_t current_alarm = 0;
time_t current_alarm_1 = 0;
time_t current_alarm_2 = 0;
time_t current_alarm_3 = 0;

/*
 * Insert alarm entry on list, in order.
 */
void alarm_insert (alarm_t *alarm)
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
        printf ("%d(%d)[\"%s\"] ", next->seconds, next->id, next->message);
    printf ("]\n");
#endif
    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */
    if ((current_alarm_1 == 0 || alarm->time < current_alarm_1) && display1_count <= display2_count && display1_count <= display2_count) {
        printf("\nAlarm Thread Created New Display Alarm Thread 1 For Alarm(%d at %ld: %s.", alarm->id,  time (NULL), alarm->message);
        current_alarm_1 = alarm->time;
        status = pthread_cond_signal (&alarm_cond1);
        display1_count++;
        if (status != 0)
            err_abort (status, "Signal cond");
    }
    else if ((current_alarm_2 == 0 || alarm->time < current_alarm_2) && display2_count <= display1_count && display2_count <= display3_count) {
        printf("\nAlarm Thread Created New Display Alarm Thread 2 For Alarm( %d at %ld: %s.", alarm->id,  time (NULL), alarm->message);
        current_alarm_2 = alarm->time;
        status = pthread_cond_signal (&alarm_cond2);
        display2_count++;
        if (status != 0)
            err_abort (status, "Signal cond");
    } 
    else if ((current_alarm_3 == 0 || alarm->time < current_alarm_3) && display3_count <= display1_count && display3_count <= display2_count) {
        printf("\nAlarm Thread Created New Display Alarm Thread 3 For Alarm( %d at %ld: %s.", alarm->id,  time (NULL), alarm->message);
        current_alarm_3 = alarm->time;
        status = pthread_cond_signal (&alarm_cond3);
        display3_count++;
        if (status != 0)
            err_abort (status, "Signal cond");
    } 
}

/*
 * Edit alarm entry on list, in order.
 */
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
    while (next != NULL) {
        printf("next: %d, alarm: %d\n", next->id, alarm->id);
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

void *display_1_thread (void *arg){
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;
    
    status = pthread_mutex_lock (&alarm_mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    while (1) {
        /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL) {
            status = pthread_cond_wait (&alarm_cond1, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list;
        alarm_list = alarm->link;
        now = time (NULL);
        expired = 0;
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            while (current_alarm == alarm->time) {
                status = pthread_cond_timedwait (
                    &alarm_cond1, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                if (status != 0)
                    err_abort (status, "Cond timedwait");
            }
            if (!expired)
                alarm_insert (alarm);
        } else
            expired = 1;
        if (expired) {
            display1_count--;
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            free (alarm);
        }
    }
}

void *display_2_thread (void *arg){
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;
    
    status = pthread_mutex_lock (&alarm_mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    while (1) {
        /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL) {
            status = pthread_cond_wait (&alarm_cond2, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list;
        alarm_list = alarm->link;
        now = time (NULL);
        expired = 0;
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            while (current_alarm == alarm->time) {
                status = pthread_cond_timedwait (
                    &alarm_cond2, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                if (status != 0)
                    err_abort (status, "Cond timedwait");
            }
            if (!expired)
                alarm_insert (alarm);
        } else
            expired = 1;
        if (expired) {
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            display2_count--;
            free (alarm);
        }
    }
}

void *display_3_thread (void *arg){
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;
    
    status = pthread_mutex_lock (&alarm_mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    while (1) {
        /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL) {
            status = pthread_cond_wait (&alarm_cond3, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list;
        alarm_list = alarm->link;
        now = time (NULL);
        expired = 0;
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            while (current_alarm == alarm->time) {
                status = pthread_cond_timedwait (
                    &alarm_cond3, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                if (status != 0)
                    err_abort (status, "Cond timedwait");
            }
            if (!expired)
                alarm_insert (alarm);
        } else
            expired = 1;
        if (expired) {
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            display3_count--;
            free (alarm);
        }
    }
}
/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    pthread_t thread1;
    int status1;
    pthread_t thread2;
    int status2;
    pthread_t thread3;
    int status3;
	int numthreads = 0;
	
	status1 = pthread_create (
        &thread1, NULL, display_1_thread, NULL);
	
    if (status1 != 0)
        err_abort (status1, "Create alarm thread");

    status2 = pthread_create (
        &thread2, NULL, display_2_thread, NULL);
    if (status2 != 0)
        err_abort (status2, "Create alarm thread");

    status3 = pthread_create (
        &thread3, NULL, display_3_thread, NULL);
    if (status3 != 0)
        err_abort (status3, "Create alarm thread");
	
	
	
	
    return NULL;
}

int main (int argc, char *argv[])
{
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


    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");

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
				/*Check if  is all numbers*/
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
