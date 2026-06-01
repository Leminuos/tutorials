#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/list.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/uaccess.h>

#define OLED_SET_XY	  		99
#define OLED_SET_XY_WRITE_DATA  100
#define OLED_SET_XY_WRITE_DATAS 101
#define OLED_SET_DATAS	  		102

#define OLED_CMD 	0
#define OLED_DATA 	1

struct spi_device *oled_dev;
static struct gpio_desc *oled_dc;
static char data_buf[1024];

static struct fb_info *oled_fb_info;

static void oled_write_cmd_data(unsigned char uc_data,unsigned char uc_cmd)
{
    if(uc_cmd==0)
    {
        gpiod_set_value(oled_dc, 0);
    }
    else
    {
        gpiod_set_value(oled_dc, 1);
    }

    spi_write(oled_dev, &uc_data, 1);
}

static void oled_write_datas(unsigned char *buf, int len)
{
    gpiod_set_value(oled_dc, 1);
    spi_write(oled_dev, buf, len);
}

static int oled_hardware_init(void)
{		  			 		  						  					  				 	   		  	  	 	  
    oled_write_cmd_data(0xae,OLED_CMD);

    oled_write_cmd_data(0x00,OLED_CMD);// lower column address
    oled_write_cmd_data(0x10,OLED_CMD);// higher column address

    oled_write_cmd_data(0x40,OLED_CMD);// display start line

    oled_write_cmd_data(0xB0,OLED_CMD);// page address

    oled_write_cmd_data(0x81,OLED_CMD);// contract control
    oled_write_cmd_data(0x66,OLED_CMD);// 128

    oled_write_cmd_data(0xa1,OLED_CMD);// segment remap

    oled_write_cmd_data(0xa6,OLED_CMD);// normal /reverse

    oled_write_cmd_data(0xa8,OLED_CMD);// multiple ratio
    oled_write_cmd_data(0x3f,OLED_CMD);// duty = 1/64

    oled_write_cmd_data(0xc8,OLED_CMD);// com scan direction

    oled_write_cmd_data(0xd3,OLED_CMD);// set displat offset
    oled_write_cmd_data(0x00,OLED_CMD);//

    oled_write_cmd_data(0xd5,OLED_CMD);// set osc division
    oled_write_cmd_data(0x80,OLED_CMD);//

    oled_write_cmd_data(0xd9,OLED_CMD);// ser pre-charge period
    oled_write_cmd_data(0x1f,OLED_CMD);//

    oled_write_cmd_data(0xda,OLED_CMD);// set com pins
    oled_write_cmd_data(0x12,OLED_CMD);//

    oled_write_cmd_data(0xdb,OLED_CMD);// set vcomh
    oled_write_cmd_data(0x30,OLED_CMD);//

    oled_write_cmd_data(0x8d,OLED_CMD);// set charge pump disable 
    oled_write_cmd_data(0x14,OLED_CMD);//

    oled_write_cmd_data(0xaf,OLED_CMD);// set display on

    return 0;
}

void oled_disp_set_pos(int x, int y)
{ 
    oled_write_cmd_data(0xb0+y,OLED_CMD);
    oled_write_cmd_data((x&0x0f),OLED_CMD); 
    oled_write_cmd_data(((x&0xf0)>>4)|0x10,OLED_CMD);
}   	      	   			 

static void oled_disp_clear(void)  
{
    unsigned char x, y;
    for (y = 0; y < 8; y++)
    {
        oled_disp_set_pos(0, y);
        for (x = 0; x < 128; x++)
            oled_write_cmd_data(0, OLED_DATA); /* 清零 */
    }
}

static int oled_probe(struct spi_device *spi)
{	
    dma_addr_t phy_addr;

    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    oled_fb_info = framebuffer_alloc(0, &spi->dev);

    oled_fb_info->var.xres_virtual = oled_fb_info->var.xres = 128;
    oled_fb_info->var.yres_virtual = oled_fb_info->var.yres = 64;

    oled_fb_info->var.bits_per_pixel = 1;  /* rgb565 */

    strcpy(oled_fb_info->fix.id, "100ask_lcd");
    oled_fb_info->fix.smem_len = oled_fb_info->var.xres * oled_fb_info->var.yres * oled_fb_info->var.bits_per_pixel / 8;

    oled_fb_info->screen_base = dma_alloc_wc(NULL, oled_fb_info->fix.smem_len, &phy_addr, GFP_KERNEL);
    oled_fb_info->fix.smem_start = phy_addr;

    oled_fb_info->fix.type = FB_TYPE_PACKED_PIXELS;
    oled_fb_info->fix.visual = FB_VISUAL_MONO10;

    oled_fb_info->fix.line_length = oled_fb_info->var.xres * oled_fb_info->var.bits_per_pixel / 8;

    register_framebuffer(oled_fb_info);

    /* spi oled init */
    oled_dc = gpiod_get(&spi->dev, "dc", GPIOD_OUT_HIGH);

    oled_hardware_init();

    oled_disp_clear();

    return 0;
}

static int oled_remove(struct spi_device *spi)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	gpiod_put(oled_dc);

	return 0;
}

static const struct of_device_id oled_of_match[] = {
	{.compatible = "100ask,oled"},
	{}
};

static struct spi_driver oled_driver = {
	.driver = {
		.name	= "oled",
		.of_match_table = oled_of_match,
	},
	.probe		= oled_probe,
	.remove		= oled_remove,
	//.id_table	= oled_spi_ids,
};

int oled_init(void)
{
	return spi_register_driver(&oled_driver);
}

static void oled_exit(void)
{
	spi_unregister_driver(&oled_driver);
}

module_init(oled_init);
module_exit(oled_exit);

MODULE_LICENSE("GPL v2");

