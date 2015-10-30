#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include "ioctl_basic.h" 

#define DEVICE_DISPLAY "/dev/spidev1.0"
#define DEVICE_SENSOR "/dev/gmem"

pthread_t polling_thread, display_thread;
pthread_mutex_t lock;
pthread_mutex_t lock;

int exit_flag = 0;
long my_distance;
uint8_t array_overwrite[1][2];
uint8_t array_original [10][26] = {
		0x0F, 0x00,		// 0	
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x00,
		0x02, 0x80,	
		0x03, 0xE0,
		0x04, 0x00,
		0x05, 0x00,
		0x06, 0x00,
		0x07, 0x00,
		0x08, 0x00,

		0x0F, 0x00,		// 1
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x00,
		0x02, 0x0C,	
		0x03, 0x0C,
		0x04, 0x00,
		0x05, 0x00,
		0x06, 0x00,
		0x07, 0x80,
		0x08, 0xE0,

		0x0F, 0x00,		// 2
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x00,
		0x02, 0x00,	
		0x03, 0x00,
		0x04, 0x00,
		0x05, 0x00,
		0x06, 0x00,
		0x07, 0x8C,
		0x08, 0xEC,

		0x0F, 0x00,		// 3
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x00,
		0x02, 0x03,	
		0x03, 0x03,
		0x04, 0x00,
		0x05, 0x00,
		0x06, 0x00,
		0x07, 0x8C,
		0x08, 0xEC,

		0x0F, 0x00,			// 4
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x00,
		0x02, 0x70,	
		0x03, 0x10,
		0x04, 0x00,
		0x05, 0x00,
		0x06, 0x00,
		0x07, 0x8F,
		0x08, 0xEF,

		0x0F, 0x00,			// 5
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x00,
		0x02, 0x00,	
		0x03, 0x00,
		0x04, 0x00,
		0x05, 0x00,
		0x06, 0x00,
		0x07, 0xFF,
		0x08, 0xFF,
		
		0x0F, 0x00,			// 6
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x00,
		0x02, 0x00,	
		0x03, 0x00,
		0x04, 0x00,
		0x05, 0x00,
		0x06, 0x00,
		0x07, 0x00,
		0x08, 0x00,

		0x0F, 0x00,			// 7 MAN
		0x0C, 0x01,	
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x7E,
		0x02, 0xA5,	
		0x03, 0xA5,
		0x04, 0x7E,
		0x05, 0x18,
		0x06, 0x5A,
		0x07, 0x18,
		0x08, 0x24,
		
		0x0F, 0x00,			// 8
		0x0C, 0x01,	
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x7E,
		0x02, 0xA5,	
		0x03, 0xA5,
		0x04, 0x7E,
		0x05, 0x18,
		0x06, 0x18,
		0x07, 0xA5,
		0x08, 0x42,

		0x0F, 0x00,			// 9 SMILEY
		0x0C, 0x01,	
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x3C,
		0x02, 0x42,	
		0x03, 0xA5,
		0x04, 0x81,
		0x05, 0xA5,
		0x06, 0x99,
		0x07, 0x42,
		0x08, 0x3C,
};


/* to configure mux*/

