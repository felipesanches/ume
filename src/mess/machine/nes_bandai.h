#ifndef __NES_BANDAI_H
#define __NES_BANDAI_H

#include "machine/nes_nxrom.h"
#include "machine/i2cmem.h"


// ======================> nes_karaokestudio_device

class nes_karaokestudio_device : public nes_nrom_device
{
public:
	// construction/destruction
	nes_karaokestudio_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	// device-level overrides
	virtual void device_start();
	virtual DECLARE_WRITE8_MEMBER(write_h);

	virtual void pcb_reset();

	// TODO: add microphone I/O emulation
};


// ======================> nes_oekakids_device

class nes_oekakids_device : public nes_nrom_device
{
public:
	// construction/destruction
	nes_oekakids_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	// device-level overrides
	virtual void device_start();
	virtual DECLARE_WRITE8_MEMBER(write_h);
	virtual DECLARE_READ8_MEMBER(nt_r);
	virtual DECLARE_WRITE8_MEMBER(nt_w);

	virtual void pcb_reset();

	virtual void ppu_latch(offs_t offset);

	// TODO: add oeka kids controller emulation
protected:
	void update_chr();
	UINT8 m_reg, m_latch;
};


// ======================> nes_lz93d50_device

class nes_lz93d50_device : public nes_nrom_device
{
public:
	// construction/destruction
	nes_lz93d50_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, const char *source);
	nes_lz93d50_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	// device-level overrides
	virtual void device_start();
	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr);
	virtual DECLARE_WRITE8_MEMBER(lz93d50_write);
	virtual DECLARE_WRITE8_MEMBER(write_h) { lz93d50_write(space, offset, data, mem_mask); }
	virtual DECLARE_WRITE8_MEMBER(write_m);

	virtual void pcb_reset();

protected:
	UINT16     m_irq_count;
	int        m_irq_enable;

	static const device_timer_id TIMER_IRQ = 0;
	emu_timer *irq_timer;
};


// ======================> nes_lz93d50_24c01_device

class nes_lz93d50_24c01_device : public nes_lz93d50_device
{
public:
	// construction/destruction
	nes_lz93d50_24c01_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname, const char *source);
	nes_lz93d50_24c01_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	virtual void device_start();
	virtual machine_config_constructor device_mconfig_additions() const;
	virtual DECLARE_READ8_MEMBER(read_m);
	virtual DECLARE_WRITE8_MEMBER(write_h);

	virtual void pcb_reset();

	// TODO: fix EEPROM I/O emulation
	required_device<i2cmem_device> m_i2cmem;
	UINT8 m_i2c_mem, m_i2c_clk;
};


// ======================> nes_lz93d50_24c02_device

class nes_lz93d50_24c02_device : public nes_lz93d50_24c01_device
{
public:
	// construction/destruction
	nes_lz93d50_24c02_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	virtual machine_config_constructor device_mconfig_additions() const;
};


// ======================> nes_fcg_device

class nes_fcg_device : public nes_lz93d50_device
{
public:
	// construction/destruction
	nes_fcg_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);
};


// ======================> nes_datach_device

class nes_datach_device : public nes_lz93d50_device
{
public:
	// construction/destruction
	nes_datach_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	// TODO: add some kind of barcode reader emulation
};


// ======================> nes_fjump2_device

class nes_fjump2_device : public nes_lz93d50_device
{
public:
	// construction/destruction
	nes_fjump2_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock);

	// device-level overrides
	virtual void device_start();
	virtual DECLARE_READ8_MEMBER(read_m);
	virtual DECLARE_WRITE8_MEMBER(write_m);
	virtual DECLARE_WRITE8_MEMBER(write_h);

	virtual void pcb_reset();
private:
	void set_prg();
	UINT8 m_reg[9];
};



// device type definition
extern const device_type NES_KARAOKESTUDIO;
extern const device_type NES_OEKAKIDS;
extern const device_type NES_LZ93D50;
extern const device_type NES_LZ93D50_24C01;
extern const device_type NES_LZ93D50_24C02;
extern const device_type NES_FCG;
extern const device_type NES_DATACH;
extern const device_type NES_FJUMP2;

#endif
