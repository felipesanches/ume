// license:BSD-3-Clause
// copyright-holders:Curt Coder
/*

Wave Mate Bullet

PCB Layout
----------

|-------------------------------------------|
|                           CN7     CN6     |
|                                   PIO     |
|                           FDC             |
|       4164 4164                   CN5     |
|       4164 4164       SW1              CN2|
|       4164 4164                           |
|       4164 4164                   PROM    |
|       4164 4164   4.9152MHz               |
|       4164 4164   16MHz   DMA     CPU     |
|       4164 4164                           |
|       4164 4164           CN4 CN3      CN1|
|                                           |
|                           DART    CTC     |
|                                   CN8     |
|-------------------------------------------|

Notes:
    Relevant IC's shown.

    CPU     - SGS Z80ACPUB1 Z80A CPU
    DMA     - Zilog Z8410APS Z80A DMA
    PIO     - SGS Z80APIOB1 Z80A PIO
    DART    - Zilog Z8470APS Z80A DART
    CTC     - Zilog Z8430APS Z80A CTC
    FDC     - Synertek SY1793-02 FDC
    PROM    - AMD AM27S190C 32x8 TTL PROM
    4164    - Fujitsu MB8264-15 64Kx1 RAM
    CN1     - 2x25 PCB header, external DMA bus
    CN2     - 2x17 PCB header, Winchester / 2x25 header, SCSI (in board revision E)
    CN3     - 2x5 PCB header, RS-232 A (system console)
    CN4     - 2x5 PCB header, RS-232 B
    CN5     - 2x17 PCB header, Centronics
    CN6     - 2x25 PCB header, 8" floppy drives
    CN7     - 2x17 PCB header, 5.25" floppy drives
    CN8     - 4-pin Molex

*/

/*

    TODO:

    - memory banking is broken
    - z80dart wait/ready
    - IMI 7710 Winchester controller
        chdman createhd -o imi7710.chd -chs 350,3,10 -ss 1024
    - revision E model

*/

#include "includes/bullet.h"



//**************************************************************************
//  MACROS / CONSTANTS
//**************************************************************************

// DMA ready sources
enum
{
	FDRDY = 0,
	DARTARDY,
	DARTBRDY,
	WINRDY,
	EXRDY1,
	EXRDY2
};

#define SEG0 \
	BIT(m_mbank, 0)

#define SEG5 \
	BIT(m_mbank, 5)

#define DMB4 \
	BIT(m_xdma0, 4)

#define DMB6 \
	BIT(m_xdma0, 6)



//**************************************************************************
//  READ/WRITE HANDLERS
//**************************************************************************

//-------------------------------------------------
//  mreq_r -
//-------------------------------------------------

READ8_MEMBER( bullet_state::mreq_r )
{
	UINT8 data = 0;

	if (!m_brom && !BIT(offset, 5))
	{
		data = m_rom->base()[offset & 0x1f];
	}
	else
	{
		if (offset < 0xc000)
		{
			data = m_ram->pointer()[(m_segst << 16) | offset];
		}
		else
		{
			data = m_ram->pointer()[offset];
		}
	}

	return data;
}


//-------------------------------------------------
//  mreq_w -
//-------------------------------------------------

WRITE8_MEMBER( bullet_state::mreq_w )
{
	if (offset < 0xc000)
	{
		m_ram->pointer()[(m_segst << 16) | offset] = data;
	}
	else
	{
		m_ram->pointer()[offset] = data;
	}
}


//-------------------------------------------------
//  win_r -
//-------------------------------------------------

READ8_MEMBER( bullet_state::win_r )
{
	return 0;
}


//-------------------------------------------------
//  wstrobe_w -
//-------------------------------------------------

WRITE8_MEMBER( bullet_state::wstrobe_w )
{
}


//-------------------------------------------------
//  brom_r -
//-------------------------------------------------

READ8_MEMBER( bullet_state::brom_r )
{
	m_brom = 1;

	return 0;
}


//-------------------------------------------------
//  brom_w -
//-------------------------------------------------

WRITE8_MEMBER( bullet_state::brom_w )
{
	m_brom = 1;
}


//-------------------------------------------------
//  exdsk_w -
//-------------------------------------------------

WRITE8_MEMBER( bullet_state::exdsk_w )
{
	/*

	    bit     signal      description

	    0                   drive select 0
	    1                   drive select 1
	    2                   select 8" floppy
	    3       MSE         select software control of port
	    4       SIDE        select side 2
	    5       _MOTOR      disable 5" floppy spindle motors
	    6
	    7       WPRC        enable write precompensation

	*/

	if (BIT(data, 3))
	{
		m_exdsk_sw = true;
	}

	if (m_exdsk_sw)
	{
		// drive select
		m_floppy = NULL;

		switch (data & 0x07)
		{
		// 5.25"
		case 0: m_floppy = m_floppy0->get_device(); break;
		case 1: m_floppy = m_floppy1->get_device(); break;
		case 2: m_floppy = m_floppy2->get_device(); break;
		case 3: m_floppy = m_floppy3->get_device(); break;
		// 8"
		case 4: m_floppy = m_floppy4->get_device(); break;
		case 5: m_floppy = m_floppy5->get_device(); break;
		case 6: m_floppy = m_floppy6->get_device(); break;
		case 7: m_floppy = m_floppy7->get_device(); break;
		}

		m_fdc->set_floppy(m_floppy);
	}

	if (m_floppy)
	{
		// side select
		m_floppy->ss_w(BIT(data, 4));

		// floppy motor
		m_floppy->mon_w(BIT(data, 5));
	}
}


