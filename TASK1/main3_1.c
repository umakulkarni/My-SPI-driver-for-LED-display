#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <inttypes.h>

#define DEVICE "/dev/spidev1.0" 

pthread_t polling_thread, display_thread;
pthread_mutex_t lock;
long distance = 1;
long distance_prev = 0;
int direction = 0;
int count1 = 0;
int count0 = 0;
long delay;
int fd_spi;
uint8_t array1[2];

uint8_t array [] = {	
		0x0F, 0x00,
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x06,
		0x02, 0x06,	
		0x03, 0x06,
		0x04, 0x83,
		0x05, 0x7F,
		0x06, 0x24,
		0x07, 0x22,
		0x08, 0x63,
};

uint8_t array_run [] = {
		0x0F, 0x00,	
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x06,
		0x02, 0x06,	
		0x03, 0x06,
		0x04, 0x03,
		0x05, 0x7F,
		0x06, 0xA2,
		0x07, 0x32,
		0x08, 0x16,
};
uint8_t arrayrev [] = {
		0x0F, 0x00,	
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x60,
		0x02, 0x60,	
		0x03, 0x60,
		0x04, 0xC1,
		0x05, 0xFE,
		0x06, 0x24,
		0x07, 0x44,
		0x08, 0xC6,
};

uint8_t arrayrev_run [] = {
		0x0F, 0x00,	
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
		0x01, 0x60,
		0x02, 0x60,	
		0x03, 0x60,
		0x04, 0xC0,
		0x05, 0xFE,
		0x06, 0x45,
		0x07, 0x4C,
		0x08, 0x68,
};


typedef unsigned long long ticks;
uint64_t tsc(void)                     // to convert 32 bit timestamp output into 64 bits
{
	uint32_t a, d;
	//asm("cpuid");
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return (( (uint64_t)a)|( (uint64_t)d)<<32 );
}


