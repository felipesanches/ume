/***************************************************************************

    ACT Apricot PC/Xi

    license: MAME, GPL-2.0+
    copyright-holders: Dirk Best

    - Error 29 (timer failed)
    - Dump of the keyboard MCU ROM needed (can be dumped using test mode)

***************************************************************************/

#include "emu.h"
#include "cpu/i86/i86.h"
#include "cpu/i8089/i8089.h"
#include "machine/ram.h"
#include "machine/pit8253.h"
#include "machine/i8255.h"
#include "machine/pic8259.h"
#include "machine/z80dart.h"
#include "machine/serial.h"
#include "bus/centronics/ctronics.h"
#include "machine/wd_fdc.h"
#include "video/mc6845.h"
#include "sound/sn76496.h"
#include "imagedev/flopdrv.h"
#include "formats/apridisk.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class apricot_state : public driver_device
{
public:
	apricot_state(const machine_config &mconfig, device_type type, const char *tag) :
	driver_device(mconfig, type, tag),
	m_cpu(*this, "ic91"),
	m_ram(*this, RAM_TAG),
	m_iop(*this, "ic71"),
	m_sn(*this, "ic7"),
	m_crtc(*this, "ic30"),
	m_ppi(*this, "ic17"),
	m_pic(*this, "ic31"),
	m_pit(*this, "ic16"),
	m_sio(*this, "ic15"),
	m_rs232(*this, "rs232"),
	m_centronics(*this, "centronics"),
	m_fdc(*this, "ic68"),
	m_floppy0(*this, "ic68:0"),
	m_floppy1(*this, "ic68:1"),
	m_screen_buffer(*this, "screen_buffer"),
	m_data_selector_dtr(1),
	m_data_selector_rts(1),
	m_video_mode(0),
	m_display_on(1),
	m_display_enabled(0)
	{ }

	required_device<cpu_device> m_cpu;
	required_device<ram_device> m_ram;
	required_device<i8089_device> m_iop;
	required_device<sn76489_device> m_sn;
	required_device<mc6845_device> m_crtc;
	required_device<i8255_device> m_ppi;
	required_device<pic8259_device> m_pic;
	required_device<pit8253_device> m_pit;
	required_device<z80sio0_device> m_sio;
	required_device<rs232_port_device> m_rs232;
	required_device<centronics_device> m_centronics;
	required_device<wd2793_t> m_fdc;
	required_device<floppy_connector> m_floppy0;
	required_device<floppy_connector> m_floppy1;

	required_shared_ptr<UINT16> m_screen_buffer;

	DECLARE_WRITE8_MEMBER( i8089_ca1_w );
	DECLARE_WRITE8_MEMBER( i8089_ca2_w );
	DECLARE_READ8_MEMBER( apricot_sysctrl_r );
	DECLARE_WRITE8_MEMBER( apricot_sysctrl_w );
	DECLARE_WRITE_LINE_MEMBER( timer_out1 );
	DECLARE_WRITE_LINE_MEMBER( timer_out2 );
	void wd2793_intrq_w(bool state);
	void wd2793_drq_w(bool state);
	DECLARE_WRITE_LINE_MEMBER( apricot_mc6845_de ) { m_display_enabled = state; };

	DECLARE_WRITE_LINE_MEMBER( data_selector_dtr_w ) { m_data_selector_dtr = state; };
	DECLARE_WRITE_LINE_MEMBER( data_selector_rts_w ) { m_data_selector_rts = state; };

	virtual void machine_start();

	IRQ_CALLBACK_MEMBER( irq_callback ) { return m_pic->inta_r(); }

	int m_data_selector_dtr;
	int m_data_selector_rts;

	bool m_video_mode;
	bool m_display_on;

	int m_display_enabled;

	UINT32 screen_update_apricot(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
};


//**************************************************************************
//  I/O
//**************************************************************************

WRITE8_MEMBER( apricot_state::i8089_ca1_w )
{
	m_iop->sel_w(0);
	m_iop->ca_w(1);
	m_iop->ca_w(0);
}

WRITE8_MEMBER( apricot_state::i8089_ca2_w )
{
	m_iop->sel_w(1);
	m_iop->ca_w(1);
	m_iop->ca_w(0);
}

READ8_MEMBER( apricot_state::apricot_sysctrl_r )
{
	UINT8 data = 0;

	data |= m_display_enabled << 3;

	return data;
}

WRITE8_MEMBER( apricot_state::apricot_sysctrl_w )
{
	m_display_on = BIT(data, 3);
	m_video_mode = BIT(data, 4);

	if (!BIT(data, 5))
		m_fdc->set_floppy(BIT(data, 6) ? m_floppy1->get_device() : m_floppy0->get_device());

	// switch video modes
	m_crtc->set_clock( m_video_mode ? XTAL_15MHz / 10 : XTAL_15MHz / 16);
	m_crtc->set_hpixels_per_column( m_video_mode ? 10 : 16);
}

static const i8255_interface apricot_i8255a_intf =
{
	DEVCB_DEVICE_MEMBER("centronics", centronics_device, read),
	DEVCB_DEVICE_MEMBER("centronics", centronics_device, write),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(apricot_state, apricot_sysctrl_w),
	DEVCB_DRIVER_MEMBER(apricot_state, apricot_sysctrl_r),
	DEVCB_NULL
};

WRITE_LINE_MEMBER( apricot_state::timer_out1 )
{
	// receive clock via timer 1
	if (m_data_selector_rts == 0 && m_data_selector_dtr == 0)
		m_sio->rxca_w(state);
}

WRITE_LINE_MEMBER( apricot_state::timer_out2 )
{
	// transmit clock via timer 2
	if (m_data_selector_rts == 0 && m_data_selector_dtr == 0)
		m_sio->txca_w(state);

	// transmit and receive clock via timer 2
	if (m_data_selector_rts == 1 && m_data_selector_dtr == 0)
	{
		m_sio->txca_w(state);
		m_sio->rxca_w(state);
	}
}

static const struct pit8253_interface apricot_pit8253_intf =
{
	{
		{ XTAL_4MHz / 16, DEVCB_LINE_VCC, DEVCB_DEVICE_LINE_MEMBER("ic31", pic8259_device, ir6_w) },
		{ XTAL_4MHz / 2,  DEVCB_LINE_VCC, DEVCB_DRIVER_LINE_MEMBER(apricot_state, timer_out1) },
		{ XTAL_4MHz / 2,  DEVCB_LINE_VCC, DEVCB_DRIVER_LINE_MEMBER(apricot_state, timer_out2) }
	}
};

static Z80SIO_INTERFACE( apricot_z80sio_intf )
{
	0, 0,
	XTAL_4MHz / 16, XTAL_4MHz / 16,

	// channel a
	DEVCB_DEVICE_LINE_MEMBER("rs232", serial_port_device, tx),
	DEVCB_DEVICE_LINE_MEMBER("rs232", rs232_port_device, dtr_w),
	DEVCB_DEVICE_LINE_MEMBER("rs232", rs232_port_device, rts_w),
	DEVCB_DEVICE_LINE_MEMBER("ic71", i8089_device, drq2_w),
	DEVCB_NULL,

	// channel b
	DEVCB_NULL,
	DEVCB_DRIVER_LINE_MEMBER(apricot_state, data_selector_dtr_w),
	DEVCB_DRIVER_LINE_MEMBER(apricot_state, data_selector_rts_w),
	DEVCB_NULL,
	DEVCB_NULL,

	DEVCB_DEVICE_LINE_MEMBER("ic31", pic8259_device, ir5_w),
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL
};

// note: fault output should be connected to syncb input of the sio
static const centronics_interface apricot_centronics_intf =
{
	DEVCB_DEVICE_LINE_MEMBER("ic15", z80dart_device, ctsb_w),
	DEVCB_DEVICE_LINE_MEMBER("ic15", z80dart_device, dcdb_w),
	DEVCB_NULL
};


//**************************************************************************
//  FLOPPY
//**************************************************************************

void apricot_state::wd2793_intrq_w(bool state)
{
	m_pic->ir4_w(state);
	m_iop->ext1_w(state);
}

void apricot_state::wd2793_drq_w(bool state)
{
	m_iop->drq1_w(state);
}

static SLOT_INTERFACE_START( apricot_floppies )
	SLOT_INTERFACE( "d31v", SONY_OA_D31V )
	SLOT_INTERFACE( "d32w", SONY_OA_D32W )
SLOT_INTERFACE_END


//**************************************************************************
//  VIDEO EMULATION
//**************************************************************************

UINT32 apricot_state::screen_update_apricot(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (!m_display_on)
		m_crtc->screen_update(screen, bitmap, cliprect);
	else
		bitmap.fill(RGB_BLACK, cliprect);

	return 0;
}

static MC6845_UPDATE_ROW( apricot_update_row )
{
	apricot_state *state = device->machine().driver_data<apricot_state>();
	UINT8 *ram = state->m_ram->pointer();
	int i, x;

	if (state->m_video_mode)
	{
		// text mode
		for (i = 0; i < x_count; i++)
		{
			UINT16 code = state->m_screen_buffer[(ma + i) & 0x7ff];
			UINT16 offset = ((code & 0x7ff) << 5) | (ra << 1);
			UINT16 data = ram[offset + 1] << 8 | ram[offset];
			int fill = 0;

			if (BIT(code, 12) && BIT(data, 14)) fill = 1; // strike-through?
			if (BIT(code, 13) && BIT(data, 15)) fill = 1; // underline?

			// draw 10 pixels of the character
			for (x = 0; x <= 10; x++)
			{
				int color = fill ? 1 : BIT(data, x);
				if (BIT(code, 15)) color = !color; // reverse?
				bitmap.pix32(y, x + i*10) = RGB_MONOCHROME_GREEN_HIGHLIGHT[color ? 1 + BIT(code, 14) : 0];
			}
		}
	}
	else
	{
		// graphics mode
		fatalerror("Graphics mode not implemented!\n");
	}
}

static MC6845_INTERFACE( apricot_mc6845_intf )
{
	false,
	0,0,0,0,
	10,
	NULL,
	apricot_update_row,
	NULL,
	DEVCB_DRIVER_LINE_MEMBER(apricot_state, apricot_mc6845_de),
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	NULL
};


//**************************************************************************
//  MACHINE EMULATION
//**************************************************************************

void apricot_state::machine_start()
{
	// install shared memory to the main cpu and the iop
	m_cpu->space(AS_PROGRAM).install_ram(0x00000, m_ram->size() - 1, m_ram->pointer());
	m_iop->space(AS_PROGRAM).install_ram(0x00000, m_ram->size() - 1, m_ram->pointer());

	// setup interrupt acknowledge callback for the main cpu
	m_cpu->set_irq_acknowledge_callback(device_irq_acknowledge_delegate(FUNC(apricot_state::irq_callback), this));

	// setup floppy disk controller callbacks
	m_fdc->setup_intrq_cb(wd2793_t::line_cb(FUNC(apricot_state::wd2793_intrq_w), this));
	m_fdc->setup_drq_cb(wd2793_t::line_cb(FUNC(apricot_state::wd2793_drq_w), this));

	// motor on is connected to gnd
	m_floppy0->get_device()->mon_w(0);
	m_floppy1->get_device()->mon_w(0);
}


//**************************************************************************
//  ADDRESS MAPS
//**************************************************************************

static ADDRESS_MAP_START( apricot_mem, AS_PROGRAM, 16, apricot_state )
//  AM_RANGE(0x00000, 0x3ffff) AM_RAMBANK("standard_ram")
//  AM_RANGE(0x40000, 0xeffff) AM_RAMBANK("expansion_ram")
	AM_RANGE(0xf0000, 0xf0fff) AM_MIRROR(0x7000) AM_RAM AM_SHARE("screen_buffer")
	AM_RANGE(0xfc000, 0xfffff) AM_MIRROR(0x4000) AM_ROM AM_REGION("bootstrap", 0)
ADDRESS_MAP_END

static ADDRESS_MAP_START( apricot_io, AS_IO, 16, apricot_state )
	AM_RANGE(0x00, 0x03) AM_DEVREADWRITE8("ic31", pic8259_device, read, write, 0x00ff)
	AM_RANGE(0x40, 0x47) AM_DEVREADWRITE8("ic68", wd2793_t, read, write, 0x00ff)
	AM_RANGE(0x48, 0x4f) AM_DEVREADWRITE8("ic17", i8255_device, read, write, 0x00ff)
	AM_RANGE(0x50, 0x51) AM_MIRROR(0x06) AM_DEVWRITE8("ic7", sn76489_device, write, 0x00ff)
	AM_RANGE(0x58, 0x5f) AM_DEVREADWRITE8("ic16", pit8253_device, read, write, 0x00ff)
	AM_RANGE(0x60, 0x67) AM_DEVREADWRITE8("ic15", z80sio0_device, ba_cd_r, ba_cd_w, 0x00ff)
	AM_RANGE(0x68, 0x69) AM_MIRROR(0x04) AM_DEVWRITE8("ic30", mc6845_device, address_w, 0x00ff)
	AM_RANGE(0x6a, 0x6b) AM_MIRROR(0x04) AM_DEVREADWRITE8("ic30", mc6845_device, register_r, register_w, 0x00ff)
	AM_RANGE(0x70, 0x71) AM_MIRROR(0x04) AM_WRITE8(i8089_ca1_w, 0x00ff)
	AM_RANGE(0x72, 0x73) AM_MIRROR(0x04) AM_WRITE8(i8089_ca2_w, 0x00ff)
	AM_RANGE(0x78, 0x7f) AM_NOP // unavailable
ADDRESS_MAP_END


//**************************************************************************
//  MACHINE DRIVERS
//**************************************************************************

static const sn76496_config psg_intf =
{
	DEVCB_NULL
};

static MACHINE_CONFIG_START( apricot, apricot_state )
	// main cpu
	MCFG_CPU_ADD("ic91", I8086, XTAL_15MHz / 3)
	MCFG_CPU_PROGRAM_MAP(apricot_mem)
	MCFG_CPU_IO_MAP(apricot_io)

	// i/o cpu
	MCFG_CPU_ADD("ic71", I8089, XTAL_15MHz / 3)
	MCFG_CPU_PROGRAM_MAP(apricot_mem)
	MCFG_CPU_IO_MAP(apricot_io)
	MCFG_I8089_DATABUS_WIDTH(16)
	MCFG_I8089_SINTR1(DEVWRITELINE("ic31", pic8259_device, ir0_w))
	MCFG_I8089_SINTR2(DEVWRITELINE("ic31", pic8259_device, ir1_w))

	// video hardware
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_SIZE(800, 400)
	MCFG_SCREEN_VISIBLE_AREA(0, 800-1, 0, 400-1)
	MCFG_SCREEN_REFRESH_RATE(72)
	MCFG_SCREEN_UPDATE_DRIVER(apricot_state, screen_update_apricot)

	MCFG_MC6845_ADD("ic30", MC6845, "screen", XTAL_15MHz / 10, apricot_mc6845_intf)

	// sound hardware
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("ic7", SN76489, XTAL_4MHz / 2)
	MCFG_SOUND_CONFIG(psg_intf)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)

	// internal ram
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("256k")
	MCFG_RAM_EXTRA_OPTIONS("384k,512k") // with 1 or 2 128k expansion boards

	// devices
	MCFG_I8255A_ADD("ic17", apricot_i8255a_intf)
	MCFG_PIC8259_ADD("ic31", INPUTLINE("ic91", 0), VCC, NULL)
	MCFG_PIT8253_ADD("ic16", apricot_pit8253_intf)
	MCFG_Z80SIO0_ADD("ic15", XTAL_15MHz / 6, apricot_z80sio_intf)

	// rs232 port
	MCFG_RS232_PORT_ADD("rs232", default_rs232_devices, NULL)
