/*
 * Floppy test cases.
 *
 * Copyright (c) 2012 Kevin Wolf <kwolf@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>

#include "libqtest.h"
#include "qemu-common.h"

#define TEST_IMAGE_SIZE 1440 * 1024

#define FLOPPY_BASE 0x3f0
#define FLOPPY_IRQ 6

enum {
    reg_sra         = 0x0,
    reg_srb         = 0x1,
    reg_dor         = 0x2,
    reg_msr         = 0x4,
    reg_dsr         = 0x4,
    reg_fifo        = 0x5,
    reg_dir         = 0x7,
};

enum {
    CMD_SENSE_INT   = 0x08,
    CMD_SEEK        = 0x0f,
    CMD_READ        = 0xe6,
};

enum {
    RQM     = 0x80,
    DIO     = 0x40,

    DSKCHG  = 0x80,
};

char test_image[] = "/tmp/qtest.XXXXXX";

#define assert_bit_set(data, mask) g_assert_cmphex((data) & (mask), ==, (mask))
#define assert_bit_clear(data, mask) g_assert_cmphex((data) & (mask), ==, 0)

static uint8_t base = 0x70;

enum {
    CMOS_FLOPPY     = 0x10,
};

static void floppy_send(uint8_t byte)
{
    uint8_t msr;

    msr = inb(FLOPPY_BASE + reg_msr);
    assert_bit_set(msr, RQM);
    assert_bit_clear(msr, DIO);

    outb(FLOPPY_BASE + reg_fifo, byte);
}

static uint8_t floppy_recv(void)
{
    uint8_t msr;

    msr = inb(FLOPPY_BASE + reg_msr);
    assert_bit_set(msr, RQM | DIO);

    return inb(FLOPPY_BASE + reg_fifo);
}

static void ack_irq(void)
{
    g_assert(get_irq(FLOPPY_IRQ));
    floppy_send(CMD_SENSE_INT);
    floppy_recv();
    floppy_recv();
    g_assert(!get_irq(FLOPPY_IRQ));
}

static uint8_t send_read_command(void)
{
    uint8_t drive = 0;
    uint8_t head = 0;
    uint8_t cyl = 0;
    uint8_t sect_addr = 1;
    uint8_t sect_size = 2;
    uint8_t eot = 1;
    uint8_t gap = 0x1b;
    uint8_t gpl = 0xff;

    uint8_t msr = 0;
    uint8_t st0;

    uint8_t ret = 0;

    floppy_send(CMD_READ);
    floppy_send(head << 2 | drive);
    g_assert(!get_irq(FLOPPY_IRQ));
    floppy_send(cyl);
    floppy_send(head);
    floppy_send(sect_addr);
    floppy_send(sect_size);
    floppy_send(eot);
    floppy_send(gap);
    floppy_send(gpl);

    uint8_t i = 0;
    uint8_t n = 2;
    for (; i < n; i++) {
        msr = inb(FLOPPY_BASE + reg_msr);
        if (msr == 0xd0) {
            break;
        }
        sleep(1);
    }

    if (i >= n) {
        return 1;
    }

    st0 = floppy_recv();
    if (st0 != 0x60) {
        ret = 1;
    }

    floppy_recv();
    floppy_recv();
    floppy_recv();
    floppy_recv();
    floppy_recv();
    floppy_recv();

    return ret;
}

static void send_step_pulse(int cyl)
{
    int drive = 0;
    int head = 0;

    floppy_send(CMD_SEEK);
    floppy_send(head << 2 | drive);
    g_assert(!get_irq(FLOPPY_IRQ));
    floppy_send(cyl);
    ack_irq();
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(base + 0, reg);
    return inb(base + 1);
}

static void test_cmos(void)
{
    uint8_t cmos;

    cmos = cmos_read(CMOS_FLOPPY);
    g_assert(cmos == 0x40);
}

static void test_no_media_on_start(void)
{
    uint8_t dir;

    /* Media changed bit must be set all time after start if there is
     * no media in drive. */
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    send_step_pulse(1);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
}

static void test_read_without_media(void)
{
    uint8_t ret;

    ret = send_read_command();
    g_assert(ret == 0);
}

static void test_media_change(void)
{
    uint8_t dir;

    /* Insert media in drive. DSKCHK should not be reset until a step pulse
     * is sent. */
    qmp("{'execute':'change', 'arguments':{ 'device':'floppy0', "
        "'target': '%s' }}", test_image);
    qmp(""); /* ignore event (FIXME open -> open transition?!) */
    qmp(""); /* ignore event */

    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);

    send_step_pulse(0);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);

    /* Step to next track should clear DSKCHG bit. */
    send_step_pulse(1);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_clear(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_clear(dir, DSKCHG);

    /* Eject the floppy and check that DSKCHG is set. Reading it out doesn't
     * reset the bit. */
    qmp("{'execute':'eject', 'arguments':{ 'device':'floppy0' }}");
    qmp(""); /* ignore event */

    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);

    send_step_pulse(0);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);

    send_step_pulse(1);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
    dir = inb(FLOPPY_BASE + reg_dir);
    assert_bit_set(dir, DSKCHG);
}

static void test_sense_interrupt(void)
{
    int drive = 0;
    int head = 0;
    int cyl = 0;
    int ret = 0;

    floppy_send(CMD_SENSE_INT);
    ret = floppy_recv();
    g_assert(ret == 0x80);

    floppy_send(CMD_SEEK);
    floppy_send(head << 2 | drive);
    g_assert(!get_irq(FLOPPY_IRQ));
    floppy_send(cyl);

    floppy_send(CMD_SENSE_INT);
    ret = floppy_recv();
    g_assert(ret == 0x20);
    floppy_recv();
}

/* success if no crash or abort */
static void fuzz_registers(void)
{
    unsigned int i;

    for (i = 0; i < 1000; i++) {
        uint8_t reg, val;

        reg = (uint8_t)g_test_rand_int_range(0, 8);
        val = (uint8_t)g_test_rand_int_range(0, 256);

        outb(FLOPPY_BASE + reg, val);
        inb(FLOPPY_BASE + reg);
    }
}

int main(int argc, char **argv)
{
    const char *arch = qtest_get_arch();
    char *cmdline;
    int fd;
    int ret;

    /* Check architecture */
    if (strcmp(arch, "i386") && strcmp(arch, "x86_64")) {
        g_test_message("Skipping test for non-x86\n");
        return 0;
    }

    /* Create a temporary raw image */
    fd = mkstemp(test_image);
    g_assert(fd >= 0);
    ret = ftruncate(fd, TEST_IMAGE_SIZE);
    g_assert(ret == 0);
    close(fd);

    /* Run the tests */
    g_test_init(&argc, &argv, NULL);

    cmdline = g_strdup_printf("-vnc none ");

    qtest_start(cmdline);
    qtest_irq_intercept_in(global_qtest, "ioapic");
    qtest_add_func("/fdc/cmos", test_cmos);
    qtest_add_func("/fdc/no_media_on_start", test_no_media_on_start);
    qtest_add_func("/fdc/read_without_media", test_read_without_media);
    qtest_add_func("/fdc/media_change", test_media_change);
    qtest_add_func("/fdc/sense_interrupt", test_sense_interrupt);
    qtest_add_func("/fdc/fuzz-registers", fuzz_registers);

    ret = g_test_run();

    /* Cleanup */
    qtest_quit(global_qtest);
    unlink(test_image);

    return ret;
}
