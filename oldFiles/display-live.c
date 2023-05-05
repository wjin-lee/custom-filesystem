/*
 * display-live.c
 *
 *  Modified on: 14/04/2023
 *      Author: Robert Sheehan
 *      Modified By: PK Wadsworth
 *
 *      This is used to display the blocks in the device.
 *      If run from the command line without a parameter it
 *      displays all blocks in the device.
 *      If run with an integer parameter it shows that many
 *      blocks from the device.
 *      It will also update live :)
 *
 *      Run `gcc -Wall -o display-live display-live.c device.c -lcurses` to
 *      compile and `./display-live` to run.
 */

#include "device.h"
#include "fileSystem.h"

#include <curses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_LINES_IN_DEFAULT_RENDERER 9 // Dont change this unless you change the `displayBlock_ncurses` function

#define NUM_LINES_IN_CUSTOM_RENDERER 2 // Update this to match at least the number of lines in your custom data renderer

// This function will run for every block that the terminal displays. It will run once every ~20 milliseconds.
// You can use the `wprintw(win, "Some string with %s number: %d", "formatting", 22)` function to print things
// to the window after the block. The previous example prints "Some string with formatting number 22" to the terminal
// window.
void custom_data_renderer(WINDOW *win, int blockNumber, block b) {
    wprintw(win, "YOUR CUSTOM DEBUG INFORMATION GOES HERE\n");
    wprintw(win, "The block number is %d and the first byte is 0x%x\n", blockNumber, b[0]);
    // Uses 2 `\n` characters so I set the NUM_LINES_IN_CUSTOM_RENDERER to 2
}

void displayBlock_ncurses(int blockNumber, WINDOW *win) {
    block b;
    if (blockRead(blockNumber, b)) {
        printDevError("-- displayBlock --");
    } else {
        int row;
        int col;
        const int number_per_row = 16;
        unsigned char *byte = b;

        wprintw(win, "Block %d\n", blockNumber);
        wprintw(win, "==========\n\n");

        for (row = 0; row < BLOCK_SIZE / number_per_row; ++row) {
            unsigned char *byte2 = byte;
            for (col = 0; col < number_per_row; ++col) {
                wprintw(win, "%02x ", *byte++);
            }
            wprintw(win, "\t");
            for (col = 0; col < number_per_row; ++col) {
                unsigned char c = *byte2++;
                if (c < 32 || c > 126)
                    c = '-';
                wprintw(win, "%c", c);
            }
            wprintw(win, "\n");
        }
        wprintw(win, "\n");

        custom_data_renderer(win, blockNumber, b);

        wprintw(win, "\n");
    }
}

typedef struct {
    WINDOW *pad;
    int *num_blocks_to_display;
    int *scroll_offset;
} render_block_thread_args;

void *render_block_thread_fn(void *args_in) {
    render_block_thread_args *args = (render_block_thread_args *)args_in;

    WINDOW *pad = args->pad;
    int *numBlocksToDisplay = args->num_blocks_to_display;
    int *scroll_offset = args->scroll_offset;

    int containing_x, containing_y;
    while (true) {
        getmaxyx(stdscr, containing_y, containing_x); // dynamically updates the display size to match the terminal
        werase(pad);                                  // Clear the window

        for (int i = 0; i < *numBlocksToDisplay; ++i) {
            displayBlock_ncurses(i, pad);
        }

        prefresh(pad, *scroll_offset, 0, 0, 0, containing_y - 1, containing_x - 1); // Refresh the pad
        usleep(20000);
    }
}

int main(int argc, char *argv[]) {
    int num_blocks_to_display;
    if (argc < 2) {
        num_blocks_to_display = numBlocks();
    } else {
        num_blocks_to_display = atoi(argv[1]);
        if (num_blocks_to_display < 1 || num_blocks_to_display > numBlocks())
            num_blocks_to_display = numBlocks();
    }

    initscr();
    timeout(0); // Non-blocking getch
    noecho();
    curs_set(0); // Hide cursor

    int containing_x;
    int containing_y;

    getmaxyx(stdscr, containing_y, containing_x);

    // Adding 10 for a margin of safety and bottom padding

    const int NUM_LINES_PER_BLOCK = NUM_LINES_IN_CUSTOM_RENDERER + NUM_LINES_IN_DEFAULT_RENDERER;

    WINDOW *pad = newpad(num_blocks_to_display * NUM_LINES_PER_BLOCK + 10, containing_x); // Create a new window with enough height
    keypad(stdscr, TRUE);

    int scroll_offset = 0;                                                            // Initialize the scroll offset
    int max_scroll = num_blocks_to_display * NUM_LINES_PER_BLOCK + 10 - containing_y; // Calculate the maximum scroll offset

    pthread_t render_block_thread_handle;
    render_block_thread_args render_block_thread_args;

    render_block_thread_args.num_blocks_to_display = &num_blocks_to_display;
    render_block_thread_args.scroll_offset = &scroll_offset;
    render_block_thread_args.pad = pad;

    pthread_create(&render_block_thread_handle, NULL, render_block_thread_fn, (void *)&render_block_thread_args);

    int ch;
    while ((ch = getch()) != 'q') { // Press 'q' to exit

        switch (ch) {
        case 259: // Scroll up
            scroll_offset = (scroll_offset <= 0) ? 0 : scroll_offset - 1;
            break;
        case 258: // Scroll down
            scroll_offset = (scroll_offset >= max_scroll) ? max_scroll : scroll_offset + 1;
            break;
        }

        usleep(1000); // sleep for 10 milliseconds to not got the cpu;
    }

    pthread_cancel(render_block_thread_handle);
    delwin(pad);
    endwin();

    return EXIT_SUCCESS;
}