//-------------------------------------------------
//  exdma_w -
//-------------------------------------------------

WRITE8_MEMBER( bullet_state::exdma_w )
{
	/*

	    bit     description

	    0       DMA ready source select 0
	    1       DMA ready source select 1
	    2       DMA ready source select 2
	    3       memory control 0
	    4       memory control 1
	    5
	    6
	    7

	*/

	m_exdma = data;

	m_buf = BIT(data, 3);

	update_dma_rdy();
}


//-------------------------------------------------
//  hdcon_w -
//-------------------------------------------------

WRITE8_MEMBER( bullet_state::hdcon_w )
{
	/*

	    bit     signal  description

	    0       PLO     phase lock oscillator
	    1       RCD     read clock frequency
	    2       EXC     MB8877 clock frequency
	    3       DEN     MB8877 density select
	    4               enable software control of mode
	    5
	    6
	    7

	*/

	if (BIT(data, 4))
	{
		m_hdcon_sw = true;
	}

	if (m_hdcon_sw)
	{
		// FDC clock
		m_fdc->set_unscaled_clock(BIT(data, 2) ? XTAL_16MHz/16 : XTAL_16MHz/8);

		// density select
		m_fdc->dden_w(BIT(data, 3));
	}
}


//-------------------------------------------------
//  info_r -
//-------------------------------------------------

READ8_MEMBER( bullet_state::info_r )
{
	/*

	    bit     signal      description

	    0       SW1         DIP switch 1
	    1       SW2         DIP switch 2
	    2       SW3         DIP switch 3
	    3       SW4         DIP switch 4
	    4       HLDST       floppy disk head load status
	    5       *XDCG       floppy disk exchange (8" only)
	    6       FDIRQ       FDC interrupt request line
	    7       FDDRQ       FDC data request line

	*/

	UINT8 data = 0;

	// DIP switches
	data |= m_sw1->read() & 0x0f;

	// floppy
	data |= m_fdc->hld_r() << 4;
	data |= (m_floppy ? m_floppy->dskchg_r() : 1) << 5;
	data |= m_fdc->intrq_r() << 6;
	data |= m_fdc->drq_r() << 7;

	return data;
}


//-------------------------------------------------
//  segst_w -
//-------------------------------------------------

WRITE8_MEMBER( bullet_state::segst_w )
{
	m_segst = BIT(data, 0);
}


//-------------------------------------------------
//  mreq_r -
//-------------------------------------------------

READ8_MEMBER( bulletf_state::mreq_r )
{
	UINT8 data = 0;

	if (!m_rome && !BIT(offset, 5))
	{
		data = m_rom->base()[offset & 0x1f];
	}
	else
	{
		if (offset < 0xc000)
		{
			if (!SEG5)
			{
				data = m_ram->pointer()[(SEG0 << 16) | offset];
			}
			else if (offset >= 0x8000)
			{
				data = m_ram->pointer()[0x1c000 + (offset - 0x8000)];
			}
		}
		else
		{
			data = m_ram->pointer()[offset];
		}
	}

	return data;
}


//-------------------------------------------------
//  mreq_w -
//-------------------------------------------------

WRITE8_MEMBER( bulletf_state::mreq_w )
{
	if (offset < 0xc000)
	{
		if (!SEG5)
		{
			m_ram->pointer()[(SEG0 << 16) | offset] = data;
		}
		else if (offset >= 0x8000)
		{
			m_ram->pointer()[0x1c000 + (offset - 0x8000)] = data;
		}
	}
	else
	{
		m_ram->pointer()[offset] = data;
	}
}


//-------------------------------------------------
//  xdma0_w -
//-------------------------------------------------

WRITE8_MEMBER( bulletf_state::xdma0_w )
{
	/*

	    bit     signal

	    0       device select (0=FDC, 1=SCSI)
	    1
	    2
	    3
	    4       DMB4        Source bank
	    5
	    6       DMB6        Destination bank
	    7

	*/

	m_rome = 1;

	m_xdma0 = data;
}


//-------------------------------------------------
//  xfdc_w -
//-------------------------------------------------

