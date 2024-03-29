// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    CMD SwiftLink RS-232 cartridge emulation

    Copyright MESS Team.
    Visit http://mamedev.org for licensing and usage restrictions.

**********************************************************************/

/*

    http://mclauchlan.site.net.au/scott/C=Hacking/C-Hacking10/C-Hacking10-swiftlink.html

*/

#include "swiftlink.h"



//**************************************************************************
//  MACROS/CONSTANTS
//**************************************************************************

#define MOS6551_TAG     "mos6551"
#define RS232_TAG       "rs232"



//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

const device_type C64_SWIFTLINK = &device_creator<c64_swiftlink_cartridge_device>;


//-------------------------------------------------
//  MACHINE_CONFIG_FRAGMENT( c64_swiftlink )
//-------------------------------------------------

static MACHINE_CONFIG_FRAGMENT( c64_swiftlink )
	MCFG_DEVICE_ADD(MOS6551_TAG, MOS6551, XTAL_3_6864MHz)
	MCFG_MOS6551_IRQ_HANDLER(WRITELINE(c64_swiftlink_cartridge_device, acia_irq_w))
	MCFG_MOS6551_TXD_HANDLER(DEVWRITELINE(RS232_TAG, rs232_port_device, tx))

	MCFG_RS232_PORT_ADD(RS232_TAG, default_rs232_devices, NULL)
	MCFG_SERIAL_OUT_RX_HANDLER(DEVWRITELINE(MOS6551_TAG, mos6551_device, rxd_w))
	MCFG_RS232_OUT_DCD_HANDLER(DEVWRITELINE(MOS6551_TAG, mos6551_device, dcd_w))
	MCFG_RS232_OUT_DSR_HANDLER(DEVWRITELINE(MOS6551_TAG, mos6551_device, dsr_w))
	MCFG_RS232_OUT_CTS_HANDLER(DEVWRITELINE(MOS6551_TAG, mos6551_device, cts_w))
MACHINE_CONFIG_END


//-------------------------------------------------
//  machine_config_additions - device-specific
//  machine configurations
//-------------------------------------------------

machine_config_constructor c64_swiftlink_cartridge_device::device_mconfig_additions() const
{
	return MACHINE_CONFIG_NAME( c64_swiftlink );
}


//-------------------------------------------------
//  INPUT_PORTS( c64_swiftlink )
//-------------------------------------------------

static INPUT_PORTS_START( c64_swiftlink )
	PORT_START("CS")
	PORT_DIPNAME( 0x03, 0x01, "Base Address" )
	PORT_DIPSETTING(    0x00, "$D700 (C128)" )
	PORT_DIPSETTING(    0x01, "$DE00" )
	PORT_DIPSETTING(    0x02, "$DF00" )

	PORT_START("IRQ")
	PORT_DIPNAME( 0x01, 0x01, "Interrupt" )
	PORT_DIPSETTING(    0x00, "IRQ" )
	PORT_DIPSETTING(    0x01, "NMI" )
INPUT_PORTS_END


//-------------------------------------------------
//  input_ports - device-specific input ports
//-------------------------------------------------

ioport_constructor c64_swiftlink_cartridge_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( c64_swiftlink );
}



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  c64_swiftlink_cartridge_device - constructor
//-------------------------------------------------

c64_swiftlink_cartridge_device::c64_swiftlink_cartridge_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	device_t(mconfig, C64_SWIFTLINK, "C64 SwiftLink cartridge", tag, owner, clock, "c64_swiftlink", __FILE__),
	device_c64_expansion_card_interface(mconfig, *this),
	m_acia(*this, MOS6551_TAG),
	m_io_cs(*this, "CS"),
	m_io_irq(*this, "IRQ")
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void c64_swiftlink_cartridge_device::device_start()
{
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void c64_swiftlink_cartridge_device::device_reset()
{
	m_acia->reset();

	m_cs = m_io_cs->read();
	m_irq = m_io_irq->read();
}


//-------------------------------------------------
//  c64_cd_r - cartridge data read
//-------------------------------------------------

UINT8 c64_swiftlink_cartridge_device::c64_cd_r(address_space &space, offs_t offset, UINT8 data, int sphi2, int ba, int roml, int romh, int io1, int io2)
{
	if (((m_cs == DE00) && !io1) || ((m_cs == DF00) && !io2) ||
		((m_cs == D700) && ((offset & 0xff00) == 0xd700)))
	{
		data = m_acia->read(space, offset & 0x03);
	}

	return data;
}


//-------------------------------------------------
//  c64_cd_w - cartridge data write
//-------------------------------------------------

void c64_swiftlink_cartridge_device::c64_cd_w(address_space &space, offs_t offset, UINT8 data, int sphi2, int ba, int roml, int romh, int io1, int io2)
{
	if (((m_cs == DE00) && !io1) || ((m_cs == DF00) && !io2) ||
		((m_cs == D700) && ((offset & 0xff00) == 0xd700)))
	{
		m_acia->write(space, offset & 0x03, data);
	}
}


//-------------------------------------------------
//  acia_irq_w -
//-------------------------------------------------

WRITE_LINE_MEMBER( c64_swiftlink_cartridge_device::acia_irq_w )
{
	switch (m_irq)
	{
	case IRQ: m_slot->irq_w(state); break;
	case NMI: m_slot->nmi_w(state); break;
	}
}
