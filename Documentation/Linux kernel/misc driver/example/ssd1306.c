// misc_driver.c — SSD1306 via I2C using a misc device
// Build: kernel module
// Device file: /dev/ssd1306_misc
//
// BeagleBone Black (I2C):
//   SSD1306 VCC -> 3.3V
//   SSD1306 GND -> GND
//   SSD1306 SCL -> P9_19 (I2C2_SCL)  => bus #2
//   SSD1306 SDA -> P9_20 (I2C2_SDA)  => bus #2
//
// Protocol notes (I2C):
//   - Control byte 0x00 => following bytes are COMMAND
//   - Control byte 0x40 => following bytes are DATA
//
// Behavior:
//   - On module init: send SSD1306 init sequence
//   - On write(): take ASCII bytes from user, render with 5x8 font,
//     write sequentially from (page=0, col=0), horizontal addressing.
//     If string too long, remaining is truncated (no wrapping for brevity).

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define DRV_NAME        "ssd1306_misc"
#define DEV_NAME        "ssd1306_misc"

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT/8)

#define CTRL_CMD        0x00  // I2C control: command
#define CTRL_DATA       0x40  // I2C control: data

// Module parameters: I2C bus & address (BBB default: bus=2, addr=0x3C)
static int i2c_bus  = 2;
static int i2c_addr = 0x3C;
module_param(i2c_bus, int, 0444);
module_param(i2c_addr, int, 0444);
MODULE_PARM_DESC(i2c_bus,  "I2C bus number (BBB I2C2 = 2)");
MODULE_PARM_DESC(i2c_addr, "I2C 7-bit address of SSD1306 (0x3C or 0x3D)");

static struct i2c_client *g_client;

// ---------- Minimal 5x8 font for ' ', 0-9, A-Z, a-z (others blank) ----------
struct glyph { char c; u8 col[5]; };

static const struct glyph font5x8[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'0', {0x3E,0x45,0x49,0x51,0x3E}},
    {'1', {0x00,0x21,0x7F,0x01,0x00}},
    {'2', {0x23,0x45,0x49,0x49,0x31}},
    {'3', {0x22,0x41,0x49,0x49,0x36}},
    {'4', {0x0C,0x14,0x24,0x7F,0x04}},
    {'5', {0x72,0x51,0x51,0x51,0x4E}},
    {'6', {0x1E,0x29,0x49,0x49,0x06}},
    {'7', {0x40,0x47,0x48,0x50,0x60}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x30,0x49,0x49,0x4A,0x3C}},

    {'A', {0x3E,0x09,0x09,0x09,0x3E}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x3A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},
    {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x04,0x02,0x7F}},
    {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x26,0x49,0x49,0x49,0x32}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x7F,0x20,0x10,0x20,0x7F}},
    {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}},
    {'Z', {0x61,0x51,0x49,0x45,0x43}},

    {'a', {0x20,0x54,0x54,0x54,0x78}},
    {'b', {0x7F,0x48,0x44,0x44,0x38}},
    {'c', {0x38,0x44,0x44,0x44,0x20}},
    {'d', {0x38,0x44,0x44,0x48,0x7F}},
    {'e', {0x38,0x54,0x54,0x54,0x18}},
    {'f', {0x08,0x7E,0x09,0x01,0x02}},
    {'g', {0x0C,0x52,0x52,0x52,0x3E}},
    {'h', {0x7F,0x08,0x04,0x04,0x78}},
    {'i', {0x00,0x44,0x7D,0x40,0x00}},
    {'j', {0x20,0x40,0x44,0x3D,0x00}},
    {'k', {0x7F,0x10,0x28,0x44,0x00}},
    {'l', {0x00,0x41,0x7F,0x40,0x00}},
    {'m', {0x7C,0x04,0x18,0x04,0x78}},
    {'n', {0x7C,0x08,0x04,0x04,0x78}},
    {'o', {0x38,0x44,0x44,0x44,0x38}},
    {'p', {0x7C,0x14,0x14,0x14,0x08}},
    {'q', {0x08,0x14,0x14,0x14,0x7C}},
    {'r', {0x7C,0x08,0x04,0x04,0x08}},
    {'s', {0x48,0x54,0x54,0x54,0x20}},
    {'t', {0x04,0x3F,0x44,0x40,0x20}},
    {'u', {0x3C,0x40,0x40,0x20,0x7C}},
    {'v', {0x1C,0x20,0x40,0x20,0x1C}},
    {'w', {0x3C,0x40,0x30,0x40,0x3C}},
    {'x', {0x44,0x28,0x10,0x28,0x44}},
    {'y', {0x0C,0x50,0x50,0x50,0x3C}},
    {'z', {0x44,0x64,0x54,0x4C,0x44}},
};

