/*
 * Columbia University
 * COMS W4118 Fall 2015
 * Extends Homework 3 starter code.
 *
 */
#include <bionic/errno.h> /* Google does things a little different...*/
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h>
#include <errno.h>
#include <sys/stat.h>
#include "../flo-kernel/include/linux/akm8975.h" 
#include "light.h"

#define EMULATOR 					0
#define DEVICE 						1

#define LIGHT_INTENSITY_SENSOR 		5

#define TIME_INTERVAL 				2000000
#define MAX_LI						3276800

#define __NR_set_light_intensity	378
#define __NR_get_light_intensity	379

/* Unused: */
/* Set to 1 for a bit of debug output */
#if 1
	#define dbg(fmt, ...) printf("LIGHT_D: " fmt, ## __VA_ARGS__)
#else
	#define dbg(fmt, ...)
#endif


static int effective_sensor;
static int cur_device;

/* helper functions */
static int open_sensors(struct sensors_module_t **hw_module,
			struct sensors_poll_device_t **poll_device);
static void enumerate_sensors(const struct sensors_module_t *sensors);
static int poll_sensor_data_emulator(void);
static int poll_sensor_data(struct sensors_poll_device_t *sensors_device);

void daemon_mode()
{
	pid_t fork_temp, fork_daemon, daemon_pid;

	printf("Starting new daemon process.\n");
	printf("Make main's child (semi-daemon) fork the daemon because\n");

	/* One source amongst many others */
	printf("http://www.dsm.fordham.edu/cgi-bin/man-cgi.pl?" \
		"topic=DAEMON&ampsect=7\n");

	printf("And also because we lose nothing if we don't.\n");

	printf("Forking main child.\n");
	fork_temp = fork();

	if (fork_temp == -1)
		printf("Error forking child: %s\n", strerror(errno));

	if (fork_temp > 0) {
		printf("Main (parent process) exiting.\n");
		exit(EXIT_SUCCESS);
	}

	printf("Detach from terminal and create independent session\n");
	printf("with child as leader.\n");
	if (setsid() == -1)
		printf("Error setting session ID: %s\n", strerror(errno));

	printf("Forking daemon process.\n");
	fork_daemon = fork();

	if (fork_temp == -1)
		printf("Error forking daemon: %s\n", strerror(errno));

	if (fork_daemon > 0) {
		printf("Child exiting. Daemon continues main execution.\n");
		exit(EXIT_SUCCESS);
	}

	daemon_pid = getpid();
	printf("We got the daemon's pid: %i.\n", daemon_pid);
	printf("Check by doing command line $(ps).\n");

	printf("Changing directory to root.\n");
	if (chdir("/") == -1)
		printf("Error changing current directory to root: %s\n",
			strerror(errno));

	printf("Disabling file operations by resetting umask to 0.\n");
	umask(0);

	printf("Closing file descriptors.\n");
	if (close(0) == -1) {
		printf("Error closing stdin: %s\n", strerror(errno));
	}
	if (close(2) == -1) {
		printf("Debug closing stderr: %s\n", strerror(errno));
	}
	if (close(1) == -1) {
		printf("Error closing stdout: %s\n", strerror(errno));
	}
}

/* Wrappers to syscalls. */
int get(struct light_intensity* user_light_intensity)
{
	return syscall(__NR_get_light_intensity, user_light_intensity);
}

int set(struct light_intensity* user_light_intensity)
{
	return syscall(__NR_set_light_intensity, user_light_intensity);
}

