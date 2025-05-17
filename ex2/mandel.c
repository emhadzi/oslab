/*
 * mandel.c
 *
 * A program to draw the Mandelbrot Set on a 256-color xterm.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#include "mandel-lib.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define MANDEL_MAX_ITERATION 100000
#define MAXN 1000  // Maximum number of calculation threads
#define M 1000  // Size of buffer in lines
/***************************
 * Compile-time parameters *
 ***************************/

/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
int y_chars = 400;
int x_chars = 700;
int n;			// Number of threads
int **buff;		// Output buffer

sem_t *can_calc;
sem_t *can_print; // To support multiple printers in the future

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;
	
/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep;
double ystep;

/*
 * This function computes a line of output
 * as an array of x_char color values.
 */
void compute_mandel_line(int line, int color_val[])
{
	/*
	 * x and y traverse the complex plane.
	 */
	double x, y;

	int n;
	int val;

	/* Find out the y value corresponding to this line */
	y = ymax - ystep * line;

	/* and iterate for all points on this line */
	for (x = xmin, n = 0; n < x_chars; x+= xstep, n++) {

		/* Compute the point's color value */
		val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
		if (val > 255)
			val = 255;

		/* And store it in the color_val[] array */
		val = xterm_color(val);
		color_val[n] = val;
	}
}

/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */
void output_mandel_line(int fd, int color_val[])
{
	int i;
	
	char point ='@';
	char newline='\n';
	// We assume x is long enough to be an efficient buffer eg 1KB long
	// Otherwise, printf would be more efficient
	for (i = 0; i < x_chars; i++) {
		/* Set the current color, then output the point */
		set_xterm_color(fd, color_val[i]);
		if (write(fd, &point, 1) != 1) {
			perror("compute_and_output_mandel_line: write point");
			exit(1);
		}
	}

	/* Now that the line is done, output a newline character */
	if (write(fd, &newline, 1) != 1) {
		perror("compute_and_output_mandel_line: write newline");
		exit(1);
	}
}

void *calc_lines(void *arg)
{
	// printf("Thread %ld started\n", pthread_self());
	// fflush(stdout);
	// line is thread local so we are ok
	for(int line = *(int *)arg; line < y_chars; line += n){
		// Wait until i can write to buffer
		sem_wait(&can_calc[line % M]);
		// printf("Thread %ld calculating\n", pthread_self());
		// fflush(stdout);
		compute_mandel_line(line, buff[line % M]);
		sem_post(&can_print[line % M]);
	}
	// printf("Thread %ld done\n", pthread_self());
	// fflush(stdout);
	return NULL;
}

void *output_buffer(void *arg)
{
	printf("Output thread started\n");
	fflush(stdout);
	for (int lines_done = 0; lines_done < y_chars; lines_done++){
		sem_wait(&can_print[lines_done % M]);
		// printf("Output thread\n");
		// fflush(stdout);
		output_mandel_line(1, buff[lines_done % M]);
		sem_post(&can_calc[lines_done % M]);
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t t[MAXN];
	pthread_t tout;
	buff = malloc(min(M, y_chars) * sizeof(int *));
	for (int i = 0; i < min(M, y_chars); i++){
		buff[i] = malloc(x_chars * sizeof(int));
	}
	
	can_calc = malloc(min(M, y_chars) * sizeof(sem_t));
	can_print = malloc(min(M, y_chars) * sizeof(sem_t));
	
	for (int i = 0; i < min(M, y_chars); i++){
		sem_init(&can_calc[i], 0, 1);
		sem_init(&can_print[i], 0, 0);
	}

	n = atoi(argv[1]);

	xstep = (xmax - xmin) / x_chars;
	ystep = (ymax - ymin) / y_chars;

	/*  
	* Have one output thread
	* And n calc threads
	* Have a buffer of size m of precalculated lines
	*/
	int ret = pthread_create(&tout, NULL, output_buffer, NULL);
	if (ret) {
		perror("pthread_create");
		exit(1);
	}

	for (int i = 0; i < n; i++){	
		int *arg = malloc(sizeof(int));
		*arg = i;
		int ret = pthread_create(&t[i], NULL, calc_lines, arg);
		if (ret) {
			perror("pthread_create");
			exit(1);
		}
	}

	for (int i = 0; i < n; i++)
		pthread_join(t[i], NULL);

	pthread_join(tout, NULL);

	reset_xterm_color(1);
	return 0;
}
