#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <font.h>

int fd_fb;
struct fb_var_screeninfo var;	/* Current var */
int screen_size;
unsigned char *fbmem;
unsigned int line_width;
unsigned int pixel_width;

void lcd_put_pixel(int x, int y, unsigned int color)
{
	unsigned char *pen_8 = fbmem+y*line_width+x*pixel_width;
	unsigned short *pen_16;	
	unsigned int *pen_32;	
	int bit;

	unsigned int red, green, blue;	

	pen_16 = (unsigned short *)pen_8;
	pen_32 = (unsigned int *)pen_8;

	switch (var.bits_per_pixel)
	{
		case 1:
		{
			pen_8 = fbmem+y*line_width+(x>>3);
			bit = x & 0x7;
			if (color)
				*pen_8 |= (1<<bit);
			else
				*pen_8 &= ~(1<<bit);
			break;
		}
		
		case 8:
		{
			*pen_8 = color;
			break;
		}
		case 16:
		{
			/* 565 */
			red   = (color >> 16) & 0xff;
			green = (color >> 8) & 0xff;
			blue  = (color >> 0) & 0xff;
			color = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
			*pen_16 = color;
			break;
		}
		case 32:
		{
			*pen_32 = color;
			break;
		}
		default:
		{
			printf("can't surport %dbpp\n", var.bits_per_pixel);
			break;
		}
	}
}

void lcd_put_ascii(int x, int y, unsigned char c)
{
	unsigned char *dots = (unsigned char *)&fontdata_8x16[c*16];
	int i, b;
	unsigned char byte;

	for (i = 0; i < 16; i++)
	{
		byte = dots[i];

		for (b = 7; b >= 0; b--)
		{
			if (byte & (1<<b))
			{
				/* show */
				lcd_put_pixel(x+7-b, y+i, 0xffffff);
			}
			else
			{
				/* hide */
				lcd_put_pixel(x+7-b, y+i, 0);
			}
		}
	}
}

int main(int argc, char **argv)
{
	
	if (argc != 2)
	{
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}
	
	fd_fb = open(argv[1], O_RDWR);
	if (fd_fb < 0)
	{
		printf("can't open %s\n", argv[1]);
		return -1;
	}

	if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &var))
	{
		printf("can't get var\n");
		return -1;
	}

	line_width  = var.xres * var.bits_per_pixel / 8;
	pixel_width = var.bits_per_pixel / 8;
	screen_size = var.xres * var.yres * var.bits_per_pixel / 8;
	fbmem = (unsigned char *)mmap(NULL , screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);
	if (fbmem == (unsigned char *)-1)
	{
		printf("can't mmap\n");
		return -1;
	}

	memset(fbmem, 0, screen_size);

	lcd_put_ascii(var.xres/2, var.yres/2, 'A');
	
	munmap(fbmem , screen_size);
	close(fd_fb);
	
	return 0;	
}
