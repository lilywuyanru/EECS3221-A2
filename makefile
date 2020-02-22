new_alarm_mutex : new_alarm_mutex.c
	cc new_alarm_mutex.c -D_POSIX_PTHREAD_SEMANTICS -lpthread
	./a.out