WRITE8_MEMBER( bulletf_state::xfdc_w )
{
	/*

	    bit     signal

	    0       Unit select number
	    1       Unit select number
	    2       Unit select number
	    3       Unit select number
	    4       Select side 2
	    5       Disable 3 & 5 inch spindle motors
	    6       Set for 1 MHz controller operation, reset for 2 MHz controller operation
	    7       Set to select single density

	*/

	// drive select
	m_floppy = NULL;

	switch (data & 0x0f)
	{
	// 5.25"
	case 0: m_floppy = m_floppy0->get_device(); break;
	case 1: m_floppy = m_floppy1->get_device(); break;
	case 2: m_floppy = m_floppy2->get_device(); break;
	case 3: m_floppy = m_floppy3->get_device(); break;
	// 8"
	case 4: m_floppy = m_floppy4->get_device(); break;
	case 5: m_floppy = m_floppy5->get_device(); break;
	case 6: m_floppy = m_floppy6->get_device(); break;
	case 7: m_floppy = m_floppy7->get_device(); break;
	// 3.5"
	case 8: m_floppy = m_floppy8->get_device(); break;
	case 9: m_floppy = m_floppy9->get_device(); break;
	}

	m_fdc->set_floppy(m_floppy);

	if (m_floppy)
	{
		// side select
		m_floppy->ss_w(BIT(data, 4));

		// floppy motor
		m_floppy->mon_w(BIT(data, 5));
	}

	// FDC clock
	m_fdc->set_unscaled_clock(BIT(data, 6) ? XTAL_16MHz/16 : XTAL_16MHz/8);

	// density select
	m_fdc->dden_w(BIT(data, 7));
}


//-------------------------------------------------
//  mbank_w -
//-------------------------------------------------

WRITE8_MEMBER( bulletf_state::mbank_w )
{
	/*

	    bit     signal

	    0       Select active bank (0=bank 0, 1=bank 1)
	    1
	    2
	    3
	    4       Select system space overlay (0=no overlay, 1=overlay)
	    5
	    6
	    7

	*/

	m_mbank = data;
}


//-------------------------------------------------
//  scsi_r -
//-------------------------------------------------

READ8_MEMBER( bulletf_state::scsi_r )
{
	UINT8 data = m_scsibus->scsi_data_r();

	m_scsibus->scsi_ack_w(1);

	m_wack = 0;
	update_dma_rdy();

	return data;
}


//-------------------------------------------------
//  scsi_w -
//-------------------------------------------------

WRITE8_MEMBER( bulletf_state::scsi_w )
{
	m_scsibus->scsi_data_w(data);

	m_scsibus->scsi_ack_w(1);

	m_wack = 0;
	update_dma_rdy();
}


//-------------------------------------------------
//  hwsts_r -
//-------------------------------------------------

READ8_MEMBER( bulletf_state::hwsts_r )
{
	/*

	    bit     signal

	    0       CBUSY   Centronics busy
	    1       SW2     DIP switch 2
	    2       SW3     DIP switch 3
	    3       FD2S    *Floppy disk two sided
	    4       HLDST   Floppy disk head load status
	    5       XDCG    *Floppy disk exchange (8-inch only)
	    6       FDIRQ   FDC interrupt request line
	    7       FDDRQ   FDC data request line

	*/

	UINT8 data = 0;

	// centronics busy
	data |= m_centronics->busy_r();

	// DIP switches
	data |= m_sw1->read() & 0x06;

	// floppy
	data |= (m_floppy ? m_floppy->twosid_r() : 1) << 3;
	data |= m_fdc->hld_r() << 4;
	data |= (m_floppy ? m_floppy->dskchg_r() : 1) << 5;
	data |= m_fdc->intrq_r() << 6;
	data |= m_fdc->drq_r() << 7;

	return data;
}



//**************************************************************************
//  ADDRESS MAPS
//**************************************************************************

//-------------------------------------------------
//  ADDRESS_MAP( bullet_mem )
//-------------------------------------------------

static ADDRESS_MAP_START( bullet_mem, AS_PROGRAM, 8, bullet_state )
	AM_RANGE(0x0000, 0xffff) AM_READWRITE(mreq_r, mreq_w)
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( bullet_io )
//-------------------------------------------------

static ADDRESS_MAP_START( bullet_io, AS_IO, 8, bullet_state )
	ADDRESS_MAP_GLOBAL_MASK(0x1f)
	AM_RANGE(0x00, 0x03) AM_DEVREADWRITE(Z80DART_TAG, z80dart_device, ba_cd_r, ba_cd_w)
	AM_RANGE(0x04, 0x07) AM_DEVREADWRITE(Z80PIO_TAG, z80pio_device, read, write)
	AM_RANGE(0x08, 0x0b) AM_DEVREADWRITE(Z80CTC_TAG, z80ctc_device, read, write)
	AM_RANGE(0x0c, 0x0c) AM_MIRROR(0x03) AM_READWRITE(win_r, wstrobe_w)
	AM_RANGE(0x10, 0x13) AM_DEVREADWRITE(MB8877_TAG, mb8877_t, read, write)
	AM_RANGE(0x14, 0x14) AM_DEVREADWRITE_LEGACY(Z80DMA_TAG, z80dma_r, z80dma_w)
	AM_RANGE(0x15, 0x15) AM_READWRITE(brom_r, brom_w)
	AM_RANGE(0x16, 0x16) AM_WRITE(exdsk_w)
	AM_RANGE(0x17, 0x17) AM_WRITE(exdma_w)
	AM_RANGE(0x18, 0x18) AM_WRITE(hdcon_w)
	AM_RANGE(0x19, 0x19) AM_READ(info_r)
	AM_RANGE(0x1a, 0x1a) AM_WRITE(segst_w)
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( bulletf_mem )
//-------------------------------------------------