void setmux()
{
	int fd;
	fd = open("/sys/class/gpio/export", O_WRONLY);												// gpio14 mux
	write(fd, (void *) "30", sizeof("30"));
	close(fd);
	fd = open("/sys/class/gpio/gpio30/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);
	fd = open("/sys/class/gpio/gpio30/value", O_WRONLY);
	write(fd, "0", sizeof("0"));
	close(fd);
	fd = open("/sys/class/gpio/export", O_WRONLY);												// gpio15 mux
	write(fd, (void *) "31", sizeof("31"));
	close(fd);
	fd = open("/sys/class/gpio/gpio31/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);
	fd = open("/sys/class/gpio/gpio31/value", O_WRONLY);
	write(fd, "0", sizeof("0"));
	close(fd);

}


void sensor_set()
{

	int fd;
	fd = open("/sys/class/gpio/export", O_WRONLY);// gpio 14
	write(fd, (void *) "14", sizeof("14"));
	close(fd);

	fd = open("/sys/class/gpio/gpio14/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);

	fd = open("/sys/class/gpio/gpio14/value", O_WRONLY);
	write(fd, "0", sizeof("0"));
	usleep(50);
	close(fd);

	fd = open("/sys/class/gpio/export", O_WRONLY);  				 								//gpio 15
	write(fd, (void *) "15", sizeof("15"));
	close(fd);

	fd = open("/sys/class/gpio/gpio15/direction", O_WRONLY);
	write(fd, "out", sizeof("out"));
	close(fd);

	fd = open("/sys/class/gpio/gpio14/value", O_WRONLY);  											// gpio 14
	write(fd, "0", sizeof("0"));
	close(fd);
	usleep(10);
	
	fd = open("/sys/class/gpio/gpio15/direction", O_WRONLY);
	write(fd, "in", sizeof("in"));
	close(fd);

			
}

void displaymux()
{
	int fd;	
	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, (void *) "42", sizeof("42"));
	write(fd, (void *) "43", sizeof("43"));
	write(fd, (void *) "55", sizeof("55"));
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



void* polling_function(void* arg)
{
	int l;
	int poll_echo = 0;	
	int fd, fd1,fd2;
	int ret, rc;	
	char echo = '1';
	long timeout = 1000;
	long my_times, times;
	uint64_t ct2 = 0;
	uint64_t ct1 = 0;
	uint64_t cts = 0;
	double my_distance;
	struct pollfd fd_set[1];
	setmux();
	sensor_set();
		
	
	
	fd = open("/sys/class/gpio/gpio15/value", O_RDONLY);
	ret = read(fd, &poll_echo, sizeof(int));
	close(fd);
		
	while(1)
	{		
		fd1 = open("/sys/class/gpio/gpio14/value", O_WRONLY);  		 
		fd = open("/sys/class/gpio/gpio15/edge", O_WRONLY);
		fd2 = open("/sys/class/gpio/gpio15/value", O_RDONLY);
		write(fd, "rising", sizeof("rising"));

		
		fd_set[0].fd = fd2;
		fd_set[0].events = POLLPRI;
		
		write(fd1, "1", sizeof("1"));																	// trigger
		usleep(30);
		write(fd1, "0", sizeof("0"));
		
		
		ret=read(fd2,&poll_echo,sizeof(int));
		rc = poll(&fd_set[0], 1, timeout);   
		ct1 = tsc();																		//Rising edge GPIO interrupt occurred 
		if (fd_set[0].revents & POLLPRI) 
		{
			//printf("\npoll() GPIO interrupt occurred rising edge \n");						
			write(fd, "falling", sizeof("falling"));
			fd_set[0].revents = 0;
			read(fd2, &poll_echo , 1);			
			rc = poll(&fd_set[0], 1 , timeout);
			
			if(fd_set[0].revents & POLLPRI)
			{
				ct2 = tsc();																// Falling edge GPIO interrupt occurred 
				//printf("Falling edge occured \n");
			}				
			
	

		}
		close(fd);
		close(fd1);
		close(fd2);
		cts = ct2 - ct1;
		times = (long)cts;
		pthread_mutex_lock(&lock);
		distance_prev = distance;	
		if ( times < 80000 || times > 160000000 )												// to set sensor limits
			times = 80000;	
		my_times = times/400;
		distance = (my_times*34)/20;														// controlling distance is in cm * 100
		my_distance = (my_times*1.7)/100;													// to calculate distance in cm
		printf("Distance to the object is \t\t%lf cm\n\n", my_distance);

		// change direction only when constant decrease of increase in distance is detected

		if ((distance - distance_prev) > 50)			
		{
			count1 = count1 + 1;			
			if (count1 == 1)
			{	
				count0 = 0;
			}
			else if (count1 == 3)
			{
				direction = 1;
				count1 = 0;
			}
		
		}
		else if ((distance_prev - distance) > 50 )
		{
			count0 = count0 + 1;			
			if (count0 == 1)
			{	
				count1 = 0;
			}						
			else if (count0 == 3)
			{
				direction = 0;
				count0 = 0;
			}

		}
		
		pthread_mutex_unlock(&lock);
		usleep(502000);																	
		close(fd);
		
	}


}


void* display_function(void* arg)
{
	int j, k, l, i = 0;
	int ret;
	
	displaymux();
	struct spi_ioc_transfer tr = 
	{
		.tx_buf = (unsigned long)array1,
		.rx_buf = 0,
		.len = 2,
		.delay_usecs = 1,
		.speed_hz = 10000000,
		.bits_per_word = 8,
		.cs_change = 1,
	};
	fd_spi= open(DEVICE,O_WRONLY);
	if(fd_spi==-1)
	{
     	printf("file %s either does not exit or is currently used by an another user\n", DEVICE);
     	exit(-1);
	}
	printf ("\nUser has given Open command\n");
	
	while(1)
	{		
		i = 0;
		k= 0;
		if (distance < 800)													// Lower limit for delay and hence animation speed
			delay = 625000;
		else if (distance > 5950)											// Upper limit
			delay = 75000;
		else
		{
			delay = (446250000/distance);									// Linear mapping
		}

		// Check direction condition according to change in distance

		if (direction == 0)
		{
		while (i < 26)														// Switching between two display patterns
		{
			array1[0] = array [i];
			array1[1] = array [i+1];
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
			i = i + 2; 
		
		}
		usleep(delay);
		
		while (k < 26)
		{
			array1[0] = array_run [k];
			array1[1] = array_run [k+1];
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
			k = k + 2; 

		}
		usleep(delay);
	
		ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
		}
		else if (direction == 1)													

		{
		while (i < 26)
		{
			array1[0] = arrayrev [i];
			array1[1] = arrayrev [i+1];
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
			i = i + 2; 
		
		}
		usleep(delay);
		
		while (k < 26)
		{
			array1[0] = arrayrev_run [k];
			array1[1] = arrayrev_run [k+1];
			ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
			k = k + 2; 

		}
		usleep(delay);
	
		ret = ioctl(fd_spi, SPI_IOC_MESSAGE (1), &tr);
		}
			
		
	}
	close (fd_spi);
	printf("User has given close command\n");


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
	erc = pthread_create(&display_thread, NULL, &display_function, NULL);
	if (erc != 0)
	      printf("\ncan't create display thread\n");


	pthread_join (polling_thread, NULL);
	pthread_join (display_thread, NULL);
	pthread_mutex_destroy(&lock);

	return 0;
}
