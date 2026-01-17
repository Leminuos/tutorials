#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define MY_IOCTL_CMD  _IOW('M', 1, int)

int main() {
    int fd = open("/dev/ex_chardev", O_RDWR);
    if (fd < 0) {
        perror("Lỗi khi mở file để đọc");
        return 1;
    }

    int value = 42;
    if (ioctl(fd, MY_IOCTL_CMD, &value) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("Gửi lệnh ioctl thành công với giá trị: %d\n", value);
    close(fd);
    return 0;
}