static ADDRESS_MAP_START( bulletf_mem, AS_PROGRAM, 8, bulletf_state )
	AM_RANGE(0x0000, 0xffff) AM_READWRITE(mreq_r, mreq_w)
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( bulletf_io )
//-------------------------------------------------

static ADDRESS_MAP_START( bulletf_io, AS_IO, 8, bulletf_state )
	ADDRESS_MAP_GLOBAL_MASK(0x3f)
	AM_RANGE(0x00, 0x03) AM_DEVREADWRITE(Z80DART_TAG, z80dart_device, ba_cd_r, ba_cd_w)
	AM_RANGE(0x04, 0x07) AM_DEVREADWRITE(Z80PIO_TAG, z80pio_device, read, write)
	AM_RANGE(0x08, 0x0b) AM_DEVREADWRITE(Z80CTC_TAG, z80ctc_device, read, write)
	AM_RANGE(0x10, 0x13) AM_DEVREADWRITE(MB8877_TAG, mb8877_t, read, write)
	AM_RANGE(0x14, 0x14) AM_WRITE(xdma0_w)
	AM_RANGE(0x16, 0x16) AM_WRITE(xfdc_w)
	AM_RANGE(0x17, 0x17) AM_WRITE(mbank_w)
	AM_RANGE(0x19, 0x19) AM_READWRITE(scsi_r, scsi_w)
	AM_RANGE(0x1a, 0x1a) AM_DEVREADWRITE_LEGACY(Z80DMA_TAG, z80dma_r, z80dma_w)
	AM_RANGE(0x1b, 0x1b) AM_READ(hwsts_r)
ADDRESS_MAP_END



//**************************************************************************
//  INPUT PORTS
//**************************************************************************

//-------------------------------------------------
//  INPUT_PORTS( bullet )
//-------------------------------------------------

