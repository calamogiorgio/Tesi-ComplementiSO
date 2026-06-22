#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 256

int init_serial() {
    int ret;
    
    /* Open the serial port device file in Read/Write mode */
    int fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening serial port");
        return -1;
    }

    struct termios tty;
    
    /* Get current serial port attributes to avoid overwriting defaults */
    ret = tcgetattr(fd, &tty);
    if (ret == -1) {
        perror("Error getting attributes (tcgetattr)");
        return -1;
    }

    /* Set baud rate to 19200 bps for both input and output */
    cfsetispeed(&tty, B19200);
    cfsetospeed(&tty, B19200);

    /* Local flags: Disable canonical mode, echo, and signal chars */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    
    /* Control flags: Set data size to 8 bits, no parity, 1 stop bit (8N1) */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    
    /* Enable receiver and ignore modem control lines */
    tty.c_cflag |= (CLOCAL | CREAD);
    
    /* Special control characters: Set non-blocking read */
    tty.c_cc[VMIN]  = 0; /* Return immediately with whatever data is available */
    tty.c_cc[VTIME] = 0; /* No timer timeout */

    /* Apply configurations immediately */
    ret = tcsetattr(fd, TCSANOW, &tty);
    if (ret == -1) {
        perror("Error setting attributes (tcsetattr)");
        return -1;
    }

    return fd;
}

int main() {
    /* Initialize the serial port and get the file descriptor */
    int fd = init_serial();
    if (fd == -1) {
        exit(EXIT_FAILURE);
    }

    /* Accumulator buffer for string reconstruction */
    char line_buffer[BUFFER_SIZE];
    int line_index = 0;

    printf("Listening for Arduino data (with stream reconstruction)...\n");

    while (1) {
        char ch;
        /* Read EXACTLY 1 byte at a time to prevent stream fragmentation issues */
        int ret = read(fd, &ch, 1);

        if (ret == -1) {
            if (errno == EINTR) {
                /* Interrupted by a system signal, try again */
                continue;
            }
            /* A real read error occurred */
            perror("Error reading from serial port");
            break;
        } 
        else if (ret == 0) {
            /* No data available right now, sleep 5ms to save CPU */
            usleep(5000);
            continue;
        }

        /* Character received: store it if there is space available */
        if (line_index < BUFFER_SIZE - 1) {
            line_buffer[line_index++] = ch;
        }

        /* Process the accumulated string only when a newline character is found */
        if (ch == '\n') {
            line_buffer[line_index] = '\0'; /* Securely terminate the string */

            /* 1. Strip trailing \r and \n */
            while (line_index > 0 && (line_buffer[line_index - 1] == '\n' || line_buffer[line_index - 1] == '\r')) {
                line_buffer[--line_index] = '\0';
            }

            /* 2. CRUCIAL: If the line is now empty, SKIP IT! */
            if (line_index == 0) {
                line_index = 0; /* Reset and continue to the next character */
                continue;
            }

            /* 3. Now compare safely against the idle token "i" (without \n) */
            if (strcmp(line_buffer, "i") != 0) {
                printf("Received data: %s\n", line_buffer);
                fflush(stdout);
            }

            /* Reset the index to accumulate the next incoming message */
            line_index = 0;
        }
    }

    

    /* Clean up and close the file descriptor before exiting */
    close(fd);
    return 0;
}