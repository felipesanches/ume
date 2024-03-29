// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    Coleco Adam Expansion Port emulation

    Copyright MESS Team.
    Visit http://mamedev.org for licensing and usage restrictions.

**********************************************************************/

#include "exp.h"



//**************************************************************************
//  MACROS/CONSTANTS
//**************************************************************************

#define LOG 0



//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

const device_type ADAM_EXPANSION_SLOT = &device_creator<adam_expansion_slot_device>;



//**************************************************************************
//  DEVICE C64_EXPANSION CARD INTERFACE
//**************************************************************************

//-------------------------------------------------
//  device_adam_expansion_slot_card_interface - constructor
//-------------------------------------------------

device_adam_expansion_slot_card_interface::device_adam_expansion_slot_card_interface(const machine_config &mconfig, device_t &device)
	: device_slot_card_interface(mconfig, device),
		m_rom(NULL),
		m_ram(NULL),
		m_rom_mask(0),
		m_ram_mask(0)
{
	m_slot = dynamic_cast<adam_expansion_slot_device *>(device.owner());
}


//-------------------------------------------------
//  ~device_adam_expansion_slot_card_interface - destructor
//-------------------------------------------------

device_adam_expansion_slot_card_interface::~device_adam_expansion_slot_card_interface()
{
}


//-------------------------------------------------
//  adam_rom_pointer - get expansion ROM pointer
//-------------------------------------------------

UINT8* device_adam_expansion_slot_card_interface::adam_rom_pointer(running_machine &machine, size_t size)
{
	if (m_rom == NULL)
	{
		m_rom = auto_alloc_array(machine, UINT8, size);

		m_rom_mask = size - 1;
	}

	return m_rom;
}


//-------------------------------------------------
//  adam_ram_pointer - get expansion ROM pointer
//-------------------------------------------------

UINT8* device_adam_expansion_slot_card_interface::adam_ram_pointer(running_machine &machine, size_t size)
{
	if (m_ram == NULL)
	{
		m_ram = auto_alloc_array(machine, UINT8, size);

		m_ram_mask = size - 1;
	}

	return m_ram;
}



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  adam_expansion_slot_device - constructor
//-------------------------------------------------

adam_expansion_slot_device::adam_expansion_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
		device_t(mconfig, ADAM_EXPANSION_SLOT, "ADAM expansion slot", tag, owner, clock, "adam_expansion_slot", __FILE__),
		device_slot_interface(mconfig, *this),
		device_image_interface(mconfig, *this)
{
}


//-------------------------------------------------
//  adam_expansion_slot_device - destructor
//-------------------------------------------------

adam_expansion_slot_device::~adam_expansion_slot_device()
{
}


//-------------------------------------------------
//  device_config_complete - perform any
//  operations now that the configuration is
//  complete
//-------------------------------------------------

void adam_expansion_slot_device::device_config_complete()
{
	// inherit a copy of the static data
	const adam_expansion_slot_interface *intf = reinterpret_cast<const adam_expansion_slot_interface *>(static_config());
	if (intf != NULL)
	{
		*static_cast<adam_expansion_slot_interface *>(this) = *intf;
	}

	// or initialize to defaults if none provided
	else
	{
		memset(&m_out_int_cb, 0, sizeof(m_out_int_cb));
	}

	// set brief and instance name
	update_names();
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void adam_expansion_slot_device::device_start()
{
	m_cart = dynamic_cast<device_adam_expansion_slot_card_interface *>(get_card_device());

	// resolve callbacks
	m_out_int_func.resolve(m_out_int_cb, *this);
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void adam_expansion_slot_device::device_reset()
{
}


//-------------------------------------------------
//  call_load -
//-------------------------------------------------

bool adam_expansion_slot_device::call_load()
{
	if (m_cart)
	{
		size_t size = 0;

		if (software_entry() == NULL)
		{
			size = length();

			fread(m_cart->adam_rom_pointer(machine(), size), size);
		}
		else
		{
			size = get_software_region_length("rom");
			if (size) memcpy(m_cart->adam_rom_pointer(machine(), size), get_software_region("rom"), size);

			size = get_software_region_length("ram");
			if (size) memcpy(m_cart->adam_ram_pointer(machine(), size), get_software_region("ram"), size);
		}
	}

	return IMAGE_INIT_PASS;
}


//-------------------------------------------------
//  call_softlist_load -
//-------------------------------------------------

bool adam_expansion_slot_device::call_softlist_load(char *swlist, char *swname, rom_entry *start_entry)
{
	load_software_part_region(this, swlist, swname, start_entry);

	return true;
}


//-------------------------------------------------
//  get_default_card_software -
//-------------------------------------------------

const char * adam_expansion_slot_device::get_default_card_software(const machine_config &config, emu_options &options)
{
	return software_get_default_slot(config, options, this, "standard");
}


//-------------------------------------------------
//  bd_r - buffered data read
//-------------------------------------------------

UINT8 adam_expansion_slot_device::bd_r(address_space &space, offs_t offset, UINT8 data, int bmreq, int biorq, int aux_rom_cs, int cas1, int cas2)
{
	if (m_cart != NULL)
	{
		data = m_cart->adam_bd_r(space, offset, data, bmreq, biorq, aux_rom_cs, cas1, cas2);
	}

	return data;
}


//-------------------------------------------------
//  cd_w - cartridge data write
//-------------------------------------------------

void adam_expansion_slot_device::bd_w(address_space &space, offs_t offset, UINT8 data, int bmreq, int biorq, int aux_rom_cs, int cas1, int cas2)
{
	if (m_cart != NULL)
	{
		m_cart->adam_bd_w(space, offset, data, bmreq, biorq, aux_rom_cs, cas1, cas2);
	}
}

WRITE_LINE_MEMBER( adam_expansion_slot_device::int_w ) { m_out_int_func(state); }


// slot devices
#include "adamlink.h"
#include "ide.h"
#include "ram.h"

//-------------------------------------------------
//  SLOT_INTERFACE( adam_slot1_devices )
//-------------------------------------------------

SLOT_INTERFACE_START( adam_slot1_devices )
	SLOT_INTERFACE("adamlink", ADAMLINK)
SLOT_INTERFACE_END


//-------------------------------------------------
//  SLOT_INTERFACE( adam_slot2_devices )
//-------------------------------------------------

SLOT_INTERFACE_START( adam_slot2_devices )
	SLOT_INTERFACE("ide", ADAM_IDE)
SLOT_INTERFACE_END


//-------------------------------------------------
//  SLOT_INTERFACE( adam_slot3_devices )
//-------------------------------------------------

SLOT_INTERFACE_START( adam_slot3_devices )
	SLOT_INTERFACE("ram", ADAM_RAM)
SLOT_INTERFACE_END
