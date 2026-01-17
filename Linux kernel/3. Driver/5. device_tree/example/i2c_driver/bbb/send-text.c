#include <stdio.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#define I2C_BUS     "/dev/i2c-2"
#define ESP32_ADDR  0x69
#define CMD_WRITE   0x30

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s \"your message\"\n", argv[0]);
        return 1;
    }

    int fd = open(I2C_BUS, O_RDWR);
    ioctl(fd, I2C_SLAVE, ESP32_ADDR);

    char buf[64];
    buf[0] = CMD_WRITE;
    strcpy(&buf[1], argv[1]);
    write(fd, buf, strlen(argv[1]) + 1);

    close(fd);
    return 0;
}
