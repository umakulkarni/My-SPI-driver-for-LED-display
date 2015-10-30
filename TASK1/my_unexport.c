#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>


int main()
{
	int fd;
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	write(fd, (void *) "15", sizeof("15"));
	write(fd, (void *) "14", sizeof("14"));
	write(fd, (void *) "30", sizeof("30"));
	write(fd, (void *) "31", sizeof("31"));
	write(fd, (void *) "42", sizeof("42"));
	write(fd, (void *) "43", sizeof("43"));
	write(fd, (void *) "55", sizeof("55"));
	close(fd);
    printf("Unexport successfully completed\n");

	return 0;
}
