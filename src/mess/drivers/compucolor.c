// license:BSD-3-Clause
// copyright-holders:Curt Coder
/*

    Compucolor II

    http://www.compucolor.org/index.html

*/

#define I8080_TAG	"ua2"
#define TMS5501_TAG	"uf1"
#define CRT5027_TAG	"uf9"

#include "emu.h"
#include "cpu/i8085/i8085.h"
#include "imagedev/floppy.h"
#include "machine/ram.h"
#include "machine/tms5501.h"
#include "video/tms9927.h"

class compucolor2_state : public driver_device
{
public:
	compucolor2_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		  m_maincpu(*this, I8080_TAG),
		  //m_mioc(*this, TMS5501_TAG),
		  m_vtac(*this, CRT5027_TAG),
		  m_char_rom(*this, "chargen"),
		  m_video_ram(*this, "videoram")
	{ }

	required_device<cpu_device> m_maincpu;
	//required_device<tms5501_device> m_mioc;
	required_device<crt5027_device> m_vtac;
	required_memory_region m_char_rom;
	required_shared_ptr<UINT8> m_video_ram;

	virtual void machine_start();
	virtual void palette_init();
	
	UINT32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	DECLARE_READ8_MEMBER( xi_r );
	DECLARE_WRITE8_MEMBER( xo_w );

	IRQ_CALLBACK_MEMBER( int_ack );

	rgb_t m_palette[8];
};

static ADDRESS_MAP_START( compucolor2_mem, AS_PROGRAM, 8, compucolor2_state )
	AM_RANGE(0x0000, 0x3fff) AM_ROM AM_REGION(I8080_TAG, 0)
	AM_RANGE(0x6000, 0x6fff) AM_MIRROR(0x1000) AM_RAM AM_SHARE("videoram")
	AM_RANGE(0x8000, 0xffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( compucolor2_io, AS_IO, 8, compucolor2_state )
	//AM_RANGE(0x00, 0x0f) AM_MIRROR(0x10) AM_DEVREADWRITE(TMS5501_TAG, tms5501_device, read, write)
	AM_RANGE(0x60, 0x6f) AM_MIRROR(0x10) AM_DEVREADWRITE(CRT5027_TAG, crt5027_device, read, write)
	AM_RANGE(0x80, 0x9f) AM_MIRROR(0x60) AM_ROM AM_REGION("crt", 0)
ADDRESS_MAP_END

static INPUT_PORTS_START( compucolor2 )
	PORT_START("Y0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y4")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y5")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y6")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y7")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y8")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y9")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y10")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y11")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y12")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y13")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y14")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )

	PORT_START("Y15")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD )
INPUT_PORTS_END

UINT32 compucolor2_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	for (int y = 0; y < 32*8; y++)
	{
		offs_t offset = (y / 8) * 128;
		bool dbl = false;

		for (int sx = 0; sx < 64; sx++)
		{
			UINT8 code = m_video_ram[offset++];
			
			if (sx == 0 && BIT(code, 7))
			{
				dbl = true;
			}

			offs_t char_offs = (code << 3) | (y & 0x07);
			if (dbl) char_offs = (code << 3) | ((y >> 1) & 0x07);

			UINT8 data = m_char_rom->base()[char_offs];
			UINT8 attr = m_video_ram[offset++];
			
			rgb_t fg = m_palette[attr & 0x07];
			rgb_t bg = m_palette[(attr >> 3) & 0x07];

			for (int x = 0; x < 6; x++)
			{
				bitmap.pix32(y, (sx * 8) + x) = BIT(data, 7) ? fg : bg;

				data <<= 1;
			}
		}
	}

	return 0;
}

static struct tms9927_interface crtc_intf =
{
	8,      // pixels per video memory address
	NULL    // "self-load data"?
};

READ8_MEMBER( compucolor2_state::xi_r )
{
	return 0;
}

WRITE8_MEMBER( compucolor2_state::xo_w )
{

}

IRQ_CALLBACK_MEMBER( compucolor2_state::int_ack )
{
	return 0;//m_mioc->get_vector();
}

void compucolor2_state::machine_start()
{
	m_maincpu->set_irq_acknowledge_callback(device_irq_acknowledge_delegate(FUNC(compucolor2_state::int_ack), this));
}

void compucolor2_state::palette_init()
{
	for (int i = 0; i < 8; i++)
	{
		m_palette[i] = MAKE_RGB(BIT(i, 0) * 0xff, BIT(i, 1) * 0xff, BIT(i, 2) * 0xff);
	}
}

