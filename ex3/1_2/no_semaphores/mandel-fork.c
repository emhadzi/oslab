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

#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

/***************************
 * Compile-time parameters *
 ***************************/

/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
const int y_chars = 50;
const int x_chars = 90;

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
const double xmin = -1.8, xmax = 1.0;
const double ymin = -1.0, ymax = 1.0;

/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep = (xmax - xmin) / x_chars;
double ystep = (ymax - ymin) / y_chars;

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

void compute_and_output_mandel_line(int fd, int line)
{
	/*
	 * A temporary array, used to hold color values for the line being drawn
	 */
	int color_val[x_chars];

	compute_mandel_line(line, color_val);
	output_mandel_line(fd, color_val);
}

/*
 * Create a shared memory area, usable by all descendants of the calling
 * process.
 */
void *create_shared_memory_area(unsigned int numbytes)
{
	int pages;
	void *addr;

	if (numbytes == 0) {
		fprintf(stderr, "%s: internal error: called for numbytes == 0\n", __func__);
		exit(1);
	}

	/*
	 * Determine the number of pages needed, round up the requested number of
	 * pages
	 */
	pages = (numbytes - 1) / sysconf(_SC_PAGE_SIZE) + 1;

	/* Create a shared, anonymous mapping for this number of pages */
	addr = mmap(NULL, pages * sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	return addr;
}

void destroy_shared_memory_area(void *addr, unsigned int numbytes) {
	int pages;

	if (numbytes == 0) {
		fprintf(stderr, "%s: internal error: called for numbytes == 0\n", __func__);
		exit(1);
	}

	/*
	 * Determine the number of pages needed, round up the requested number of
	 * pages
	 */
	pages = (numbytes - 1) / sysconf(_SC_PAGE_SIZE) + 1;

	if (munmap(addr, pages * sysconf(_SC_PAGE_SIZE)) == -1) {
		perror("destroy_shared_memory_area: munmap failed");
		exit(1);
	}
}

// USER CODE

int NPROCS;
int *shared_buf;
size_t shared_buf_size;

void handle_sigint(int sig)
{
	reset_xterm_color(1);
	exit(EXIT_SUCCESS);
}

void setup_sigint_handler()
{
	struct sigaction sa;
	sa.sa_handler = handle_sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}

// Child process part of code
void child(int id){
    for(int line=id; line<y_chars; line+=NPROCS){
	    compute_mandel_line(line, &shared_buf[line*x_chars]);
    }
    return;
}


int main(int argc, char *argv[])
{
    // Setup singal handler
    setup_sigint_handler();

    // Check arguments
    if(argc !=2) {
        perror("NPROCS not entered\n");
        exit(EXIT_FAILURE);
    }
    NPROCS = atoi(argv[1]);
    if(NPROCS == 0){
        perror("Invalid NPROCS\n");
        exit(EXIT_FAILURE);
    }

    // Create shared buffer
    shared_buf_size = x_chars * y_chars * sizeof(int);
    shared_buf = create_shared_memory_area(shared_buf_size);

    // Create NPROCS processes
    int pids[NPROCS];
    for(int i = 0; i<NPROCS; ++i){
        int p = fork();
        if(p == -1){
            perror("Fork failed\n");
            exit(EXIT_FAILURE);
        } else if (p == 0) {
            child(i);
            destroy_shared_memory_area(shared_buf, shared_buf_size);
            exit(EXIT_SUCCESS);
        }
        pids[i]=p;
    }

    // Wait for all children to finish
    int status;
    for(int i=0; i<NPROCS; ++i)
        waitpid(pids[i], &status, 0);

    // Print result
	for(int i=0; i<y_chars; ++i){
          output_mandel_line(1, &shared_buf[x_chars * i]);
	}

    // clean up
    destroy_shared_memory_area(shared_buf, shared_buf_size);
	reset_xterm_color(1);

	return 0;
}
