#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <unistd.h>
#include "ad5724.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s)
{
    perror(s);
    abort();
}

static const char *device = "/dev/spidev1.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 10000000;
static int fd;

// Write `count` 3-byte transfers
static int ad5724_write(uint8_t *tx, int count)
{
    struct spi_ioc_transfer tr[count];
    int i, ret;
    for (i = 0; i < count; i++) {
        tr[i].tx_buf = (unsigned long)(tx + i*3);
        tr[i].rx_buf = (unsigned long)NULL;
        tr[i].len = 3;
        tr[i].speed_hz = speed;
        tr[i].bits_per_word = bits;
        tr[i].delay_usecs = 0;
        tr[i].cs_change = 1;
    }
    tr[count - 1].cs_change = 0;
    ret = ioctl(fd, SPI_IOC_MESSAGE(count), tr);
    if (ret < 1)
        pabort("can't send spi message");

    return 0;
}

int ad5724_set_outputs(int16_t *outputs)
{
    uint8_t tx[3*4];
    for (int i = 0; i < 4; i++)
    {
        tx[i*3 + 0] = i; // W, 0, REG 0b000, DAC address
        tx[i*3 + 1] = (outputs[i] >> 4) & 0xFF; // Take top 8 bits (of 12)
        tx[i*3 + 2] = (outputs[i] << 4) & 0xF0; // Take bottom 4 bits (of 12) shifted up into high bits
    }
    // TODO: use control register load instead of LDAC line?
    return ad5724_write(tx, 4);
}

// Power on/off all DACs
// MUST wait at least 10us after powerup before loading DAC register
static int ad5724_set_power(bool on)
{
    uint8_t tx[3];
    tx[0] = 0b010 << 3; // PWR REG
    tx[1] = 0;
    if (on)
        tx[2] = 0xF;
    else
        tx[2] = 0;
    return ad5724_write(tx, 1);
}

static int ad5724_set_output_range(uint8_t address, uint8_t range)
{
    uint8_t tx[3];
    tx[0] = (0b001 << 3) | (address & 0x7);
    tx[1] = 0;
    tx[2] = range & 0x7;
    return ad5724_write(tx, 1);
}

int ad5724_init(void)
{
    // TODO: set output range on all channels
    // TODO: power up all channels
}

int main(void)
{
    int ret = 0;

    fd = open(device, O_RDWR);
    if (fd < 0)
        pabort("can't open device");

    /*
     * spi mode
     */
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
        pabort("can't set spi mode");

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
        pabort("can't get spi mode");

    /*
     * bits per word
     */
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort("can't set bits per word");

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort("can't get bits per word");

    /*
     * max speed hz
     */
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort("can't set max speed hz");

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort("can't get max speed hz");

    printf("spi mode: %d\n", mode);
    printf("bits per word: %d\n", bits);
    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    int16_t outputs[] = {1, 2, 3, 4};
    ad5724_set_outputs(outputs);

    close(fd);

    return ret;
}