void setmux()
{
	int fd;	
	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, (void *) "4", sizeof("4"));
	write(fd, (void *) "42", sizeof("42"));
	write(fd, (void *) "43", sizeof("43"));
	write(fd, (void *) "55", sizeof("55"));
	close(fd);

	fd = open("/sys/class/gpio/gpio4/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);

	fd = open("/sys/class/gpio/gpio4/value", O_WRONLY);
	write(fd, "1", sizeof("1"));
	close(fd);


	fd = open("/sys/class/gpio/gpio42/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);

	fd = open("/sys/class/gpio/gpio42/value", O_WRONLY);
	write(fd, "0", sizeof("1"));
	close(fd);


	fd = open("/sys/class/gpio/gpio43/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);

	fd = open("/sys/class/gpio/gpio43/value", O_WRONLY);
	write(fd, "0", sizeof("1"));
	close(fd);


	fd = open("/sys/class/gpio/gpio55/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);

	fd = open("/sys/class/gpio/gpio55/value", O_WRONLY);
	write(fd, "0", sizeof("1"));
	close(fd);

}

void* display_function(void* arg)
{
	
	int ret, i;
	int fd_spi;
	int count = 0;
	int pattern_cnt1 = 0;
	int pattern_cnt2 = 0;
	int pattern_cnt3 = 0;
	int pattern_cnt4 = 0;
	int display_sequence1[] = {7, 100, 8, 100, 0, 0};				//predefined user sequences
	int display_sequence2[] = {0, 200, 1, 200, 2, 200, 3, 200, 4, 200, 5, 200, 6, 200, 0, 0};
	int display_sequence3[] = {9, 400, 6, 400, 0, 0};
	int display_sequence4[] = {0, 0};
	int display_value = -1;

	setmux();
	fd_spi = open(DEVICE_DISPLAY, O_RDWR);
	if(fd_spi == -1)
	{
    	 	printf("file %s either does not exit or is currently used by an another user\n", DEVICE_DISPLAY);
     		exit(-1);
	}
	printf ("\nUser has given Open command\n");
	ret = ioctl(fd_spi,IOCTL_PATTERN, array_original);
	printf("IOCTL return value is %d\n", ret);
	while (1)
	{
	if (my_distance < 1700 && display_value != 0)		
		{		
			pattern_cnt1++;
			if(pattern_cnt1 == 3)
			{		
			ret = write(fd_spi, display_sequence1, sizeof(display_sequence1));
			display_value = 0;
			pattern_cnt1 = 0;
			}
			else
			{
			pattern_cnt2 = 0;
			pattern_cnt3 = 0;
			pattern_cnt4 = 0;
			}
			
		}
	else if (1800 < my_distance && my_distance < 4500 && display_value != 1)
		{

			pattern_cnt2++;
			if(pattern_cnt2 == 3)
			{			
			ret = write(fd_spi, display_sequence2, sizeof(display_sequence2));
			display_value = 1;
			pattern_cnt2 = 0;
			}
			else
			{
			pattern_cnt1 = 0;
			pattern_cnt3 = 0;
			pattern_cnt4 = 0;
			}

		}
	else if (4600 < my_distance && my_distance < 6300 && display_value != 2)
		{
			pattern_cnt3++;
			if(pattern_cnt3 == 3)
			{
			ret = write(fd_spi, display_sequence3, sizeof(display_sequence3));
			display_value = 2;
			pattern_cnt3 = 0;
			}
			else
			{
			pattern_cnt1 = 0;
			pattern_cnt2 = 0;
			pattern_cnt4 = 0;
			}

		}
	else if (my_distance > 6500)
		{
			pattern_cnt4++;
			if (pattern_cnt4 == 3)
			{
				ret = write(fd_spi, display_sequence4, sizeof(display_sequence4));
				exit_flag = 1;
				pattern_cnt4 = 0;
				goto stp;
			}
			else 
			{
			pattern_cnt1 = 0;
			pattern_cnt2 = 0;
			pattern_cnt3 = 0;
			}
			
		}
	usleep(100000);
	}	
stp: 
	usleep(100000);
	close(fd_spi);	
	return 0;


}



void* polling_function(void* arg)
{

	int fd_sensor;
	int ret, i;
	char my_string[] = "Uma";
	long timing;
	int count_flag = 0;
	double dist;
	
	fd_sensor = open(DEVICE_SENSOR, O_RDWR);
	if(fd_sensor == -1)
	{
    	 	printf("file %s either does not exit or is currently used by an another user\n", DEVICE_SENSOR);
     		exit(-1);
	}
	printf ("\nUser has given Open command\n");
	while(1)
	{
		if (exit_flag == 0)													// to check exit command
		{
			ret = write(fd_sensor, my_string, strlen(my_string));
			if(ret == -1)
			{
				printf("write failed\n");
					
			}
			pthread_mutex_lock(&lock);
			ret = read(fd_sensor, &timing, 8);
			if(ret == -1)
			{
				printf("read failed or work is in process\n");
					
			}
			else
			{
				if (count_flag == 1)
				{
					my_distance = (timing*340)/80000;
					dist = (timing*4.25)/100000;
					printf("Object is at distance is \t\t%lf cm\n\n", dist);
				}
				count_flag = 1;
			}
			pthread_mutex_unlock(&lock);
			usleep(50);
		}
		else 
		{
			printf(" Termination command is given to stop the application\n");
			break;
		}
	}
	close(fd_sensor);
}

int main()
{

	int err, erc;
	if (pthread_mutex_init(&lock, NULL) != 0) 
	{
	    printf("\n mutex init failed\n");
	    return 1;
	}

	err = pthread_create(&polling_thread, NULL, &polling_function, NULL);
	if (err != 0)
	      printf("\ncan't create polling thread\n");
	usleep (2000);
	erc = pthread_create(&display_thread, NULL, &display_function, NULL);
	if (erc != 0)
	      printf("\ncan't create display thread\n");

	pthread_join (polling_thread, NULL);
	pthread_join (display_thread, NULL);
	pthread_mutex_destroy(&lock);

	return 0;
}