static bool glyph_for(char c, u8 out[5])
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(font5x8); i++) {
        if (font5x8[i].c == c) {
            memcpy(out, font5x8[i].col, 5);
            return true;
        }
    }
    memset(out, 0x00, 5);
    return false;
}

// -------------- I2C helpers --------------

static int ssd1306_send_cmd1(u8 cmd)
{
    u8 buf[2] = { CTRL_CMD, cmd };
    struct i2c_msg m = {
        .addr  = g_client->addr,
        .flags = 0,
        .len   = 2,
        .buf   = buf,
    };
    return i2c_transfer(g_client->adapter, &m, 1) == 1 ? 0 : -EIO;
}

static int ssd1306_send_cmds(const u8 *cmds, size_t n)
{
    int ret = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        ret = ssd1306_send_cmd1(cmds[i]);
        if (ret)
            return ret;
    }
    return 0;
}

static int ssd1306_send_data(const u8 *data, size_t n)
{
    // We may need to chunk to avoid I2C controller limits.
    const size_t MAX_CHUNK = 16; // conservative
    u8 buf[1 + MAX_CHUNK];
    struct i2c_msg m = {
        .addr  = g_client->addr,
        .flags = 0,
        .buf   = buf,
    };
    size_t off = 0;

    while (off < n) {
        size_t chunk = min(n - off, MAX_CHUNK);
        buf[0] = CTRL_DATA;
        memcpy(&buf[1], &data[off], chunk);
        m.len = 1 + chunk;
        if (i2c_transfer(g_client->adapter, &m, 1) != 1)
            return -EIO;
        off += chunk;
    }
    return 0;
}

static int ssd1306_set_addr_window(u8 col_start, u8 col_end, u8 page_start, u8 page_end)
{
    u8 seq[] = {
        0x21, col_start, col_end, // Column address
        0x22, page_start, page_end // Page address
    };
    return ssd1306_send_cmds(seq, sizeof(seq));
}

static int ssd1306_clear_screen(void)
{
    int ret;
    u8 zeros[SSD1306_WIDTH];
    memset(zeros, 0x00, sizeof(zeros));

    // Horizontal addressing: clear all pages
    ret = ssd1306_set_addr_window(0, SSD1306_WIDTH - 1, 0, SSD1306_PAGES - 1);
    if (ret) return ret;

    for (int p = 0; p < SSD1306_PAGES; p++) {
        ret = ssd1306_send_data(zeros, SSD1306_WIDTH);
        if (ret) return ret;
    }
    return 0;
}

static int ssd1306_init_sequence(void)
{
    // Standard init (horizontal addressing)
    const u8 init_seq[] = {
        0xAE,             // display off
        0xD5, 0x80,       // clock divide
        0xA8, 0x3F,       // multiplex 1/64
        0xD3, 0x00,       // display offset = 0
        0x40,             // start line = 0
        0x8D, 0x14,       // charge pump enable
        0x20, 0x00,       // memory mode = horizontal
        0xA1,             // segment remap
        0xC8,             // COM scan dir remap
        0xDA, 0x12,       // COM pins config
        0x81, 0x7F,       // contrast
        0xD9, 0xF1,       // pre-charge
        0xDB, 0x40,       // VCOM detect
        0xA4,             // resume from all-on
        0xA6,             // normal display
        0xAF,             // display on
    };
    return ssd1306_send_cmds(init_seq, sizeof(init_seq));
}

// -------------- misc fops --------------

static int misc_open(struct inode *node, struct file *filep)
{
    pr_info("%s: open\n", DRV_NAME);
    return 0;
}

static int misc_release(struct inode *node, struct file *filep)
{
    pr_info("%s: release\n", DRV_NAME);
    return 0;
}

