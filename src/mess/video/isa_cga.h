#pragma once

#ifndef __ISA_CGA_H__
#define __ISA_CGA_H__

#include "emu.h"
#include "machine/isa.h"
#include "video/mc6845.h"

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> isa8_cga_device

class isa8_cga_device :
		public device_t,
		public device_isa8_card_interface
{
	friend class isa8_cga_superimpose_device;
//  friend class isa8_ec1841_0002_device;
	friend class isa8_cga_poisk2_device;
	friend class isa8_cga_pc1512_device;

public:
	// construction/destruction
	isa8_cga_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
	isa8_cga_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, const char *source);

	// optional information overrides
	virtual machine_config_constructor device_mconfig_additions() const;
	virtual ioport_constructor device_input_ports() const;
	virtual const rom_entry *device_rom_region() const;

protected:
	required_ioport m_cga_config;

	// device-level overrides
	virtual void device_start();
	virtual void device_reset();
public:
	void mode_control_w(UINT8 data);
	void set_palette_luts();
	void plantronics_w(UINT8 data);
	virtual DECLARE_READ8_MEMBER( io_read );
	virtual DECLARE_WRITE8_MEMBER( io_write );
	DECLARE_WRITE_LINE_MEMBER( hsync_changed );
	DECLARE_WRITE_LINE_MEMBER( vsync_changed );
	virtual UINT32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

public:
	int     m_framecnt;

	UINT8   m_mode_control;  /* wo 0x3d8 */
	UINT8   m_color_select;  /* wo 0x3d9 */
	UINT8   m_status;        /* ro 0x3da */

	mc6845_update_row_func  m_update_row;
	UINT8   m_palette_lut_2bpp[4];
	offs_t  m_chr_gen_offset[4];
	UINT8   m_font_selection_mask;
	UINT8   *m_chr_gen_base;
	UINT8   *m_chr_gen;
	UINT8   m_vsync;
	UINT8   m_hsync;
	size_t  m_vram_size;
	UINT8   *m_vram;
	bool    m_superimpose;
	UINT8   m_plantronics; /* This should be moved into the appropriate subclass */
	offs_t  m_start_offset;
};

// device type definition
extern const device_type ISA8_CGA;


// ======================> isa8_cga_superimpose_device

class isa8_cga_superimpose_device :
		public isa8_cga_device
{
public:
	// construction/destruction
	isa8_cga_superimpose_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};

// device type definition
extern const device_type ISA8_CGA_SUPERIMPOSE;


// ======================> isa8_poisk2_device

class isa8_cga_poisk2_device :
		public isa8_cga_device
{
public:
	// construction/destruction
	isa8_cga_poisk2_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
	// optional information overrides
	virtual const rom_entry *device_rom_region() const;
};

// device type definition
extern const device_type ISA8_CGA_POISK2;


// ======================> isa8_pc1512_device

class isa8_cga_pc1512_device :
		public isa8_cga_device
{
public:
	// construction/destruction
	isa8_cga_pc1512_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
	// optional information overrides
	virtual ioport_constructor device_input_ports() const;
	virtual const rom_entry *device_rom_region() const;

protected:
	// device-level overrides
	virtual void device_start();
	virtual void device_reset();

public:
	UINT8   m_write;
	UINT8   m_read;
	UINT8   m_mc6845_address;
	UINT8   m_mc6845_locked_register[31];

public:
	// Static information
	// mapping of the 4 planes into videoram
	// (text data should be readable at videoram+0)
	static const offs_t vram_offset[4];
	static const UINT8 mc6845_writeonce_register[31];

	virtual DECLARE_READ8_MEMBER( io_read );
	virtual DECLARE_WRITE8_MEMBER( io_write );

	DECLARE_WRITE8_MEMBER( vram_w );
};

// device type definition
extern const device_type ISA8_CGA_PC1512;

// ======================> isa8_wyse700_device

class isa8_wyse700_device :
		public isa8_cga_device
{
public:
	// construction/destruction
	isa8_wyse700_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
	// optional information overrides
	virtual const rom_entry *device_rom_region() const;

protected:
	// device-level overrides
	virtual void device_start();
	virtual void device_reset();

public:
	virtual DECLARE_READ8_MEMBER( io_read );
	virtual DECLARE_WRITE8_MEMBER( io_write );
	virtual UINT32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	void change_resolution(UINT8 mode);

	UINT8 m_bank_offset;
	UINT8 m_bank_base;
	UINT8 m_control;
};

// device type definition
extern const device_type ISA8_WYSE700;

// ======================> isa8_ec1841_0002_device

class isa8_ec1841_0002_device :
		public isa8_cga_device
{
public:
	// construction/destruction
	isa8_ec1841_0002_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
	// optional information overrides
	virtual const rom_entry *device_rom_region() const;

protected:
	// device-level overrides
	virtual void device_start();
	virtual void device_reset();

public:
	virtual DECLARE_READ8_MEMBER( io_read );
	virtual DECLARE_WRITE8_MEMBER( io_write );

	UINT8   m_p3df;
	DECLARE_READ8_MEMBER( char_ram_read );
	DECLARE_WRITE8_MEMBER( char_ram_write );
};

// device type definition
extern const device_type ISA8_EC1841_0002;


#endif  /* __ISA_CGA_H__ */