// note: missing a receive clock callback to support external clock mode
// (m_data_selector_rts == 1 and m_data_selector_dtr == 0)
	MCFG_SERIAL_OUT_RX_HANDLER(DEVWRITELINE("ic15", z80dart_device, rxa_w))
	MCFG_RS232_OUT_DCD_HANDLER(DEVWRITELINE("ic15", z80dart_device, dcda_w))
	MCFG_RS232_OUT_DSR_HANDLER(DEVWRITELINE("ic15", z80dart_device, synca_w))
	MCFG_RS232_OUT_CTS_HANDLER(DEVWRITELINE("ic15", z80dart_device, ctsa_w))

	// centronics printer
	MCFG_CENTRONICS_PRINTER_ADD("centronics", apricot_centronics_intf)

	// floppy
	MCFG_WD2793x_ADD("ic68", XTAL_4MHz / 2)
	MCFG_FLOPPY_DRIVE_ADD("ic68:0", apricot_floppies, "d32w", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("ic68:1", apricot_floppies, "d32w", floppy_image_device::default_floppy_formats)
MACHINE_CONFIG_END

static MACHINE_CONFIG_DERIVED( apricotxi, apricot )
MACHINE_CONFIG_END


//**************************************************************************
//  ROM DEFINITIONS
//**************************************************************************

ROM_START( apricot )
	ROM_REGION(0x4000, "bootstrap", 0)
	ROM_LOAD16_BYTE("pc_bios_lo_001.bin", 0x0000, 0x2000, CRC(0c217cc2) SHA1(0d7a2b61e17966462b555115f962a175fadcf72a))
	ROM_LOAD16_BYTE("pc_bios_hi_001.bin", 0x0001, 0x2000, CRC(7c27f36c) SHA1(c801bbf904815f76ec6463e948f57e0118a26292))
ROM_END

ROM_START( apricotxi )
	ROM_REGION(0x4000, "bootstrap", 0)
	ROM_LOAD16_BYTE("lo_ve007.u11", 0x0000, 0x2000, CRC(e74e14d1) SHA1(569133b0266ce3563b21ae36fa5727308797deee)) // LO Ve007 03.04.84
	ROM_LOAD16_BYTE("hi_ve007.u9",  0x0001, 0x2000, CRC(b04fb83e) SHA1(cc2b2392f1b4c04bb6ec8ee26f8122242c02e572)) // HI Ve007 03.04.84
ROM_END


//**************************************************************************
//  GAME DRIVERS
//**************************************************************************

//    YEAR  NAME       PARENT   COMPAT  MACHINE    INPUT  CLASS          INIT  COMPANY  FULLNAME      FLAGS
COMP( 1983, apricot,   0,       0,      apricot,   0,     driver_device, 0,    "ACT",   "Apricot PC", GAME_NOT_WORKING )
COMP( 1984, apricotxi, apricot, 0,      apricotxi, 0,     driver_device, 0,    "ACT",   "Apricot Xi", GAME_NOT_WORKING )