INPUT_PORTS_START( bullet )
	PORT_START("SW1")
	PORT_DIPUNUSED_DIPLOC( 0x01, IP_ACTIVE_LOW, "SW1:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, IP_ACTIVE_LOW, "SW1:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, IP_ACTIVE_LOW, "SW1:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, IP_ACTIVE_LOW, "SW1:4" )
	PORT_DIPNAME( 0xf0, 0x50, "Floppy Type" ) PORT_DIPLOCATION("SW1:5,6,7,8")
	PORT_DIPSETTING(    0xf0, "5.25\" SD" )
	PORT_DIPSETTING(    0x50, "5.25\" DD" )
	PORT_DIPSETTING(    0x90, "8\" SD" )
	PORT_DIPSETTING(    0x00, "8\" DD" )
INPUT_PORTS_END


//-------------------------------------------------
//  INPUT_PORTS( bulletf )
//-------------------------------------------------

INPUT_PORTS_START( bulletf )
	PORT_START("SW1")
	PORT_DIPNAME( 0x01, 0x01, "SCSI Bus Termination" ) PORT_DIPLOCATION("SW1:1")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPUNUSED_DIPLOC( 0x02, IP_ACTIVE_LOW, "SW1:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, IP_ACTIVE_LOW, "SW1:3" )
	PORT_DIPNAME( 0x08, 0x08, "Boot ROM Device" ) PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(    0x00, "Onboard" )
	PORT_DIPSETTING(    0x08, "EPROM" )
	PORT_DIPNAME( 0xf0, 0xc0, "Floppy Type" ) PORT_DIPLOCATION("SW1:5,6,7,8")
	PORT_DIPSETTING(    0x10, "3\" DD" )
	PORT_DIPSETTING(    0xc0, "5.25\" SD" )
	PORT_DIPSETTING(    0x40, "5.25\" DD" )
	PORT_DIPSETTING(    0xa0, "8\" SD" )
	PORT_DIPSETTING(    0x20, "8\" DD" )
INPUT_PORTS_END



//**************************************************************************
//  DEVICE CONFIGURATION
//**************************************************************************

//-------------------------------------------------
//  Z80CTC_INTERFACE( ctc_intf )
//-------------------------------------------------

TIMER_DEVICE_CALLBACK_MEMBER(bullet_state::ctc_tick)
{
	m_ctc->trg0(1);
	m_ctc->trg0(0);

	m_ctc->trg1(1);
	m_ctc->trg1(0);

	m_ctc->trg2(1);
	m_ctc->trg2(0);
}

WRITE_LINE_MEMBER( bullet_state::dart_rxtxca_w )
{
	m_dart->txca_w(state);
	m_dart->rxca_w(state);
}

static Z80CTC_INTERFACE( ctc_intf )
{
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),
	DEVCB_DRIVER_LINE_MEMBER(bullet_state, dart_rxtxca_w),
	DEVCB_DEVICE_LINE_MEMBER(Z80DART_TAG, z80dart_device, rxtxcb_w),
	DEVCB_DEVICE_LINE_MEMBER(DEVICE_SELF, z80ctc_device, trg3)
};


//-------------------------------------------------
//  Z80DART_INTERFACE( dart_intf )
//-------------------------------------------------

WRITE_LINE_MEMBER( bullet_state::dartardy_w )
{
	m_dartardy = state;
	update_dma_rdy();
}

WRITE_LINE_MEMBER( bullet_state::dartbrdy_w )
{
	m_dartbrdy = state;
	update_dma_rdy();
}

static Z80DART_INTERFACE( dart_intf )
{
	0, 0, 0, 0,

	DEVCB_DEVICE_LINE_MEMBER(RS232_A_TAG, serial_port_device, tx),
	DEVCB_DEVICE_LINE_MEMBER(RS232_A_TAG, rs232_port_device, dtr_w),
	DEVCB_DEVICE_LINE_MEMBER(RS232_A_TAG, rs232_port_device, rts_w),
	DEVCB_DRIVER_LINE_MEMBER(bullet_state, dartardy_w),
	DEVCB_NULL,

	DEVCB_DEVICE_LINE_MEMBER(RS232_B_TAG, serial_port_device, tx),
	DEVCB_DEVICE_LINE_MEMBER(RS232_B_TAG, rs232_port_device, dtr_w),
	DEVCB_DEVICE_LINE_MEMBER(RS232_B_TAG, rs232_port_device, rts_w),
	DEVCB_DRIVER_LINE_MEMBER(bullet_state, dartbrdy_w),
	DEVCB_NULL,

	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL
};


//-------------------------------------------------
//  Z80DMA_INTERFACE( dma_intf )
//-------------------------------------------------

void bullet_state::update_dma_rdy()
{
	int rdy = 1;

	switch (m_exdma & 0x07)
	{
	case FDRDY:
		rdy = m_fdrdy;
		break;

	case DARTARDY:
		rdy = m_dartardy;
		break;

	case DARTBRDY:
		rdy = m_dartbrdy;
		break;

	case WINRDY:
		rdy = m_winrdy;
		break;

	case EXRDY1:
		rdy = m_exrdy1;
		break;

	case EXRDY2:
		rdy = m_exrdy2;
		break;
	}

	z80dma_rdy_w(m_dmac, rdy);
}

READ8_MEMBER( bullet_state::dma_mreq_r )
{
	UINT8 data = m_ram->pointer()[(m_buf << 16) | offset];

	if (BIT(m_exdma, 4))
	{
		m_buf = !m_buf;
	}

	return data;
}

WRITE8_MEMBER( bullet_state::dma_mreq_w )
{
	m_ram->pointer()[(m_buf << 16) | offset] = data;

	if (BIT(m_exdma, 4))
	{
		m_buf = !m_buf;
	}
}

READ8_MEMBER(bullet_state::io_read_byte)
{
	return m_maincpu->space(AS_IO).read_byte(offset);
}

WRITE8_MEMBER(bullet_state::io_write_byte)
{
	m_maincpu->space(AS_IO).write_byte(offset, data);
}

static Z80DMA_INTERFACE( dma_intf )
{
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_HALT),
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(bullet_state, dma_mreq_r),
	DEVCB_DRIVER_MEMBER(bullet_state, dma_mreq_w),
	DEVCB_DRIVER_MEMBER(bullet_state, io_read_byte),
	DEVCB_DRIVER_MEMBER(bullet_state, io_write_byte)
};


//-------------------------------------------------
//  Z80DMA_INTERFACE( bulletf_dma_intf )
//-------------------------------------------------

void bulletf_state::update_dma_rdy()
{
	int rdy = 1;

	if (BIT(m_xdma0, 0))
	{
		rdy = m_wack || m_wrdy;
	}
	else
	{
		rdy = m_fdrdy;
	}

	z80dma_rdy_w(m_dmac, rdy);
}

READ8_MEMBER( bulletf_state::dma_mreq_r )
{
	return m_ram->pointer()[(DMB4 << 16) | offset];
}

WRITE8_MEMBER( bulletf_state::dma_mreq_w )
{
	m_ram->pointer()[(DMB6 << 16) | offset] = data;
}

static Z80DMA_INTERFACE( bulletf_dma_intf )
{
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_HALT),
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(bulletf_state, dma_mreq_r),
	DEVCB_DRIVER_MEMBER(bulletf_state, dma_mreq_w),
	DEVCB_DRIVER_MEMBER(bullet_state, io_read_byte),
	DEVCB_DRIVER_MEMBER(bullet_state, io_write_byte)
};


//-------------------------------------------------
//  Z80PIO_INTERFACE( pio_intf )
//-------------------------------------------------