// Write text starting at page 0, col 0 (single row with 8-pixel height).
// Simple renderer: for each ASCII, push 5 columns + 1 column spacing.
static ssize_t misc_write(struct file *filp, const char __user *ubuf,
                          size_t count, loff_t *f_pos)
{
    int ret;
    char *kbuf;
    size_t i, max_chars;
    u8 linebuf[SSD1306_WIDTH];
    size_t out = 0;

    if (!g_client)
        return -ENODEV;

    if (count == 0)
        return 0;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, ubuf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    // Prepare screen: set window to page 0, columns 0..127
    ret = ssd1306_set_addr_window(0, SSD1306_WIDTH - 1, 0, 0);
    if (ret) { kfree(kbuf); return ret; }

    // Pack glyphs into a single line buffer (truncate if exceeds width)
    // Each glyph = 5 cols + 1 spacing => 6 cols per char
    max_chars = SSD1306_WIDTH / 6;

    for (i = 0; i < count && i < max_chars; i++) {
        u8 g[5];
        glyph_for(kbuf[i], g);
        if (out + 6 > sizeof(linebuf)) break;
        linebuf[out + 0] = g[0];
        linebuf[out + 1] = g[1];
        linebuf[out + 2] = g[2];
        linebuf[out + 3] = g[3];
        linebuf[out + 4] = g[4];
        linebuf[out + 5] = 0x00; // 1-column spacing
        out += 6;
    }

    // Fill remaining with blank
    while (out < sizeof(linebuf)) linebuf[out++] = 0x00;

    ret = ssd1306_send_data(linebuf, sizeof(linebuf));
    kfree(kbuf);
    if (ret) return ret;

    return count;
}

static ssize_t misc_read(struct file *filp, char __user *ubuf,
                         size_t count, loff_t *f_pos)
{
    const char *msg = "ssd1306 ready\n";
    size_t len = strlen(msg);

    if (*f_pos >= len)
        return 0;

    if (count > len - *f_pos)
        count = len - *f_pos;

    if (copy_to_user(ubuf, msg + *f_pos, count))
        return -EFAULT;

    *f_pos += count;
    return count;
}

static const struct file_operations misc_fops = {
    .owner   = THIS_MODULE,
    .open    = misc_open,
    .release = misc_release,
    .read    = misc_read,
    .write   = misc_write,
};

static struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEV_NAME,
    .fops  = &misc_fops,
};

// -------------- module init/exit --------------

static int __init ssd1306_misc_init(void)
{
    struct i2c_adapter *adap;
    struct i2c_board_info info = {
        I2C_BOARD_INFO("ssd1306", 0), // modalias (not used by core here)
    };
    int ret;

    pr_info("%s: init (bus=%d addr=0x%02X)\n", DRV_NAME, i2c_bus, i2c_addr);

    adap = i2c_get_adapter(i2c_bus);
    if (!adap) {
        pr_err("%s: cannot get i2c adapter %d\n", DRV_NAME, i2c_bus);
        return -ENODEV;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,5,0)
    info.addr = i2c_addr;
    g_client = i2c_new_client_device(adap, &info);
#else
    g_client = i2c_new_dummy_device(adap, i2c_addr);
#endif
    i2c_put_adapter(adap);

    if (IS_ERR(g_client)) {
        pr_err("%s: cannot create i2c client (addr=0x%02X)\n", DRV_NAME, i2c_addr);
        return PTR_ERR(g_client);
    }

    // Init panel
    ret = ssd1306_init_sequence();
    if (ret) {
        pr_err("%s: init sequence failed: %d\n", DRV_NAME, ret);
        goto err_client;
    }

    // Clear screen once
    ret = ssd1306_clear_screen();
    if (ret) {
        pr_err("%s: clear screen failed: %d\n", DRV_NAME, ret);
        goto err_client;
    }

    // Register misc
    ret = misc_register(&misc_dev);
    if (ret) {
        pr_err("%s: misc_register failed: %d\n", DRV_NAME, ret);
        goto err_client;
    }

    pr_info("%s: ready, device /dev/%s\n", DRV_NAME, DEV_NAME);
    return 0;

err_client:
    i2c_unregister_device(g_client);
    g_client = NULL;
    return ret;
}

static void __exit ssd1306_misc_exit(void)
{
    misc_deregister(&misc_dev);
    if (g_client) {
        i2c_unregister_device(g_client);
        g_client = NULL;
    }
    pr_info("%s: exit\n", DRV_NAME);
}

module_init(ssd1306_misc_init);
module_exit(ssd1306_misc_exit);

MODULE_AUTHOR("Luu An Phu");
MODULE_DESCRIPTION("SSD1306 OLED via I2C using misc device");
MODULE_LICENSE("GPL");
