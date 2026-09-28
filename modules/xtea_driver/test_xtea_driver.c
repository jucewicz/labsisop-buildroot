#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 1024

int main(void)
{
    int ret;
    int fd;

    char receive[BUFFER_LENGTH];
    char command[BUFFER_LENGTH];


    printf("Starting XTEA driver test...\n");


    fd = open("/dev/xtea_driver", O_RDWR);

    if (fd < 0) {
        perror("Failed to open /dev/xtea_driver");
        return errno;
    }


    printf("Enter command:\n");

    if (fgets(command, sizeof(command), stdin) == NULL) {
        close(fd);
        return 1;
    }


    command[strcspn(command, "\n")] = '\0';


    ret = write(fd, command, strlen(command));

    if (ret < 0) {
        perror("Failed to write to device");
        close(fd);
        return errno;
    }


    ret = read(fd, receive, sizeof(receive) - 1);

    if (ret < 0) {
        perror("Failed to read from device");
        close(fd);
        return errno;
    }


    receive[ret] = '\0';


    printf("Result:\n%s\n", receive);


    close(fd);

    return 0;
}