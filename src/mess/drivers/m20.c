/*

 Olivetti M20 skeleton driver, by incog (19/05/2009)

Needs a proper Z8001 CPU core, check also

ftp://ftp.groessler.org/pub/chris/olivetti_m20/misc/bios/rom.s

---

APB notes:

0xfc903 checks for the string TEST at 0x3f4-0x3f6, does an int 0xfe if so, unknown purpose

Error codes:
Triangle    Test CPU registers and instructions
Square      Test ROM
4 vertical lines    CPU call or trap instructions failed
Diamond     Test system RAM
E C0     8255 parallel interface IC test failed
E C1     6845 CRT controller IC test failed
E C2     1797 floppy disk controller chip failed
E C3     8253 timer IC failed
E C4     8251 keyboard interface failed
E C5     8251 keyboard test failed
E C6     8259 PIC IC test failed
E K0     Keyboard did not respond
E K1     Keyboard responds, but self test failed
E D1     Disk drive 1 test failed
E D0     Disk drive 0 test failed
E I0     Non-vectored interrupt error
E I1     Vectored interrupt error

*************************************************************************************************/


#include "emu.h"
#include "cpu/z8000/z8000.h"
#include "cpu/i86/i86.h"
#include "video/mc6845.h"
#include "machine/ram.h"
#include "machine/wd_fdc.h"
#include "machine/i8251.h"
#include "machine/i8255.h"
#include "machine/pit8253.h"
#include "machine/pic8259.h"
#include "formats/m20_dsk.h"

#include "machine/keyboard.h"

class m20_state : public driver_device
{
public:
	m20_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_ram(*this, RAM_TAG),
		m_kbdi8251(*this, "i8251_1"),
		m_ttyi8251(*this, "i8251_2"),
		m_i8255(*this, "ppi8255"),
		m_i8259(*this, "i8259"),
		m_fd1797(*this, "fd1797"),
		m_floppy0(*this, "fd1797:0:5dd"),
		m_floppy1(*this, "fd1797:1:5dd"),
		m_p_videoram(*this, "p_videoram"){ }

	required_device<z8001_device> m_maincpu;
	required_device<ram_device> m_ram;
	required_device<i8251_device> m_kbdi8251;
	required_device<i8251_device> m_ttyi8251;
	required_device<i8255_device> m_i8255;
	required_device<pic8259_device> m_i8259;
	required_device<fd1797_t> m_fd1797;
	required_device<floppy_image_device> m_floppy0;
	required_device<floppy_image_device> m_floppy1;

	required_shared_ptr<UINT16> m_p_videoram;

	virtual void machine_start();
	virtual void machine_reset();

