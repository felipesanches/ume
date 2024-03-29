// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    COMX PL-80 plotter emulation

    Copyright MESS Team.
    Visit http://mamedev.org for licensing and usage restrictions.

**********************************************************************/

#pragma once

#ifndef __COMX_PL80__
#define __COMX_PL80__

#include "emu.h"
#include "cpu/m6805/m6805.h"
#include "bus/centronics/ctronics.h"



//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> comx_pl80_device

class comx_pl80_device :  public device_t,
							public device_centronics_peripheral_interface
{
public:
	// construction/destruction
	comx_pl80_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	// optional information overrides
	virtual const rom_entry *device_rom_region() const;
	virtual machine_config_constructor device_mconfig_additions() const;
	virtual ioport_constructor device_input_ports() const;

	// not really public
	DECLARE_WRITE8_MEMBER( pa_w );
	DECLARE_WRITE8_MEMBER( pb_w );
	DECLARE_WRITE8_MEMBER( pc_w );
	DECLARE_READ8_MEMBER( pd_r );

protected:
	// device-level overrides
	virtual void device_start();
	virtual void device_reset();

private:
	required_memory_region m_plotter;
	required_ioport m_font;
	required_ioport m_sw;

	// PL-80 plotter state
	UINT16 m_font_addr;         // font ROM pack address latch
	UINT8 m_x_motor_phase;      // X motor phase
	UINT8 m_y_motor_phase;      // Y motor phase
	UINT8 m_z_motor_phase;      // Z motor phase
	UINT8 m_plotter_data;       // plotter data bus
	int m_plotter_ack;          // plotter acknowledge
	int m_plotter_online;       // online LED
};


// device type definition
extern const device_type COMX_PL80;



#endif
