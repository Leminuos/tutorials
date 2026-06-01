#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "font.h"

#define OLED_SET_XY	  			99
#define OLED_SET_XY_WRITE_DATA  100
#define OLED_SET_XY_WRITE_DATAS 101
#define OLED_SET_DATAS	  		102

int fd;

void oled_disp_char(int x, int y, unsigned char c)
{
	int i = 0;
	const unsigned char *dots = oled_asc2_8x16[c - ' '];
	char pos[2];

	pos[0] = x;
	pos[1] = y;

	ioctl(fd, OLED_SET_XY, pos);
	ioctl(fd, OLED_SET_DATAS | (8<<8), dots);

	pos[0] = x;
	pos[1] = y+1;

	ioctl(fd, OLED_SET_XY, pos);
	ioctl(fd, OLED_SET_DATAS | (8<<8), &dots[8]);
}

void oled_disp_str(int x, int y, char *str)
{
	unsigned char j=0;
	while (str[j])
	{		
		oled_disp_char(x, y, str[j]);
		x += 8;

		if(x > 127)
		{
			x = 0;
			y += 2;
		}

		j++;
	}
}

void oled_disp_test(void)
{ 	
    int i;

    oled_disp_str(0, 0, "Linux tutorial");
    oled_disp_str(0, 2, "Bui Nguyen");
} 

/*
 * oled_test /dev/myoled
 */
int main(int argc, char **argv)
{
	int buf[2];

	if (argc != 2)
	{
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}
	
	fd = open(argv[1], O_RDWR);
	if (fd < 0)
	{
		printf(" can not open %s\n", argv[1]);
		return -1;
	}

	oled_disp_test();
	
	return 0;
}