READ8_MEMBER( bullet_state::pio_pb_r )
{
	/*

	    bit     signal      description

	    0                   centronics busy
	    1                   centronics paper end
	    2                   centronics selected
	    3       *FAULT      centronics fault
	    4                   external vector
	    5       WBUSDIR     winchester bus direction
	    6       WCOMPLETE   winchester command complete
	    7       *WINRDY     winchester ready

	*/

	UINT8 data = 0;

	// centronics
	data |= m_centronics->busy_r();
	data |= m_centronics->pe_r() << 1;
	data |= m_centronics->vcc_r() << 2;
	data |= m_centronics->fault_r() << 3;

	return data;
}

static Z80PIO_INTERFACE( pio_intf )
{
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),
	DEVCB_NULL,
	DEVCB_DEVICE_MEMBER(CENTRONICS_TAG, centronics_device, write),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(bullet_state, pio_pb_r),
	DEVCB_NULL,
	DEVCB_NULL
};


//-------------------------------------------------
//  Z80PIO_INTERFACE( bulletf_pio_intf )
//-------------------------------------------------

READ8_MEMBER( bulletf_state::pio_pa_r )
{
	/*

	    bit     signal

	    0
	    1
	    2
	    3       BUSY
	    4       MSG
	    5       C/D
	    6       REQ
	    7       I/O

	*/

	UINT8 data = 0;

	data |= m_scsibus->scsi_bsy_r() << 3;
	data |= m_scsibus->scsi_msg_r() << 4;
	data |= m_scsibus->scsi_cd_r() << 5;
	data |= m_scsibus->scsi_req_r() << 6;
	data |= m_scsibus->scsi_io_r() << 7;

	return data;
}

WRITE8_MEMBER( bulletf_state::pio_pa_w )
{
	/*

	    bit     signal

	    0       ATN
	    1       RST
	    2       SEL
	    3
	    4
	    5
	    6
	    7

	*/

	m_scsibus->scsi_atn_w(BIT(data, 0));
	m_scsibus->scsi_rst_w(BIT(data, 1));
	m_scsibus->scsi_sel_w(BIT(data, 2));
}

WRITE_LINE_MEMBER( bulletf_state::cstrb_w )
{
	m_centronics->strobe_w(!state);
}

static Z80PIO_INTERFACE( bulletf_pio_intf )
{
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),
	DEVCB_DRIVER_MEMBER(bulletf_state, pio_pa_r),
	DEVCB_DRIVER_MEMBER(bulletf_state, pio_pa_w),
	DEVCB_DEVICE_MEMBER(CENTRONICS_TAG, centronics_device, write),
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_DRIVER_LINE_MEMBER(bulletf_state, cstrb_w)
};


//-------------------------------------------------
//  wd17xx_interface fdc_intf
//-------------------------------------------------

static SLOT_INTERFACE_START( bullet_525_floppies )
	SLOT_INTERFACE( "525sd", FLOPPY_525_SD )
	SLOT_INTERFACE( "525dd", FLOPPY_525_DD )
	SLOT_INTERFACE( "525qd", FLOPPY_525_QD )
SLOT_INTERFACE_END

static SLOT_INTERFACE_START( bullet_8_floppies )
	SLOT_INTERFACE( "8dssd", FLOPPY_8_DSSD )
	SLOT_INTERFACE( "8dsdd", FLOPPY_8_DSDD )
SLOT_INTERFACE_END

static SLOT_INTERFACE_START( bullet_35_floppies )
	SLOT_INTERFACE( "35dd", FLOPPY_35_DD )
SLOT_INTERFACE_END

void bullet_state::fdc_intrq_w(bool state)
{
	m_dart->dcda_w(state);
}

void bulletf_state::fdc_intrq_w(bool state)
{
	m_dart->rib_w(state);
}

void bullet_state::fdc_drq_w(bool state)
{
	m_fdrdy = !state;
	update_dma_rdy();
}

WRITE_LINE_MEMBER( bulletf_state::req_w )
{
	if (!state)
	{
		m_scsibus->scsi_ack_w(0);

		m_wack = 1;
	}

	m_wrdy = !state;
	update_dma_rdy();
}


static DEVICE_INPUT_DEFAULTS_START( terminal )
	DEVICE_INPUT_DEFAULTS( "TERM_TXBAUD", 0xff, 0x06 ) // 9600
	DEVICE_INPUT_DEFAULTS( "TERM_RXBAUD", 0xff, 0x06 ) // 9600
	DEVICE_INPUT_DEFAULTS( "TERM_STARTBITS", 0xff, 0x01 ) // 1
	DEVICE_INPUT_DEFAULTS( "TERM_DATABITS", 0xff, 0x03 ) // 8
	DEVICE_INPUT_DEFAULTS( "TERM_PARITY", 0xff, 0x00 ) // N
	DEVICE_INPUT_DEFAULTS( "TERM_STOPBITS", 0xff, 0x01 ) // 1
DEVICE_INPUT_DEFAULTS_END


//-------------------------------------------------
//  z80_daisy_config daisy_chain
//-------------------------------------------------