static SLOT_INTERFACE_START( compucolor2_floppies )
	SLOT_INTERFACE( "525sssd", FLOPPY_525_SSSD )
SLOT_INTERFACE_END

static MACHINE_CONFIG_START( compucolor2, compucolor2_state )
	// basic machine hardware
	MCFG_CPU_ADD(I8080_TAG, I8080, XTAL_17_9712MHz/9)
	MCFG_CPU_PROGRAM_MAP(compucolor2_mem)
	MCFG_CPU_IO_MAP(compucolor2_io)

	// video hardware
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(250))
	MCFG_SCREEN_UPDATE_DRIVER(compucolor2_state, screen_update)
	MCFG_SCREEN_SIZE(64*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0, 64*8-1, 0, 32*8-1)
	MCFG_DEVICE_ADD(CRT5027_TAG, CRT5027, XTAL_17_9712MHz/12)
	MCFG_DEVICE_CONFIG(crtc_intf)

	// devices
	MCFG_DEVICE_ADD(TMS5501_TAG, TMS5501, XTAL_17_9712MHz/9)
	/*MCFG_TMS5501_IRQ_CALLBACK(INPUTLINE(I8080_TAG, I8085_INTR_LINE))
	MCFG_TMS5501_XI_CALLBACK(READ8(compucolor2_state, xi_r))
	MCFG_TMS5501_XO_CALLBACK(WRITE8(compucolor2_state, xo_w))*/
	MCFG_FLOPPY_DRIVE_ADD(TMS5501_TAG":0", compucolor2_floppies, "525sssd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(TMS5501_TAG":1", compucolor2_floppies, NULL,      floppy_image_device::default_floppy_formats)

	// internal ram
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("32K")
	MCFG_RAM_EXTRA_OPTIONS("8K,16K")

	// software list
	MCFG_SOFTWARE_LIST_ADD("flop_list", "compclr2_flop")
MACHINE_CONFIG_END

ROM_START( compclr2 )
	ROM_REGION( 0x4000, I8080_TAG, 0 )
	ROM_SYSTEM_BIOS( 0, "678", "v6.78" )
	ROMX_LOAD( "v678.rom", 0x0000, 0x4000, BAD_DUMP CRC(5e559469) SHA1(fe308774aae1294c852fe24017e58d892d880cd3), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "879", "v8.79" )
	ROMX_LOAD( "v879.rom", 0x0000, 0x4000, BAD_DUMP CRC(4de8e652) SHA1(e5c55da3ac893b8a5a99c8795af3ca72b1645f3f), ROM_BIOS(2) )

	ROM_REGION( 0x800, "chargen", 0 )
	ROM_LOAD( "chargen.uf6", 0x000, 0x400, BAD_DUMP CRC(7eef135a) SHA1(be488ef32f54c6e5f551fb84ab12b881aef72dd9) )
	ROM_LOAD( "chargen.uf7", 0x400, 0x400, BAD_DUMP CRC(2bee7cf6) SHA1(808e0fc6f2332b4297de190eafcf84668703e2f4) )

	ROM_REGION( 0x20, "crt", 0 )
	ROM_LOAD( "82s123.ua1", 0x00, 0x20, BAD_DUMP CRC(27ae54bc) SHA1(ccb056fbc1ec2132f2602217af64d77237494afb) ) // I/O PROM

	ROM_REGION( 0x20, "proms", 0 )
	ROM_LOAD( "82s129.ue2", 0x00, 0x20, NO_DUMP ) // Address Decoder/Timer for RAM
	ROM_LOAD( "82s129.uf3", 0x00, 0x20, NO_DUMP ) // System Decoder
	ROM_LOAD( "82s123.ub3", 0x00, 0x20, NO_DUMP ) // ROM/PROM Decoder
	ROM_LOAD( "82s129.uf8", 0x00, 0x20, NO_DUMP ) // CPU & Horizontal Decoder
	ROM_LOAD( "82s129.ug9", 0x00, 0x20, NO_DUMP ) // Scan Decoder
	ROM_LOAD( "82s129.ug5", 0x00, 0x20, NO_DUMP ) // Color PROM
ROM_END

COMP( 1977, compclr2,    0,      0,      compucolor2,        compucolor2, driver_device, 0,      "Intelligent Systems Corporation",  "Compucolor II",  GAME_IS_SKELETON | GAME_NOT_WORKING | GAME_NO_SOUND )
