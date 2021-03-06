#include <pigpio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "MLD019.h"
#include <assert.h>
#include <ncurses.h>
#include <sys/time.h>
#include <time.h>

#define SERIAL_TERM "/dev/ttyAMA0" //target PL011 UART module
#define BAUD 9600

#define ENABLE_PIN 6 //physical pin 31
#define PULSE_PIN 26 //physical pin 37
#define SHUTTER_PIN 5 //physical pin 29

#define SAMPLE_PERIOD_SEC 1
#define LASER_FREQ 10000

typedef enum states {
    WAIT,
    WARMUP,
    LASE,
    COOL,
    QUIT
} states_t;

char state_str[4][8] = {"WAIT","WARMUP", "LASE", "COOL"};

void getTimestampedFilePath(char* str_result, size_t str_size) 
{	/**
	Returns to [str_result] the current time (UTC) as a string of format
	YYYY-mm-dd-HH-MM-SS-UUUUUU where mm is the month number and UUUUUU are the microseconds of the epoch time
	**/

	struct timeval curr_time_tv;
		
	//get current time; tv provides sec and usec; tm used for strftime
	gettimeofday(&curr_time_tv, NULL); //get current time in timeval
	struct tm *curr_time_tm = localtime(&(curr_time_tv.tv_sec)); //use sec of timeval to get struct tm

	//use tm to get timestamp string
	strftime(str_result, str_size, "./%Y_%m_%d_%H_%M_%S_%Z_thermal_laser_test.csv",curr_time_tm);	
}

//return seconds from the epoch
double getEpochTime()
{
    struct timeval now;
    gettimeofday(&now,NULL);
    return (double) now.tv_sec + (now.tv_usec / 1000000.0);
} //end getEpochTime()

int main(int argc, char** argv) 
{
    if (argc != 4)
    {
        printf("ARGUMENT ERROR\n");
        printf("\tUsage: sudo %s [warm-up seconds] [lasing seconds] [cool down seconds]\n",argv[0]);
        return -1;
    }

    int warmup_sec = atoi(argv[1]);
    int lasing_sec = atoi(argv[2]);
    int cool_sec = atoi(argv[3]);

    gpioCfgClock(2,1,0); //2 us sample rate using PCM (1) clock
    gpioInitialise();
    gpioSetMode(ENABLE_PIN, PI_OUTPUT);
    gpioSetMode(SHUTTER_PIN, PI_OUTPUT);
    gpioWrite(ENABLE_PIN, 0); //write low to prevent emission
    gpioWrite(SHUTTER_PIN, 1); //write HI for quick switch to lasing
    gpioDelay(500000); //delay 500 ms to allow LED diode to warm up after shutter off

    //terminal manipulations for non-blocking getch()
	initscr();
	nodelay(stdscr,true);
	noecho();

    mld_t mld;
    mld.serial_handle = serOpen(SERIAL_TERM, BAUD, 0);

    //verify driver comms
    if (mldLinkControl(mld) != 0) {
        //terminate if error communicating with driver
        printf("ERROR with driver communications\n");
        return -1;
    }

    char c;
    states_t test_state = WARMUP;
    do
    {
        printf("Enter \'S\' to start...\n\r");
        printf("Enter \'q\' to quit...\n\r");
        scanf("%c", &c);
    } while (c != 'S' && c != 'q');
    if (c == 'q') return 0;

    char filepath[200];
    getTimestampedFilePath(filepath,200);

    FILE* f = fopen(filepath, "a");
    fprintf(f,"WARMUP SECONDS,%d\nLASE SECONDS,%d\nCOOL SECONDS,%d\n\n",
            warmup_sec, lasing_sec, cool_sec);
    fprintf(f, "MSEC_ELAPSED, STATE, BOARD_TEMP (C), LASER_TEMP (C)\n");



    int warmup_end_time = warmup_sec;
    int lase_end_time = warmup_end_time + lasing_sec;
    int cool_end_time = lase_end_time + cool_sec;
    double curr_time;
    double start_time = getEpochTime();
    while (getch() != 'q')
    {
        printf("test_state=%s\t",state_str[test_state]);
        curr_time = getEpochTime() - start_time; 
        if (curr_time < warmup_end_time) 
        {
            test_state = WARMUP;
            printf("Seconds until LASE=%f\n\r",warmup_end_time - curr_time);
        }
        else if (warmup_end_time < curr_time && curr_time < lase_end_time)
        {
            printf("Seconds until COOL=%f\n\r",lase_end_time - curr_time);

            if (test_state != LASE)
            {
                //configure pins on entrance into LASE state/exit from WARMUP state
                gpioSetMode(PULSE_PIN, PI_OUTPUT);
                gpioWrite(PULSE_PIN, 0);
                gpioWrite(ENABLE_PIN,1);
                gpioDelay(1000);
                gpioSetPWMfrequency(PULSE_PIN,LASER_FREQ);
                gpioPWM(PULSE_PIN,255/2);
                test_state = LASE;
            }
        }
        else if (lase_end_time < curr_time && curr_time < cool_end_time)
        {
            if (test_state != COOL)
            {
                //On exit of LASE/entrance to COOL state, disable lasing
                gpioPWM(PULSE_PIN,0);
                gpioWrite(ENABLE_PIN,0);
                test_state = COOL;
            }
        }

        float board_temp = mldBoardTemp(mld);
        float laser_temp = mldCaseTemp(mld);
        printf("board_temp=%.2f\tlaser_temp=%.2f\n\r", board_temp, laser_temp);

        fprintf(f, "%.2f,%s,%.2f,%.2f\n", curr_time*1000.0, state_str[test_state],board_temp,laser_temp);

        while ((getEpochTime() - start_time) < (SAMPLE_PERIOD_SEC + curr_time)) {}
    }

    gpioPWM(PULSE_PIN,0);
    gpioWrite(ENABLE_PIN,0);
    serClose(mld.serial_handle);
    gpioTerminate();
    fclose(f);
    echo();
	endwin(); //restore terminal configuration
    return 0;
}