int main(int argc, char **argv)
{
	struct light_intensity val;
	int retval;

	effective_sensor = -1;
	cur_device = -1;
	if (argc != 2) {
		printf("Invalid arguments - use ./light_d [-e] [-d]\n");
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "-e") == 0)
		cur_device = EMULATOR;
	else if (strcmp(argv[1], "-d") == 0)
		cur_device = DEVICE;
	else {
		printf ("Invalid arguments - use ./light_d [-e] [-d]\n");
		return EXIT_FAILURE;
	}

	struct sensors_module_t *sensors_module = NULL;
	struct sensors_poll_device_t *sensors_device = NULL;
	
	daemon_mode();

	printf("Opening sensors...\n"); /* Won't print? */
	if (open_sensors(&sensors_module,
			 &sensors_device) < 0) {
		printf("open_sensors failed\n");
		return EXIT_FAILURE;
	}
	enumerate_sensors(sensors_module);

	while (1) { /* Success */
		usleep(TIME_INTERVAL);

		val.cur_intensity = poll_sensor_data(sensors_device);
		retval = set(&val);

		if (retval) /* Kill unsuccessful daemon */
			return EXIT_FAILURE;

		val.cur_intensity = MAX_LI + 1;
		retval = get(&val);

		if (retval <= 0 || retval >= MAX_LI) /* Again */
			return EXIT_FAILURE;
	}
}

/*
 *You should send the intensity value to the kernel as soon as you read it
 * from the function below
 */

static int poll_sensor_data(struct sensors_poll_device_t *sensors_device)
{   
	const size_t numEventMax = 16;
	const size_t minBufferSize = numEventMax;
	sensors_event_t buffer[minBufferSize];
	ssize_t count = sensors_device->poll(sensors_device, buffer, minBufferSize);
	float cur_intensity = 0;

	int i;
	if (cur_device == DEVICE) {
		
		for (i = 0; i < count; ++i) {
					if (buffer[i].sensor != effective_sensor)
							continue;
				
		/* You have the intensity here - scale it and send it to your kernel */
				
		cur_intensity = buffer[i].light;
		printf("%f\n", cur_intensity);	
		}
	}

	else if (cur_device == EMULATOR) {

		/* Same thing again here - pretty bad hack for the emulator */
		/* Didn't know that the sensor simulator had only temperature but not light */
		/* cur_intensity has a floating point value that you would have fed to */
		/* light_sensor binary */
		cur_intensity = poll_sensor_data_emulator();
		printf("%f\n", cur_intensity);
	}

	cur_intensity *= 100;
	return (int)cur_intensity; /* or (int)(cur_intensity + 0.5) */
}



/*  DO NOT MODIFY BELOW THIS LINE  */
/*---------------------------------*/



static int poll_sensor_data_emulator(void)
{
	float cur_intensity;
		
		FILE *fp = fopen("/data/misc/intensity", "r");
		if (!fp)
				return 0;       
		
		fscanf(fp, "%f", &cur_intensity);
		fclose(fp);
		return cur_intensity;
}




static int open_sensors(struct sensors_module_t **mSensorModule,
			struct sensors_poll_device_t **mSensorDevice)
{
   
	int err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
					 (hw_module_t const**)mSensorModule);

	if (err) {
		printf("couldn't load %s module (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorModule)
		return -1;

	err = sensors_open(&((*mSensorModule)->common), mSensorDevice);

	if (err) {
		printf("couldn't open device for module %s (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorDevice)
		return -1;

	const struct sensor_t *list;
	ssize_t count = (*mSensorModule)->get_sensors_list(*mSensorModule, &list);
	size_t i;
	for (i=0 ; i<(size_t)count ; i++)
		(*mSensorDevice)->activate(*mSensorDevice, list[i].handle, 1);

	return 0;
}

static void enumerate_sensors(const struct sensors_module_t *sensors)
{
	int nr, s;
	const struct sensor_t *slist = NULL;
	if (!sensors)
		printf("going to fail\n");

	nr = sensors->get_sensors_list((struct sensors_module_t *)sensors,
					&slist);
	if (nr < 1 || slist == NULL) {
		printf("no sensors!\n");
		return;
	}

	for (s = 0; s < nr; s++) {
		printf("%s (%s) v%d\n\tHandle:%d, type:%d, max:%0.2f, "
			"resolution:%0.2f \n", slist[s].name, slist[s].vendor,
			slist[s].version, slist[s].handle, slist[s].type,
			slist[s].maxRange, slist[s].resolution);

		/* Awful hack to make it work on emulator */
		if (slist[s].type == LIGHT_INTENSITY_SENSOR && slist[s].handle == LIGHT_INTENSITY_SENSOR)
			effective_sensor = LIGHT_INTENSITY_SENSOR; /*the sensor ID*/

				}
}