static const z80_daisy_config daisy_chain[] =
{
	{ Z80DMA_TAG },
	{ Z80DART_TAG },
	{ Z80PIO_TAG },
	{ Z80CTC_TAG },
	{ NULL }
};



//**************************************************************************
//  MACHINE INITIALIZATION
//**************************************************************************

//-------------------------------------------------
//  MACHINE_START( bullet )
//-------------------------------------------------

void bullet_state::machine_start()
{
	// floppy callbacks
	m_fdc->setup_intrq_cb(wd_fdc_t::line_cb(FUNC(bullet_state::fdc_intrq_w), this));
	m_fdc->setup_drq_cb(wd_fdc_t::line_cb(FUNC(bullet_state::fdc_drq_w), this));

	// state saving
	save_item(NAME(m_segst));
	save_item(NAME(m_brom));
	save_item(NAME(m_exdma));
	save_item(NAME(m_buf));
	save_item(NAME(m_fdrdy));
	save_item(NAME(m_dartardy));
	save_item(NAME(m_dartbrdy));
	save_item(NAME(m_winrdy));
	save_item(NAME(m_exrdy1));
	save_item(NAME(m_exrdy2));
}


//-------------------------------------------------
//  MACHINE_START( bulletf )
//-------------------------------------------------

void bulletf_state::machine_start()
{
	// floppy callbacks
	m_fdc->setup_intrq_cb(wd_fdc_t::line_cb(FUNC(bulletf_state::fdc_intrq_w), this));
	m_fdc->setup_drq_cb(wd_fdc_t::line_cb(FUNC(bulletf_state::fdc_drq_w), this));

	// state saving
	save_item(NAME(m_fdrdy));
	save_item(NAME(m_rome));
	save_item(NAME(m_xdma0));
	save_item(NAME(m_mbank));
	save_item(NAME(m_wack));
	save_item(NAME(m_wrdy));
}


void bullet_state::machine_reset()
{
	// memory banking
	m_brom = 0;
	m_segst = 0;

	// DMA ready
	m_exdma = 0;
	m_buf = 0;
	update_dma_rdy();

	// disable software control
	m_exdsk_sw = false;
	m_hdcon_sw = false;

	UINT8 sw1 = m_sw1->read();
	int mini = BIT(sw1, 6);
	m_fdc->set_unscaled_clock(mini ? XTAL_16MHz/16 : XTAL_16MHz/8);
	m_fdc->dden_w(BIT(sw1, 7));

	if (mini)
	{
		m_floppy = m_floppy0->get_device();
	}
	else
	{
		m_floppy = m_floppy4->get_device();
	}

	m_fdc->set_floppy(m_floppy);

	if (m_floppy)
	{
		m_floppy->ss_w(0);
		m_floppy->mon_w(0);
	}
}


void bulletf_state::machine_reset()
{
	// memory banking
	m_rome = 0;
	m_mbank = 0;

	// DMA ready
	m_xdma0 = 0;
	m_wack = 0;
	m_wrdy = 0;
	update_dma_rdy();
}



//**************************************************************************
//  MACHINE CONFIGURATION
//**************************************************************************

//-------------------------------------------------
//  MACHINE_CONFIG( bullet )
//-------------------------------------------------

static MACHINE_CONFIG_START( bullet, bullet_state )
	// basic machine hardware
	MCFG_CPU_ADD(Z80_TAG, Z80, XTAL_16MHz/4)
	MCFG_CPU_PROGRAM_MAP(bullet_mem)
	MCFG_CPU_IO_MAP(bullet_io)
	MCFG_CPU_CONFIG(daisy_chain)

	// devices
	MCFG_Z80CTC_ADD(Z80CTC_TAG, XTAL_16MHz/4, ctc_intf)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("ctc", bullet_state, ctc_tick, attotime::from_hz(XTAL_4_9152MHz/4))
	MCFG_Z80DART_ADD(Z80DART_TAG, XTAL_16MHz/4, dart_intf)
	MCFG_Z80DMA_ADD(Z80DMA_TAG, XTAL_16MHz/4, dma_intf)
	MCFG_Z80PIO_ADD(Z80PIO_TAG, XTAL_16MHz/4, pio_intf)
	MCFG_MB8877x_ADD(MB8877_TAG, XTAL_16MHz/16)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":0", bullet_525_floppies, "525qd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":1", bullet_525_floppies, NULL,    floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":2", bullet_525_floppies, NULL,    floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":3", bullet_525_floppies, NULL,    floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":4", bullet_8_floppies, NULL,      floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":5", bullet_8_floppies, NULL,      floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":6", bullet_8_floppies, NULL,      floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":7", bullet_8_floppies, NULL,      floppy_image_device::default_floppy_formats)
	MCFG_CENTRONICS_PRINTER_ADD(CENTRONICS_TAG, standard_centronics)

	MCFG_RS232_PORT_ADD(RS232_A_TAG, default_rs232_devices, "serial_terminal")
	MCFG_SERIAL_OUT_RX_HANDLER(DEVWRITELINE(Z80DART_TAG, z80dart_device, rxa_w))
	MCFG_DEVICE_CARD_DEVICE_INPUT_DEFAULTS("serial_terminal", terminal)

	MCFG_RS232_PORT_ADD(RS232_B_TAG, default_rs232_devices, NULL)
	MCFG_SERIAL_OUT_RX_HANDLER(DEVWRITELINE(Z80DART_TAG, z80dart_device, rxb_w))

	// software lists
	MCFG_SOFTWARE_LIST_ADD("flop_list", "wmbullet")

	// internal ram
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("128K")
MACHINE_CONFIG_END