	DECLARE_READ16_MEMBER(m20_i8259_r);
	DECLARE_WRITE16_MEMBER(m20_i8259_w);
	DECLARE_READ16_MEMBER(port21_r);
	DECLARE_WRITE16_MEMBER(port21_w);
	DECLARE_WRITE_LINE_MEMBER(pic_irq_line_w);
	DECLARE_WRITE_LINE_MEMBER(tty_clock_tick_w);
	DECLARE_WRITE_LINE_MEMBER(kbd_clock_tick_w);
	DECLARE_WRITE_LINE_MEMBER(timer_tick_w);
	DECLARE_WRITE_LINE_MEMBER(kbd_tx);
	DECLARE_WRITE8_MEMBER(kbd_put);

private:
	bool m_kbrecv_in_progress;
	int m_kbrecv_bitcount;
	offs_t m_memsize;
	UINT16 m_kbrecv_data;
	UINT8 m_port21;
	void install_memory();

public:
	DECLARE_DRIVER_INIT(m20);
	virtual void video_start();
	UINT32 screen_update_m20(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	DECLARE_WRITE_LINE_MEMBER(kbd_rxrdy_int);

	DECLARE_FLOPPY_FORMATS( floppy_formats );

	void fdc_intrq_w(bool state);
	IRQ_CALLBACK_MEMBER(m20_irq_callback);
};


#define MAIN_CLOCK 4000000 /* 4 MHz */
#define PIXEL_CLOCK XTAL_4_433619MHz


void m20_state::video_start()
{
}

UINT32 m20_state::screen_update_m20(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	int x,y,i;
	UINT8 pen;
	UINT32 count;

	bitmap.fill(get_black_pen(machine()), cliprect);

	count = (0);

	for(y=0; y<256; y++)
	{
		for(x=0; x<512; x+=16)
		{
			for (i = 0; i < 16; i++)
			{
				pen = (m_p_videoram[count]) >> (15 - i) & 1;

				if (screen.visible_area().contains(x + i, y))
					bitmap.pix32(y, x + i) = machine().pens[pen];
			}

			count++;
		}
	}
	return 0;
}

/* TODO: correct hookup for keyboard, keyboard uses 8048 */

WRITE_LINE_MEMBER(m20_state::kbd_tx)
{
	UINT8 data;

	if (m_kbrecv_in_progress) {
		m_kbrecv_bitcount++;
		m_kbrecv_data = (m_kbrecv_data >> 1) | (state ? (1<<10) : 0);
		if (m_kbrecv_bitcount == 11) {
			data = (m_kbrecv_data >> 1) & 0xff;
			printf ("0x%02X received by keyboard\n", data);
			switch (data) {
				case 0x03: m_kbdi8251->receive_character(2); printf ("sending 2 back from kb...\n"); break;
				case 0x0a: break;
				case 0x80: m_kbdi8251->receive_character(0x80); printf ("sending 0x80 back from kb...\n");break;
				default: abort();
			}
			m_kbrecv_in_progress = 0;
		}
	}
	else {
		m_kbrecv_in_progress = 1;
		m_kbrecv_bitcount = 1;
		m_kbrecv_data = state ? (1<<10) : 0;
	}
}


/*
port21      =   0x21        !TTL latch
!   (see hw1 document, fig 2-33, pg 2-47)
!       Output                      Input
!       ======                      =====
!   B0  0 selects floppy 0
!       1 deselects floppy 0
!   B1  0 selects floppy 1
!       1 deselects floppy 1
!   B2  Motor On (Not Used)
!   B3  0 selects double density    0 => Skip basic tests
!       1 selects single density    1 => Perform basic tests
!                                   Latched copy when B7 is written to Port21
!   B4  Uncommitted output          0 => 128K card(s) present
!                                   1 => 32K card(s) present
!                                   Cannot mix types!
!   B5  Uncommitted output          0 => 8-colour card present
!                                   1 => 4-colour card present
!   B6  Uncommitted output          0 => RAM
!                                   1 => ROM (???)
!   B7  See B3 input                0 => colour card present
*/

READ16_MEMBER(m20_state::port21_r)
{
	//printf("port21 read: offset 0x%x\n", offset);
	return m_port21;
}

WRITE16_MEMBER(m20_state::port21_w)
{
	//printf("port21 write: offset 0x%x, data 0x%x\n", offset, data);
	m_port21 = (m_port21 & 0xf8) | (data & 0x7);

	// floppy drive select
	if (data & 1) {
		m_floppy0->mon_w(0);
		m_fd1797->set_floppy(m_floppy0);
	}
	else
		m_floppy0->mon_w(1);

	if (data & 2) {
		m_floppy1->mon_w(0);
		m_fd1797->set_floppy(m_floppy1);
	}
	else
		m_floppy1->mon_w(1);

	if(!(data & 3))
		m_fd1797->set_floppy(NULL);

	// density select 1 - sd, 0 - dd
	m_fd1797->dden_w(data & 8);
}

READ16_MEMBER(m20_state::m20_i8259_r)
{
	return m_i8259->read(space, offset)<<1;
}

WRITE16_MEMBER(m20_state::m20_i8259_w)
{
	m_i8259->write(space, offset, (data>>1));
}

WRITE_LINE_MEMBER( m20_state::pic_irq_line_w )
{
	if (state)
	{
		//printf ("PIC raised VI\n");
		m_maincpu->set_input_line(1, ASSERT_LINE);
	}
	else
	{
		//printf ("PIC lowered VI\n");
		m_maincpu->set_input_line(1, CLEAR_LINE);
	}
}

WRITE_LINE_MEMBER( m20_state::tty_clock_tick_w )
{
	m_ttyi8251->transmit_clock();
	m_ttyi8251->receive_clock();
}

WRITE_LINE_MEMBER( m20_state::kbd_clock_tick_w )
{
	m_kbdi8251->transmit_clock();
	m_kbdi8251->receive_clock();
}

WRITE_LINE_MEMBER( m20_state::timer_tick_w )
{
	/* Using HOLD_LINE is not completely correct:
	 * The output of the 8253 is connected to a 74LS74 flop chip.
	 * The output of the flop chip is connected to NVI CPU input.
	 * The flop is reset by a 1:8 decoder which compares CPU ST0-ST3
	 * outputs to detect an interrupt acknowledge transaction.
	 * 8253 is programmed in square wave mode, not rate
	 * generator mode.
	 */
	m_maincpu->set_input_line(0, state ? HOLD_LINE /*ASSERT_LINE*/ : CLEAR_LINE);
}


/* Memory map description (by courtesy of Dwight Elvey)

    DRAM0 = motherboard (128K)
    DRAM1 = first slot from keyboard end
    DRAM2 = second slot from keyboard end
    DRAM3 = third slot from keyboard end
    SRAM0 = memory window for expansion slot
    ROM0  = ROM

Expansion cards are either 32K or 128K. They cannot be mixed, all installed
cards need to be the same type.

B/W, 32K cards, 3 cards => 224K of memory:
<0>0000 D DRAM0  4000 I DRAM0  4000  <8>0000 D DRAM0 18000 I DRAM0  8000
<0>4000 D DRAM1  4000 I DRAM0  8000  <8>4000 D DRAM0 1C000 I DRAM0  C000
<0>8000 D DRAM2  0000 I DRAM0  C000  <8>8000 D DRAM2  4000 I DRAM1  4000
<0>C000 D DRAM2  4000 I DRAM0 10000  <8>C000 D DRAM3  0000 I DRAM2  0000
<1>0000 D DRAM0 14000 I DRAM0  8000  <9>0000 D DRAM0 18000 I DRAM0 18000
<1>4000 D DRAM0 18000 I DRAM0  C000  <9>4000 D DRAM0 1C000 I DRAM0 1C000
<1>8000 D DRAM0 1C000 I DRAM0 10000  <9>8000 D DRAM2  4000 I DRAM2  4000
<1>C000 D DRAM1  0000 I None         <9>C000 D DRAM3  0000 I DRAM3  0000
<2>0000 D DRAM0 14000 I DRAM0 14000  <A>0000 D DRAM0  8000 I DRAM0  8000
<2>4000 D DRAM0 18000 I DRAM0 18000  <A>4000 D DRAM0  C000 I DRAM0  C000
<2>8000 D DRAM0 1C000 I DRAM0 1C000  <A>8000 D DRAM1  4000 I DRAM1  4000
<2>C000 D DRAM1  0000 I DRAM1  0000  <A>C000 D DRAM2  0000 I DRAM2  0000
<3>0000 D DRAM0  0000 I DRAM0  0000  <B>0000 D DRAM3  4000 I DRAM3  4000
<3>4000 D None        I None         <B>4000 D None        I None
<3>8000 D None        I None         <B>8000 D None        I None
<3>C000 D None        I None         <B>C000 D None        I None
<4>0000 D  ROM0  0000 I  ROM0  0000  <C>0000 D DRAM3  4000 I None
<4>4000 D DRAM3  0000 I  ROM0 10000  <C>4000 D None        I None
<4>8000 D DRAM3  4000 I  ROM0 14000  <C>8000 D None        I None
<4>C000 D None        I  ROM0 18000  <C>C000 D None        I None
<5>0000 D DRAM0  8000 I  ROM0 10000  <D>0000 D None        I None
<5>4000 D DRAM0  C000 I  ROM0 14000  <D>4000 D None        I None
<5>8000 D DRAM0 10000 I  ROM0 18000  <D>8000 D None        I None
<5>C000 D SRAM0  0000 I SRAM0  0000  <D>C000 D None        I None
<6>0000 D DRAM0  8000 I DRAM0  8000  <E>0000 D None        I None
<6>4000 D DRAM0  C000 I DRAM0  C000  <E>4000 D None        I None
<6>8000 D DRAM0 10000 I DRAM0 10000  <E>8000 D None        I None
<6>C000 D None        I None         <E>C000 D None        I None
<7>0000 D  ROM0  0000 I  ROM0  0000  <F>0000 D None        I None
<7>4000 D  ROM0 10000 I  ROM0 10000  <F>4000 D None        I None
<7>8000 D  ROM0 14000 I  ROM0 14000  <F>8000 D None        I None
<7>C000 D  ROM0 18000 I  ROM0 18000  <F>C000 D None        I None

B/W, 128K cards, 3 cards => 512K of memory:
<0>0000 D DRAM0  4000 I DRAM0  4000  <8>0000 D DRAM0 18000 I DRAM0  8000
<0>4000 D DRAM1  4000 I DRAM0  8000  <8>4000 D DRAM0 1C000 I DRAM0  C000
<0>8000 D DRAM2  0000 I DRAM0  C000  <8>8000 D DRAM1  C000 I DRAM1  4000
<0>C000 D DRAM2  4000 I DRAM0 10000  <8>C000 D DRAM1 10000 I DRAM1  8000
<1>0000 D DRAM0 14000 I DRAM0  8000  <9>0000 D DRAM0 18000 I DRAM0 18000
<1>4000 D DRAM0 18000 I DRAM0  C000  <9>4000 D DRAM0 1C000 I DRAM0 1C000
<1>8000 D DRAM0 1C000 I DRAM0 10000  <9>8000 D DRAM1  C000 I DRAM1  C000
<1>C000 D DRAM1  0000 I None         <9>C000 D DRAM1 10000 I DRAM1 10000
<2>0000 D DRAM0 14000 I DRAM0 14000  <A>0000 D DRAM0  8000 I DRAM0  8000
<2>4000 D DRAM0 18000 I DRAM0 18000  <A>4000 D DRAM0  C000 I DRAM0  C000
<2>8000 D DRAM0 1C000 I DRAM0 1C000  <A>8000 D DRAM1  4000 I DRAM1  4000
<2>C000 D DRAM1  0000 I DRAM1  0000  <A>C000 D DRAM1  8000 I DRAM1  8000
<3>0000 D DRAM0  0000 I DRAM0  0000  <B>0000 D DRAM1 14000 I DRAM1 14000
<3>4000 D None        I None         <B>4000 D DRAM1 18000 I DRAM1 18000
<3>8000 D None        I None         <B>8000 D DRAM1 1C000 I DRAM1 1C000
<3>C000 D None        I None         <B>C000 D DRAM2  0000 I DRAM2  0000
<4>0000 D  ROM0  0000 I  ROM0  0000  <C>0000 D DRAM2  4000 I DRAM2  4000
<4>4000 D DRAM3  0000 I None         <C>4000 D DRAM2  8000 I DRAM2  8000
<4>8000 D DRAM3  4000 I None         <C>8000 D DRAM2  C000 I DRAM2  C000
<4>C000 D None        I None         <C>C000 D DRAM2 10000 I DRAM2 10000
<5>0000 D DRAM0  8000 I  ROM0 10000  <D>0000 D DRAM2 14000 I DRAM2 14000
<5>4000 D DRAM0  C000 I  ROM0 14000  <D>4000 D DRAM2 18000 I DRAM2 18000
<5>8000 D DRAM0 10000 I  ROM0 18000  <D>8000 D DRAM2 1C000 I DRAM2 1C000
<5>C000 D SRAM0  0000 I SRAM0  0000  <D>C000 D DRAM3  0000 I DRAM3  0000
<6>0000 D DRAM0  8000 I DRAM0  8000  <E>0000 D DRAM3  4000 I DRAM3  4000
<6>4000 D DRAM0  C000 I DRAM0  C000  <E>4000 D DRAM3  8000 I DRAM3  8000
<6>8000 D DRAM0 10000 I DRAM0 10000  <E>8000 D DRAM3  C000 I DRAM3  C000
<6>C000 D None        I None         <E>C000 D DRAM3 10000 I DRAM3 10000
<7>0000 D  ROM0  0000 I  ROM0  0000  <F>0000 D DRAM3 14000 I DRAM3 14000
<7>4000 D  ROM0 10000 I  ROM0 10000  <F>4000 D DRAM3 18000 I DRAM3 18000
<7>8000 D  ROM0 14000 I  ROM0 14000  <F>8000 D DRAM3 1C000 I DRAM3 1C000
<7>C000 D  ROM0 18000 I  ROM0 18000  <F>C000 D DRAM3  0000 I DRAM3  0000
*/


static ADDRESS_MAP_START(m20_program_mem, AS_PROGRAM, 16, m20_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE( 0x30000, 0x33fff ) AM_RAM AM_SHARE("p_videoram")
	AM_RANGE( 0x40000, 0x41fff ) AM_ROM AM_REGION("maincpu", 0x00000)
ADDRESS_MAP_END

static ADDRESS_MAP_START(m20_data_mem, AS_DATA, 16, m20_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE( 0x30000, 0x33fff ) AM_RAM AM_SHARE("p_videoram")
	AM_RANGE( 0x40000, 0x41fff ) AM_ROM AM_REGION("maincpu", 0x00000)
ADDRESS_MAP_END


void m20_state::install_memory()
{
	m20_state *state = machine().driver_data<m20_state>();

	m_memsize = m_ram->size();
	UINT8 *memptr = m_ram->pointer();
	address_space& pspace = m_maincpu->space(AS_PROGRAM);
	address_space& dspace = m_maincpu->space(AS_DATA);

	/* install mainboard memory (aka DRAM0) */

	/* <0>0000 */
	pspace.install_readwrite_bank(0x0000, 0x3fff, 0x3fff, 0, "dram0_4000");
	dspace.install_readwrite_bank(0x0000, 0x3fff, 0x3fff, 0, "dram0_4000");
	/* <0>4000 */
	pspace.install_readwrite_bank(0x4000, 0x7fff, 0x3fff, 0, "dram0_8000");
	/* <0>8000 */
	pspace.install_readwrite_bank(0x8000, 0xbfff, 0x3fff, 0, "dram0_c000");
	/* <0>C000 */
	pspace.install_readwrite_bank(0xc000, 0xcfff, 0x3fff, 0, "dram0_10000");
	/* <1>0000 */
	pspace.install_readwrite_bank(0x10000, 0x13fff, 0x3fff, 0, "dram0_8000");
	dspace.install_readwrite_bank(0x10000, 0x13fff, 0x3fff, 0, "dram0_14000");
	/* <1>4000 */
	pspace.install_readwrite_bank(0x14000, 0x17fff, 0x3fff, 0, "dram0_c000");
	dspace.install_readwrite_bank(0x14000, 0x17fff, 0x3fff, 0, "dram0_18000");
	/* <1>8000 */
	pspace.install_readwrite_bank(0x18000, 0x1bfff, 0x3fff, 0, "dram0_10000");
	dspace.install_readwrite_bank(0x18000, 0x1bfff, 0x3fff, 0, "dram0_1c000");
	/* <1>c000 empty*/
	/* <2>0000 */
	pspace.install_readwrite_bank(0x20000, 0x23fff, 0x3fff, 0, "dram0_14000");
	dspace.install_readwrite_bank(0x20000, 0x23fff, 0x3fff, 0, "dram0_14000");
	/* <2>4000 */
	pspace.install_readwrite_bank(0x24000, 0x27fff, 0x3fff, 0, "dram0_18000");
	dspace.install_readwrite_bank(0x24000, 0x27fff, 0x3fff, 0, "dram0_18000");
	/* <2>8000 */
	pspace.install_readwrite_bank(0x28000, 0x2bfff, 0x3fff, 0, "dram0_1c000");
	dspace.install_readwrite_bank(0x28000, 0x2bfff, 0x3fff, 0, "dram0_1c000");
	/* <2>c000 empty*/
	/* <3>0000 (video buffer)
	pspace.install_readwrite_bank(0x30000, 0x33fff, 0x3fff, 0, "dram0_0000");
	dspace.install_readwrite_bank(0x30000, 0x33fff, 0x3fff, 0, "dram0_0000");
	*/

	/* <5>0000 */
	dspace.install_readwrite_bank(0x50000, 0x53fff, 0x3fff, 0, "dram0_8000");
	/* <5>4000 */
	dspace.install_readwrite_bank(0x54000, 0x57fff, 0x3fff, 0, "dram0_c000");
	/* <5>8000 */
	dspace.install_readwrite_bank(0x58000, 0x5bfff, 0x3fff, 0, "dram0_10000");
	/* <5>c000 expansion bus */
	/* <6>0000 */
	pspace.install_readwrite_bank(0x60000, 0x63fff, 0x3fff, 0, "dram0_8000");
	dspace.install_readwrite_bank(0x60000, 0x63fff, 0x3fff, 0, "dram0_8000");
	/* <6>4000 */
	pspace.install_readwrite_bank(0x64000, 0x67fff, 0x3fff, 0, "dram0_c000");
	dspace.install_readwrite_bank(0x64000, 0x67fff, 0x3fff, 0, "dram0_c000");
	/* <6>8000 */
	pspace.install_readwrite_bank(0x68000, 0x6bfff, 0x3fff, 0, "dram0_10000");
	dspace.install_readwrite_bank(0x68000, 0x6bfff, 0x3fff, 0, "dram0_10000");
	/* <6>c000 empty*/
	/* segment <7> expansion ROM? */
	/* <8>0000 */
	pspace.install_readwrite_bank(0x80000, 0x83fff, 0x3fff, 0, "dram0_8000");
	dspace.install_readwrite_bank(0x80000, 0x83fff, 0x3fff, 0, "dram0_18000");
	/* <8>4000 */
	pspace.install_readwrite_bank(0x84000, 0x87fff, 0x3fff, 0, "dram0_c000");
	dspace.install_readwrite_bank(0x84000, 0x87fff, 0x3fff, 0, "dram0_1c000");
	/* <9>0000 */
	pspace.install_readwrite_bank(0x90000, 0x93fff, 0x3fff, 0, "dram0_18000");
	dspace.install_readwrite_bank(0x90000, 0x93fff, 0x3fff, 0, "dram0_18000");
	/* <9>4000 */
	pspace.install_readwrite_bank(0x94000, 0x97fff, 0x3fff, 0, "dram0_1c000");
	dspace.install_readwrite_bank(0x94000, 0x97fff, 0x3fff, 0, "dram0_1c000");
	/* <A>0000 */
	pspace.install_readwrite_bank(0xa0000, 0xa3fff, 0x3fff, 0, "dram0_8000");
	dspace.install_readwrite_bank(0xa0000, 0xa3fff, 0x3fff, 0, "dram0_8000");
	/* <A>4000 */
	pspace.install_readwrite_bank(0xa4000, 0xa7fff, 0x3fff, 0, "dram0_c000");
	dspace.install_readwrite_bank(0xa4000, 0xa7fff, 0x3fff, 0, "dram0_c000");

	//state->membank("dram0_0000")->set_base(memptr);
	state->membank("dram0_4000")->set_base(memptr + 0x4000);
	state->membank("dram0_8000")->set_base(memptr + 0x8000);
	state->membank("dram0_c000")->set_base(memptr + 0xc000);
	state->membank("dram0_10000")->set_base(memptr + 0x10000);
	state->membank("dram0_14000")->set_base(memptr + 0x14000);
	state->membank("dram0_18000")->set_base(memptr + 0x18000);
	state->membank("dram0_1c000")->set_base(memptr + 0x1c000);

	if (m_memsize > 128 * 1024) {
		/* install memory expansions (DRAM1..DRAM3) */

		if (m_memsize < 256 * 1024) {
			/* 32K expansion cards */

			/* DRAM1, 32K */

			/* prog
			   AM_RANGE( 0x2c000, 0x2ffff ) AM_RAM AM_SHARE("dram1_0000")
			   AM_RANGE( 0x88000, 0x8bfff ) AM_RAM AM_SHARE("dram1_4000")
			   AM_RANGE( 0xa8000, 0xabfff ) AM_RAM AM_SHARE("dram1_4000")
			*/
			pspace.install_readwrite_bank(0x2c000, 0x2ffff, 0x3fff, 0, "dram1_0000");
			pspace.install_readwrite_bank(0x88000, 0x8bfff, 0x3fff, 0, "dram1_4000");
			pspace.install_readwrite_bank(0xa8000, 0xabfff, 0x3fff, 0, "dram1_4000");

			/*
			  data
			  AM_RANGE( 0x04000, 0x07fff ) AM_RAM AM_SHARE("dram1_4000")
			  AM_RANGE( 0x1c000, 0x1ffff ) AM_RAM AM_SHARE("dram1_0000")
			  AM_RANGE( 0x2c000, 0x2ffff ) AM_RAM AM_SHARE("dram1_0000")
			  AM_RANGE( 0xa8000, 0xabfff ) AM_RAM AM_SHARE("dram1_4000")
			*/
			dspace.install_readwrite_bank(0x4000, 0x7fff, 0x3fff, 0, "dram1_4000");
			dspace.install_readwrite_bank(0x1c000, 0x1ffff, 0x3fff, 0, "dram1_0000");
			dspace.install_readwrite_bank(0x2c000, 0x2ffff, 0x3fff, 0, "dram1_0000");
			dspace.install_readwrite_bank(0xa8000, 0xabfff, 0x3fff, 0, "dram1_4000");

			state->membank("dram1_0000")->set_base(memptr + 0x20000);
			state->membank("dram1_4000")->set_base(memptr + 0x24000);

			if (m_memsize > 128 * 1024 + 32768) {
				/* DRAM2, 32K */

				/* prog
				   AM_RANGE( 0x8c000, 0x8ffff ) AM_RAM AM_SHARE("dram2_0000")
				   AM_RANGE( 0x98000, 0x9bfff ) AM_RAM AM_SHARE("dram2_4000")
				   AM_RANGE( 0xac000, 0xaffff ) AM_RAM AM_SHARE("dram2_0000")
				*/
				pspace.install_readwrite_bank(0x8c000, 0x8ffff, 0x3fff, 0, "dram2_0000");
				pspace.install_readwrite_bank(0x98000, 0x9bfff, 0x3fff, 0, "dram2_4000");
				pspace.install_readwrite_bank(0xac000, 0xaffff, 0x3fff, 0, "dram2_0000");

				/* data
				   AM_RANGE( 0x08000, 0x0bfff ) AM_RAM AM_SHARE("dram2_0000")
				   AM_RANGE( 0x0c000, 0x0ffff ) AM_RAM AM_SHARE("dram2_4000")
				   AM_RANGE( 0x88000, 0x8bfff ) AM_RAM AM_SHARE("dram2_4000")
				   AM_RANGE( 0x98000, 0x9bfff ) AM_RAM AM_SHARE("dram2_4000")
				   AM_RANGE( 0xac000, 0xaffff ) AM_RAM AM_SHARE("dram2_0000")
				 */
				dspace.install_readwrite_bank(0x8000, 0xbfff, 0x3fff, 0, "dram2_0000");
				dspace.install_readwrite_bank(0xc000, 0xffff, 0x3fff, 0, "dram2_4000");
				dspace.install_readwrite_bank(0x88000, 0x8bfff, 0x3fff, 0, "dram2_4000");
				dspace.install_readwrite_bank(0x98000, 0x9bfff, 0x3fff, 0, "dram2_4000");
				dspace.install_readwrite_bank(0xac000, 0xaffff, 0x3fff, 0, "dram2_0000");

				state->membank("dram2_0000")->set_base(memptr + 0x28000);
				state->membank("dram2_4000")->set_base(memptr + 0x2c000);
			}
			if (m_memsize > 128 * 1024 + 2 * 32768) {
				/* DRAM3, 32K */

				/* prog
				   AM_RANGE( 0x9c000, 0x9ffff ) AM_RAM AM_SHARE("dram3_0000")
				   AM_RANGE( 0xb0000, 0xb3fff ) AM_RAM AM_SHARE("dram3_4000")
				*/
				pspace.install_readwrite_bank(0x9c000, 0x9ffff, 0x3fff, 0, "dram3_0000");
				pspace.install_readwrite_bank(0xb0000, 0xb3fff, 0x3fff, 0, "dram3_4000");

				/* data
				   AM_RANGE( 0x44000, 0x47fff ) AM_RAM AM_SHARE("dram3_0000")
				   AM_RANGE( 0x48000, 0x4bfff ) AM_RAM AM_SHARE("dram3_4000")
				   AM_RANGE( 0x8c000, 0x8ffff ) AM_RAM AM_SHARE("dram3_0000")
				   AM_RANGE( 0x9c000, 0x9ffff ) AM_RAM AM_SHARE("dram3_0000")
				   AM_RANGE( 0xb0000, 0xb3fff ) AM_RAM AM_SHARE("dram3_4000")
				   AM_RANGE( 0xc0000, 0xc3fff ) AM_RAM AM_SHARE("dram3_4000")
				 */
				dspace.install_readwrite_bank(0x44000, 0x47fff, 0x3fff, 0, "dram3_0000");
				dspace.install_readwrite_bank(0x48000, 0x4bfff, 0x3fff, 0, "dram3_4000");
				dspace.install_readwrite_bank(0x8c000, 0x8ffff, 0x3fff, 0, "dram3_0000");
				dspace.install_readwrite_bank(0x9c000, 0x9ffff, 0x3fff, 0, "dram3_0000");
				dspace.install_readwrite_bank(0xb0000, 0xb3fff, 0x3fff, 0, "dram3_4000");
				dspace.install_readwrite_bank(0xc0000, 0xc3fff, 0x3fff, 0, "dram3_4000");

				state->membank("dram3_0000")->set_base(memptr + 0x30000);
				state->membank("dram3_4000")->set_base(memptr + 0x34000);
			}
		}
		else {
			/* 128K expansion cards */

			/* DRAM1, 128K */

			/* prog
			   AM_RANGE( 0x2c000, 0x2ffff ) AM_RAM AM_SHARE("dram1_0000")
			   AM_RANGE( 0x88000, 0x8bfff ) AM_RAM AM_SHARE("dram1_4000")
			   AM_RANGE( 0x8c000, 0x8ffff ) AM_RAM AM_SHARE("dram1_8000")
			   AM_RANGE( 0x98000, 0x9bfff ) AM_RAM AM_SHARE("dram1_c000")
			   AM_RANGE( 0x9c000, 0x9ffff ) AM_RAM AM_SHARE("dram1_10000")
			   AM_RANGE( 0xa8000, 0xabfff ) AM_RAM AM_SHARE("dram1_4000")
			   AM_RANGE( 0xac000, 0xaffff ) AM_RAM AM_SHARE("dram1_8000")
			   AM_RANGE( 0xb0000, 0xb3fff ) AM_RAM AM_SHARE("dram1_14000")
			   AM_RANGE( 0xb4000, 0xb7fff ) AM_RAM AM_SHARE("dram1_18000")
			   AM_RANGE( 0xb8000, 0xbbfff ) AM_RAM AM_SHARE("dram1_1c000")
			*/
			pspace.install_readwrite_bank(0x2c000, 0x2ffff, 0x3fff, 0, "dram1_0000");
			pspace.install_readwrite_bank(0x88000, 0x8bfff, 0x3fff, 0, "dram1_4000");
			pspace.install_readwrite_bank(0x8c000, 0x8ffff, 0x3fff, 0, "dram1_8000");
			pspace.install_readwrite_bank(0x98000, 0x9bfff, 0x3fff, 0, "dram1_c000");
			pspace.install_readwrite_bank(0x9c000, 0x9ffff, 0x3fff, 0, "dram1_10000");
			pspace.install_readwrite_bank(0xa8000, 0xabfff, 0x3fff, 0, "dram1_4000");
			pspace.install_readwrite_bank(0xac000, 0xaffff, 0x3fff, 0, "dram1_8000");
			pspace.install_readwrite_bank(0xb0000, 0xb3fff, 0x3fff, 0, "dram1_14000");
			pspace.install_readwrite_bank(0xb4000, 0xb7fff, 0x3fff, 0, "dram1_18000");
			pspace.install_readwrite_bank(0xb8000, 0xbbfff, 0x3fff, 0, "dram1_1c000");

			/* data
			   AM_RANGE( 0x04000, 0x07fff ) AM_RAM AM_SHARE("dram1_4000")
			   AM_RANGE( 0x1c000, 0x1ffff ) AM_RAM AM_SHARE("dram1_0000")
			   AM_RANGE( 0x2c000, 0x2ffff ) AM_RAM AM_SHARE("dram1_0000")
			   AM_RANGE( 0x88000, 0x8bfff ) AM_RAM AM_SHARE("dram1_c000")
			   AM_RANGE( 0x8c000, 0x8ffff ) AM_RAM AM_SHARE("dram1_10000")
			   AM_RANGE( 0x98000, 0x9bfff ) AM_RAM AM_SHARE("dram1_c000")
			   AM_RANGE( 0x9c000, 0x9ffff ) AM_RAM AM_SHARE("dram1_10000")
			   AM_RANGE( 0xa8000, 0xabfff ) AM_RAM AM_SHARE("dram1_4000")
			   AM_RANGE( 0xac000, 0xaffff ) AM_RAM AM_SHARE("dram1_8000")
			   AM_RANGE( 0xb0000, 0xb3fff ) AM_RAM AM_SHARE("dram1_14000")
			   AM_RANGE( 0xb4000, 0xb7fff ) AM_RAM AM_SHARE("dram1_18000")
			   AM_RANGE( 0xb8000, 0xbbfff ) AM_RAM AM_SHARE("dram1_1c000")
			 */
			dspace.install_readwrite_bank(0x4000, 0x7fff, 0x3fff, 0, "dram1_4000");
			dspace.install_readwrite_bank(0x1c000, 0x1ffff, 0x3fff, 0, "dram1_0000");
			dspace.install_readwrite_bank(0x2c000, 0x2ffff, 0x3fff, 0, "dram1_0000");
			dspace.install_readwrite_bank(0x88000, 0x8bfff, 0x3fff, 0, "dram1_c000");
			dspace.install_readwrite_bank(0x8c000, 0x8ffff, 0x3fff, 0, "dram1_10000");
			dspace.install_readwrite_bank(0x98000, 0x9bfff, 0x3fff, 0, "dram1_c000");
			dspace.install_readwrite_bank(0x9c000, 0x9ffff, 0x3fff, 0, "dram1_10000");
			dspace.install_readwrite_bank(0xa8000, 0xabfff, 0x3fff, 0, "dram1_4000");
			dspace.install_readwrite_bank(0xac000, 0xaffff, 0x3fff, 0, "dram1_8000");
			dspace.install_readwrite_bank(0xb0000, 0xb3fff, 0x3fff, 0, "dram1_14000");
			dspace.install_readwrite_bank(0xb4000, 0xb7fff, 0x3fff, 0, "dram1_18000");
			dspace.install_readwrite_bank(0xb8000, 0xbbfff, 0x3fff, 0, "dram1_1c000");

			state->membank("dram1_0000")->set_base(memptr + 0x20000);
			state->membank("dram1_4000")->set_base(memptr + 0x24000);
			state->membank("dram1_8000")->set_base(memptr + 0x28000);
			state->membank("dram1_c000")->set_base(memptr + 0x2c000);
			state->membank("dram1_10000")->set_base(memptr + 0x30000);
			state->membank("dram1_14000")->set_base(memptr + 0x34000);
			state->membank("dram1_18000")->set_base(memptr + 0x38000);
			state->membank("dram1_1c000")->set_base(memptr + 0x3c000);

			if (m_memsize > 256 * 1024) {
				/* DRAM2, 128K */

				/* prog
				   AM_RANGE( 0xbc000, 0xbffff ) AM_RAM AM_SHARE("dram2_0000")

				   AM_RANGE( 0xc0000, 0xc3fff ) AM_RAM AM_SHARE("dram2_4000")
				   AM_RANGE( 0xc4000, 0xc7fff ) AM_RAM AM_SHARE("dram2_8000")
				   AM_RANGE( 0xc8000, 0xcbfff ) AM_RAM AM_SHARE("dram2_c000")
				   AM_RANGE( 0xcc000, 0xcffff ) AM_RAM AM_SHARE("dram2_10000")

				   AM_RANGE( 0xd0000, 0xd3fff ) AM_RAM AM_SHARE("dram2_14000")
				   AM_RANGE( 0xd4000, 0xd7fff ) AM_RAM AM_SHARE("dram2_18000")
				   AM_RANGE( 0xd8000, 0xdbfff ) AM_RAM AM_SHARE("dram2_1c000")
				 */
				pspace.install_readwrite_bank(0xbc000, 0xbffff, 0x3fff, 0, "dram2_0000");
				pspace.install_readwrite_bank(0xc0000, 0xc3fff, 0x3fff, 0, "dram2_4000");
				pspace.install_readwrite_bank(0xc4000, 0xc7fff, 0x3fff, 0, "dram2_8000");
				pspace.install_readwrite_bank(0xc8000, 0xcbfff, 0x3fff, 0, "dram2_c000");
				pspace.install_readwrite_bank(0xcc000, 0xcffff, 0x3fff, 0, "dram2_10000");
				pspace.install_readwrite_bank(0xd0000, 0xd3fff, 0x3fff, 0, "dram2_14000");
				pspace.install_readwrite_bank(0xd4000, 0xd7fff, 0x3fff, 0, "dram2_18000");
				pspace.install_readwrite_bank(0xd8000, 0xdbfff, 0x3fff, 0, "dram2_1c000");

				/* data
				   AM_RANGE( 0x08000, 0x0bfff ) AM_RAM AM_SHARE("dram2_0000")
				   AM_RANGE( 0x0c000, 0x0ffff ) AM_RAM AM_SHARE("dram2_4000")

				   AM_RANGE( 0xbc000, 0xbffff ) AM_RAM AM_SHARE("dram2_0000")

				   AM_RANGE( 0xc0000, 0xc3fff ) AM_RAM AM_SHARE("dram2_4000")
				   AM_RANGE( 0xc4000, 0xc7fff ) AM_RAM AM_SHARE("dram2_8000")
				   AM_RANGE( 0xc8000, 0xcbfff ) AM_RAM AM_SHARE("dram2_c000")
				   AM_RANGE( 0xcc000, 0xcffff ) AM_RAM AM_SHARE("dram2_10000")

				   AM_RANGE( 0xd0000, 0xd3fff ) AM_RAM AM_SHARE("dram2_14000")
				   AM_RANGE( 0xd4000, 0xd7fff ) AM_RAM AM_SHARE("dram2_18000")
				   AM_RANGE( 0xd8000, 0xdbfff ) AM_RAM AM_SHARE("dram2_1c000")
				*/
				dspace.install_readwrite_bank(0x8000, 0xbfff, 0x3fff, 0, "dram2_0000");
				dspace.install_readwrite_bank(0xc000, 0xffff, 0x3fff, 0, "dram2_4000");
				dspace.install_readwrite_bank(0xbc000, 0xbffff, 0x3fff, 0, "dram2_0000");
				dspace.install_readwrite_bank(0xc0000, 0xc3fff, 0x3fff, 0, "dram2_4000");
				dspace.install_readwrite_bank(0xc4000, 0xc7fff, 0x3fff, 0, "dram2_8000");
				dspace.install_readwrite_bank(0xc8000, 0xcbfff, 0x3fff, 0, "dram2_c000");
				dspace.install_readwrite_bank(0xcc000, 0xcffff, 0x3fff, 0, "dram2_10000");
				dspace.install_readwrite_bank(0xd0000, 0xd3fff, 0x3fff, 0, "dram2_14000");
				dspace.install_readwrite_bank(0xd4000, 0xd7fff, 0x3fff, 0, "dram2_18000");
				dspace.install_readwrite_bank(0xd8000, 0xdbfff, 0x3fff, 0, "dram2_1c000");

				state->membank("dram2_0000")->set_base(memptr + 0x40000);
				state->membank("dram2_4000")->set_base(memptr + 0x44000);
				state->membank("dram2_8000")->set_base(memptr + 0x48000);
				state->membank("dram2_c000")->set_base(memptr + 0x4c000);
				state->membank("dram2_10000")->set_base(memptr + 0x50000);
				state->membank("dram2_14000")->set_base(memptr + 0x54000);
				state->membank("dram2_18000")->set_base(memptr + 0x58000);
				state->membank("dram2_1c000")->set_base(memptr + 0x5c000);
			}
			if (m_memsize > 384 * 1024) {
				/* DRAM3, 128K */

				/* prog
				   AM_RANGE( 0xdc000, 0xdffff ) AM_RAM AM_SHARE("dram3_0000")

				   AM_RANGE( 0xe0000, 0xe3fff ) AM_RAM AM_SHARE("dram3_4000")
				   AM_RANGE( 0xe4000, 0xe7fff ) AM_RAM AM_SHARE("dram3_8000")
				   AM_RANGE( 0xe8000, 0xebfff ) AM_RAM AM_SHARE("dram3_c000")
				   AM_RANGE( 0xec000, 0xeffff ) AM_RAM AM_SHARE("dram3_10000")

				   AM_RANGE( 0xf0000, 0xf3fff ) AM_RAM AM_SHARE("dram3_14000")
				   AM_RANGE( 0xf4000, 0xf7fff ) AM_RAM AM_SHARE("dram3_18000")
				   AM_RANGE( 0xf8000, 0xfbfff ) AM_RAM AM_SHARE("dram3_1c000")
				   AM_RANGE( 0xfc000, 0xfffff ) AM_RAM AM_SHARE("dram3_0000")
				*/
				pspace.install_readwrite_bank(0xdc000, 0xdffff, 0x3fff, 0, "dram3_0000");
				pspace.install_readwrite_bank(0xe0000, 0xe3fff, 0x3fff, 0, "dram3_4000");
				pspace.install_readwrite_bank(0xe4000, 0xe7fff, 0x3fff, 0, "dram3_8000");
				pspace.install_readwrite_bank(0xe8000, 0xebfff, 0x3fff, 0, "dram3_c000");
				pspace.install_readwrite_bank(0xec000, 0xeffff, 0x3fff, 0, "dram3_10000");
				pspace.install_readwrite_bank(0xf0000, 0xf3fff, 0x3fff, 0, "dram3_14000");
				pspace.install_readwrite_bank(0xf4000, 0xf7fff, 0x3fff, 0, "dram3_18000");
				pspace.install_readwrite_bank(0xf8000, 0xfbfff, 0x3fff, 0, "dram3_1c000");
				pspace.install_readwrite_bank(0xfc000, 0xfffff, 0x3fff, 0, "dram3_0000");

				/* data
				   AM_RANGE( 0x44000, 0x47fff ) AM_RAM AM_SHARE("dram3_0000")
				   AM_RANGE( 0x48000, 0x4bfff ) AM_RAM AM_SHARE("dram3_4000")
				   AM_RANGE( 0xdc000, 0xdffff ) AM_RAM AM_SHARE("dram3_0000")

				   AM_RANGE( 0xe0000, 0xe3fff ) AM_RAM AM_SHARE("dram3_4000")
				   AM_RANGE( 0xe4000, 0xe7fff ) AM_RAM AM_SHARE("dram3_8000")
				   AM_RANGE( 0xe8000, 0xebfff ) AM_RAM AM_SHARE("dram3_c000")
				   AM_RANGE( 0xec000, 0xeffff ) AM_RAM AM_SHARE("dram3_10000")

				   AM_RANGE( 0xf0000, 0xf3fff ) AM_RAM AM_SHARE("dram3_14000")
				   AM_RANGE( 0xf4000, 0xf7fff ) AM_RAM AM_SHARE("dram3_18000")
				   AM_RANGE( 0xf8000, 0xfbfff ) AM_RAM AM_SHARE("dram3_1c000")
				   AM_RANGE( 0xfc000, 0xfffff ) AM_RAM AM_SHARE("dram3_0000")
				*/
				dspace.install_readwrite_bank(0x44000, 0x47fff, 0x3fff, 0, "dram3_0000");
				dspace.install_readwrite_bank(0x48000, 0x4bfff, 0x3fff, 0, "dram3_4000");
				dspace.install_readwrite_bank(0xdc000, 0xdffff, 0x3fff, 0, "dram3_0000");
				dspace.install_readwrite_bank(0xe0000, 0xe3fff, 0x3fff, 0, "dram3_4000");
				dspace.install_readwrite_bank(0xe4000, 0xe7fff, 0x3fff, 0, "dram3_8000");
				dspace.install_readwrite_bank(0xe8000, 0xebfff, 0x3fff, 0, "dram3_c000");
				dspace.install_readwrite_bank(0xec000, 0xeffff, 0x3fff, 0, "dram3_10000");
				dspace.install_readwrite_bank(0xf0000, 0xf3fff, 0x3fff, 0, "dram3_14000");
				dspace.install_readwrite_bank(0xf4000, 0xf7fff, 0x3fff, 0, "dram3_18000");
				dspace.install_readwrite_bank(0xf8000, 0xfbfff, 0x3fff, 0, "dram3_1c000");
				dspace.install_readwrite_bank(0xfc000, 0xfffff, 0x3fff, 0, "dram3_0000");

				state->membank("dram3_0000")->set_base(memptr + 0x60000);
				state->membank("dram3_4000")->set_base(memptr + 0x64000);
				state->membank("dram3_8000")->set_base(memptr + 0x68000);
				state->membank("dram3_c000")->set_base(memptr + 0x6c000);
				state->membank("dram3_10000")->set_base(memptr + 0x70000);
				state->membank("dram3_14000")->set_base(memptr + 0x74000);
				state->membank("dram3_18000")->set_base(memptr + 0x78000);
				state->membank("dram3_1c000")->set_base(memptr + 0x7c000);
			}
		}
	}
}

static ADDRESS_MAP_START(m20_io, AS_IO, 16, m20_state)
	ADDRESS_MAP_UNMAP_HIGH

	AM_RANGE(0x00, 0x07) AM_DEVREADWRITE8("fd1797", fd1797_t, read, write, 0x00ff)

	AM_RANGE(0x20, 0x21) AM_READWRITE(port21_r, port21_w);

	AM_RANGE(0x60, 0x61) AM_DEVWRITE8("crtc", mc6845_device, address_w, 0x00ff)
	AM_RANGE(0x62, 0x63) AM_DEVREADWRITE8("crtc", mc6845_device, register_r, register_w, 0x00ff)

	AM_RANGE(0x80, 0x87) AM_DEVREADWRITE8("ppi8255", i8255_device, read, write, 0x00ff)

	AM_RANGE(0xa0, 0xa1) AM_DEVREADWRITE8("i8251_1", i8251_device, data_r, data_w, 0x00ff)
	AM_RANGE(0xa2, 0xa3) AM_DEVREADWRITE8("i8251_1", i8251_device, status_r, control_w, 0x00ff)

	AM_RANGE(0xc0, 0xc1) AM_DEVREADWRITE8("i8251_2", i8251_device, data_r, data_w, 0x00ff)
	AM_RANGE(0xc2, 0xc3) AM_DEVREADWRITE8("i8251_2", i8251_device, status_r, control_w, 0x00ff)

	AM_RANGE(0x120, 0x127) AM_DEVREADWRITE8("pit8253", pit8253_device, read, write, 0x00ff)

	AM_RANGE(0x140, 0x143) AM_READWRITE(m20_i8259_r, m20_i8259_w)

ADDRESS_MAP_END

#if 0
static ADDRESS_MAP_START(m20_apb_mem, AS_PROGRAM, 16, m20_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE( 0x00000, 0x007ff ) AM_RAM
	AM_RANGE( 0xf0000, 0xf7fff ) AM_RAM //mirrored?
	AM_RANGE( 0xfc000, 0xfffff ) AM_ROM AM_REGION("apb_bios",0)
ADDRESS_MAP_END

static ADDRESS_MAP_START(m20_apb_io, AS_IO, 16, m20_state)
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff) // may not be needed
	//0x4060 crtc address
	//0x4062 crtc data
ADDRESS_MAP_END
#endif

static INPUT_PORTS_START( m20 )
INPUT_PORTS_END

DRIVER_INIT_MEMBER(m20_state,m20)
{
}

IRQ_CALLBACK_MEMBER(m20_state::m20_irq_callback)
{
	if (! irqline)
		return 0xff; // NVI, value ignored
	else
		return m_i8259->acknowledge();
}

void m20_state::machine_start()
{
	m_fd1797->setup_intrq_cb(fd1797_t::line_cb(FUNC(m20_state::fdc_intrq_w), this));

	install_memory();
}

void m20_state::machine_reset()
{
	UINT8 *ROM = memregion("maincpu")->base();
	UINT8 *RAM = (UINT8 *)(m_ram->pointer() + 0x4000);

	if (m_memsize >= 256 * 1024)
		m_port21 = 0xdf;
	else
		m_port21 = 0xff;

	m_maincpu->set_irq_acknowledge_callback(device_irq_acknowledge_delegate(FUNC(m20_state::m20_irq_callback),this));

	m_fd1797->reset();

	memcpy(RAM, ROM, 8);  // we need only the reset vector
	m_maincpu->reset();     // reset the CPU to ensure it picks up the new vector
}


static MC6845_INTERFACE( mc6845_intf )
{
	false,      /* show border area */
	0,0,0,0,    /* visarea adjustment */
	16,         /* number of pixels per video memory address */
	NULL,       /* before pixel update callback */
	NULL,       /* row update callback */
	NULL,       /* after pixel update callback */
	DEVCB_NULL, /* callback for display state changes */
	DEVCB_NULL, /* callback for cursor state changes */
	DEVCB_NULL, /* HSYNC callback */
	DEVCB_NULL, /* VSYNC callback */
	NULL        /* update address callback */
};

static I8255A_INTERFACE( ppi_interface )
{
	DEVCB_NULL,     // port A read
	DEVCB_NULL,     // port A write
	DEVCB_NULL,     // port B read
	DEVCB_NULL,     // port B write
	DEVCB_NULL,     // port C read
	DEVCB_NULL      // port C write
};

WRITE_LINE_MEMBER(m20_state::kbd_rxrdy_int)
{
	m_i8259->ir4_w(state);
}

void m20_state::fdc_intrq_w(bool state)
{
	m_i8259->ir0_w(state);
}

static const i8251_interface kbd_i8251_intf =
{
	DEVCB_DRIVER_LINE_MEMBER(m20_state, kbd_tx),
	DEVCB_NULL,         // dtr
	DEVCB_NULL,         // rts
	DEVCB_DRIVER_LINE_MEMBER(m20_state, kbd_rxrdy_int),  // rx ready
	DEVCB_NULL,         // tx ready
	DEVCB_NULL,         // tx empty
	DEVCB_NULL          // syndet
};

static const i8251_interface tty_i8251_intf =
{
	DEVCB_NULL,         // txd out
	DEVCB_NULL,         // dtr
	DEVCB_NULL,         // rts
	DEVCB_NULL,         // rx ready
	DEVCB_NULL,         // tx ready
	DEVCB_NULL,         // tx empty
	DEVCB_NULL          // syndet
};

static unsigned char kbxlat[] =
{
	0x00, '\\', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o',   'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4',   '5', '6', '7', '8', '9', '-', '^', '@', '[', ';', ':', ']', ',', '.', '/',
	0x00,  '<', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O',   'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_', '!', '"', '#',
	'$',   '%', '&', '\'','(', ')', '=', 'x', 'x', '{', '+', '*', '}'
};

WRITE8_MEMBER( m20_state::kbd_put )
{
	if (data) {
		if (data == 0xd) data = 0xc1;
		else if (data == 0x20) data = 0xc0;
		else if (data == 8) data = 0x69; /* ^H */
		else if (data == 3) data = 0x64; /* ^C */
		else {
			int i;
			for (i = 0; i < sizeof(kbxlat); i++)
				if (data == kbxlat[i]) {
					data = i;
					break;
				}
		}
		printf("kbd_put called with 0x%02X\n", data);
		m_kbdi8251->receive_character(data);
	}
}

static ASCII_KEYBOARD_INTERFACE( keyboard_intf )
{
	DEVCB_DRIVER_MEMBER(m20_state, kbd_put)
};

const struct pit8253_interface pit8253_intf =
{
	{
		{
			1230782,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(m20_state, tty_clock_tick_w)
		},
		{
			1230782,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(m20_state, kbd_clock_tick_w)
		},
		{
			1230782,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(m20_state, timer_tick_w)
		}
	}
};

static SLOT_INTERFACE_START( m20_floppies )
	SLOT_INTERFACE( "5dd", FLOPPY_525_DD )
SLOT_INTERFACE_END

FLOPPY_FORMATS_MEMBER( m20_state::floppy_formats )
	FLOPPY_M20_FORMAT
FLOPPY_FORMATS_END

static MACHINE_CONFIG_START( m20, m20_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z8001, MAIN_CLOCK)
	MCFG_CPU_PROGRAM_MAP(m20_program_mem)
	MCFG_CPU_DATA_MAP(m20_data_mem)
	MCFG_CPU_IO_MAP(m20_io)

	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("160K")
	MCFG_RAM_DEFAULT_VALUE(0)
	MCFG_RAM_EXTRA_OPTIONS("128K,192K,224K,256K,384K,512K")

#if 0
	MCFG_CPU_ADD("apb", I8086, MAIN_CLOCK)
	MCFG_CPU_PROGRAM_MAP(m20_apb_mem)
	MCFG_CPU_IO_MAP(m20_apb_io)
	MCFG_DEVICE_DISABLE()
#endif

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_SIZE(512, 256)
	MCFG_SCREEN_VISIBLE_AREA(0, 512-1, 0, 256-1)
	MCFG_SCREEN_UPDATE_DRIVER(m20_state, screen_update_m20)
	MCFG_PALETTE_LENGTH(2)
	MCFG_PALETTE_INIT_OVERRIDE(driver_device, black_and_white)

	/* Devices */
	MCFG_FD1797x_ADD("fd1797", 1000000)
	MCFG_FLOPPY_DRIVE_ADD("fd1797:0", m20_floppies, "5dd", m20_state::floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("fd1797:1", m20_floppies, "5dd", m20_state::floppy_formats)
	MCFG_MC6845_ADD("crtc", MC6845, "screen", PIXEL_CLOCK/8, mc6845_intf) /* hand tuned to get ~50 fps */
	MCFG_I8255A_ADD("ppi8255",  ppi_interface)
	MCFG_I8251_ADD("i8251_1", kbd_i8251_intf)
	MCFG_I8251_ADD("i8251_2", tty_i8251_intf)
	MCFG_PIT8253_ADD("pit8253", pit8253_intf)
	MCFG_PIC8259_ADD("i8259", WRITELINE(m20_state, pic_irq_line_w), VCC, NULL)

	MCFG_ASCII_KEYBOARD_ADD(KEYBOARD_TAG, keyboard_intf)

	MCFG_SOFTWARE_LIST_ADD("flop_list","m20")
MACHINE_CONFIG_END

ROM_START(m20)
	ROM_REGION(0x2000,"maincpu", 0)
	ROM_SYSTEM_BIOS( 0, "m20", "M20 1.0" )
	ROMX_LOAD("m20.bin", 0x0000, 0x2000, CRC(5c93d931) SHA1(d51025e087a94c55529d7ee8fd18ff4c46d93230), ROM_BIOS(1))
	ROM_SYSTEM_BIOS( 1, "m20-20d", "M20 2.0d" )
	ROMX_LOAD("m20-20d.bin", 0x0000, 0x2000, CRC(cbe265a6) SHA1(c7cb9d9900b7b5014fcf1ceb2e45a66a91c564d0), ROM_BIOS(2))
	ROM_SYSTEM_BIOS( 2, "m20-20f", "M20 2.0f" )
	ROMX_LOAD("m20-20f.bin", 0x0000, 0x2000, CRC(db7198d8) SHA1(149d8513867081d31c73c2965dabb36d5f308041), ROM_BIOS(3))
ROM_END

ROM_START(m40)
	ROM_REGION(0x14000,"maincpu", 0)
	ROM_SYSTEM_BIOS( 0, "m40-81", "M40 15.dec.81" )
	ROMX_LOAD( "m40rom-15-dec-81", 0x0000, 0x2000, CRC(e8e7df84) SHA1(e86018043bf5a23ff63434f9beef7ce2972d8153), ROM_BIOS(1))
	ROM_SYSTEM_BIOS( 1, "m40-82", "M40 17.dec.82" )
	ROMX_LOAD( "m40rom-17-dec-82", 0x0000, 0x2000, CRC(cf55681c) SHA1(fe4ae14a6751fef5d7bde49439286f1da3689437), ROM_BIOS(2))
	ROM_SYSTEM_BIOS( 2, "m40-41", "M40 4.1" )
	ROMX_LOAD( "m40rom-4.1", 0x0000, 0x2000, CRC(cf55681c) SHA1(fe4ae14a6751fef5d7bde49439286f1da3689437), ROM_BIOS(3))
	ROM_SYSTEM_BIOS( 3, "m40-60", "M40 6.0" )
	ROMX_LOAD( "m40rom-6.0", 0x0000, 0x4000, CRC(8114ebec) SHA1(4e2c65b95718c77a87dbee0288f323bd1c8837a3), ROM_BIOS(4))

	ROM_REGION(0x4000, "apb_bios", ROMREGION_ERASEFF) // Processor board with 8086
ROM_END

/*    YEAR  NAME   PARENT  COMPAT  MACHINE INPUT   INIT COMPANY     FULLNAME        FLAGS */
COMP( 1981, m20,   0,      0,      m20,    m20, m20_state,    m20,  "Olivetti", "Olivetti L1 M20", GAME_NOT_WORKING | GAME_NO_SOUND)
COMP( 1981, m40,   m20,    0,      m20,    m20, m20_state,    m20, "Olivetti", "Olivetti L1 M40", GAME_NOT_WORKING | GAME_NO_SOUND)
