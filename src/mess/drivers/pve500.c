/***************************************************************************

  SONY PVE-500

  Driver by Felipe Sanches
  Technical info at https://www.garoa.net.br/wiki/PVE-500

  Licensed under GPLv2 or later.

  NOTE: Even though the MAME/MESS project has been adopting a non-commercial additional licensing clause, I do allow commercial usage of my portion of the code according to the plain terms of the GPL license (version 2 or later). This is useful if you happen to use my code in another project or in case the other MAME/MESS developers happen to drop the non-comercial clause completely. I suggest that other developers consider doing the same. --Felipe Sanches

  Changelog:

   2014 JAN 14 [Felipe Sanches]:
   * Initial driver skeleton
*/

#include "emu.h"
#include "cpu/z80/z80.h"

class pve500_state : public driver_device
{
public:
	pve500_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_subcpu(*this, "subcpu")
	{ }

	DECLARE_WRITE8_MEMBER(io_expander_w);
	DECLARE_READ8_MEMBER(io_expander_r);
	DECLARE_DRIVER_INIT(pve500);
private:
	virtual void machine_start();
	virtual void machine_reset();
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_subcpu;
};

static ADDRESS_MAP_START(maincpu_prg, AS_PROGRAM, 8, pve500_state)
	AM_RANGE (0x0000, 0xBFFF) AM_ROM // 48kbytes  EEPROM ICB7
	AM_RANGE (0xC000, 0xDFFF) AM_RAM //  ICD6  RAM 8k
	AM_RANGE (0xE000, 0xE7FF) AM_MIRROR(0x1800) AM_RAM AM_SHARE("sharedram") //  F5: 2kbytes RAM compartilhada (comunicacao entre os 2 processadores)
ADDRESS_MAP_END

static ADDRESS_MAP_START(subcpu_prg, AS_PROGRAM, 8, pve500_state)
	AM_RANGE (0x0000, 0x7FFF) AM_ROM // 32KBYTES EEPROM
	AM_RANGE (0x8000, 0xBFFF) AM_READWRITE(io_expander_r, io_expander_w) // ICG3: 16KBYTES 
	AM_RANGE (0xC000, 0xC7FF) AM_MIRROR(0x3800) AM_RAM AM_SHARE("sharedram") //  F5: 2kbytes RAM compartilhada (comunicacao entre os 2 processadores)
ADDRESS_MAP_END

DRIVER_INIT_MEMBER( pve500_state, pve500 )
{
}

static INPUT_PORTS_START( pve500 )
	PORT_START("keyboard")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) PORT_NAME("TODO") PORT_CODE(KEYCODE_A)
INPUT_PORTS_END

void pve500_state::machine_start()
{
}

void pve500_state::machine_reset()
{
}

READ8_MEMBER(pve500_state::io_expander_r)
{
	/* Implement-me ! */
	return 0;
}

WRITE8_MEMBER(pve500_state::io_expander_w)
{
	/* Implement-me !*/
}

static MACHINE_CONFIG_START( pve500, pve500_state )
	MCFG_CPU_ADD("maincpu", Z80, XTAL_12MHz / 2)
	MCFG_CPU_PROGRAM_MAP(maincpu_prg)

	MCFG_CPU_ADD("subcpu", Z80, XTAL_12MHz / 2)
	MCFG_CPU_PROGRAM_MAP(subcpu_prg)
MACHINE_CONFIG_END

ROM_START( pve500 )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD("pve500.icb5",  0x00000, 0x10000, CRC(0) SHA1(0) ) //48kbyte main-cpu program

	ROM_REGION( 0x8000, "subcpu", 0 )
	ROM_LOAD("pve500.icf3",  0x00000, 0x10000, CRC(0) SHA1(0) ) //32kbyte sub-cpu program
ROM_END

/*    YEAR  NAME    PARENT  COMPAT  MACHINE     INPUT   CLASS           INIT   COMPANY    FULLNAME                    FLAGS */
COMP( 1995, pve500, 0,      0,      pve500,     pve500, pve500_state, pve500, "SONY", "PVE-500", GAME_IMPERFECT_GRAPHICS | GAME_NO_SOUND)