//-------------------------------------------------
//  MACHINE_CONFIG( bulletf )
//-------------------------------------------------

static MACHINE_CONFIG_START( bulletf, bulletf_state )
	// basic machine hardware
	MCFG_CPU_ADD(Z80_TAG, Z80, XTAL_16MHz/4)
	MCFG_CPU_PROGRAM_MAP(bulletf_mem)
	MCFG_CPU_IO_MAP(bulletf_io)
	MCFG_CPU_CONFIG(daisy_chain)

	// devices
	MCFG_Z80CTC_ADD(Z80CTC_TAG, XTAL_16MHz/4, ctc_intf)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("ctc", bullet_state, ctc_tick, attotime::from_hz(XTAL_4_9152MHz/4))
	MCFG_Z80DART_ADD(Z80DART_TAG, XTAL_16MHz/4, dart_intf)
	MCFG_Z80DMA_ADD(Z80DMA_TAG, XTAL_16MHz/4, dma_intf)
	MCFG_Z80PIO_ADD(Z80PIO_TAG, XTAL_16MHz/4, bulletf_pio_intf)
	MCFG_MB8877x_ADD(MB8877_TAG, XTAL_16MHz/16)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":0", bullet_525_floppies, "525qd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":1", bullet_525_floppies, NULL,    floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":2", bullet_525_floppies, NULL,    floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":3", bullet_525_floppies, NULL,    floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":4", bullet_8_floppies, NULL, floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":5", bullet_8_floppies, NULL, floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":6", bullet_8_floppies, NULL, floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":7", bullet_8_floppies, NULL, floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":8", bullet_35_floppies, NULL, floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(MB8877_TAG":9", bullet_35_floppies, NULL, floppy_image_device::default_floppy_formats)
	MCFG_CENTRONICS_PRINTER_ADD(CENTRONICS_TAG, standard_centronics)

	MCFG_RS232_PORT_ADD(RS232_A_TAG, default_rs232_devices, "serial_terminal")
	MCFG_SERIAL_OUT_RX_HANDLER(DEVWRITELINE(Z80DART_TAG, z80dart_device, rxa_w))
	MCFG_DEVICE_CARD_DEVICE_INPUT_DEFAULTS("serial_terminal", terminal)

	MCFG_RS232_PORT_ADD(RS232_B_TAG, default_rs232_devices, NULL)
	MCFG_SERIAL_OUT_RX_HANDLER(DEVWRITELINE(Z80DART_TAG, z80dart_device, rxb_w))

	MCFG_SCSIBUS_ADD(SCSIBUS_TAG)
	MCFG_SCSIDEV_ADD(SCSIBUS_TAG ":harddisk0", SCSIHD, SCSI_ID_0)
	MCFG_SCSICB_ADD(SCSIBUS_TAG ":host")
	MCFG_SCSICB_REQ_HANDLER(DEVWRITELINE(DEVICE_SELF_OWNER, bulletf_state, req_w))

	// software lists
	MCFG_SOFTWARE_LIST_ADD("flop_list", "wmbullet")

	// internal ram
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("128K")
MACHINE_CONFIG_END



//**************************************************************************
//  ROMS
//**************************************************************************

//-------------------------------------------------
//  ROM( bullet )
//-------------------------------------------------

ROM_START( wmbullet )
	ROM_REGION( 0x10000, Z80_TAG, 0 )
	ROM_LOAD( "sr70x.u8", 0x00, 0x20, CRC(d54b8a30) SHA1(65ff8753dd63c9dd1899bc9364a016225585d050) )
ROM_END


#define rom_wmbulletf rom_wmbullet



//**************************************************************************
//  SYSTEM DRIVERS
//**************************************************************************

//    YEAR  NAME        PARENT      COMPAT  MACHINE     INPUT       INIT    COMPANY         FULLNAME                FLAGS
// the setname 'bullet' is used by Sega's Bullet in MAME.
COMP( 1982, wmbullet,       0,          0,      bullet,     bullet, driver_device,  0,      "Wave Mate",    "Bullet",               GAME_NOT_WORKING | GAME_SUPPORTS_SAVE | GAME_NO_SOUND_HW )
COMP( 1984, wmbulletf,  wmbullet,       0,      bulletf,    bulletf, driver_device, 0,      "Wave Mate",    "Bullet (Revision F)",  GAME_NOT_WORKING | GAME_SUPPORTS_SAVE | GAME_NO_SOUND_HW )
