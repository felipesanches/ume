/*
  Technics SX-KN5000 musical keyboard

  driver by Felipe Corrêa da Silva Sanches <fsanches@metamaquina.com.br>

  Licensed under GPLv2 or later.

  NOTE: Even though the MAME/MESS project has been adopting a non-commercial additional licensing clause, I do allow commercial usage of my portion of the code according to the plain terms of the GPL license (version 2 or later). This is useful if you happen to use my code in another project or in case the other MAME/MESS developers happen to drop the non-comercial clause completely. I suggest that other developers consider doing the same. --Felipe Sanches

Changelog:

 2014 JAN 02 [Felipe Sanches]:
 * Initial driver skeleton

*/

#include "emu.h"
#include "cpu/tlcs900/tlcs900.h"

/****************************************************\
* I/O devices                                        *
\****************************************************/

class kn5000_state : public driver_device
{
public:
	kn5000_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu")
	{
	}

//	DECLARE_READ8_MEMBER(port_r);
//	DECLARE_WRITE8_MEMBER(port_w);
	DECLARE_DRIVER_INIT(kn5000);
	virtual void machine_start();
	virtual void machine_reset();
	required_device<cpu_device> m_maincpu;
};

void kn5000_state::machine_start()
{
}

/****************************************************\
* Address maps                                       *
\****************************************************/

static ADDRESS_MAP_START( maincpu_mem, AS_PROGRAM, 8, kn5000_state )
	AM_RANGE(0x00000000, 0x0001FFFF) AM_ROM
	AM_RANGE(0x00000000, 0x003fffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( subcpu_mem, AS_PROGRAM, 8, kn5000_state )
	AM_RANGE(0x00000000, 0x0001FFFF) AM_ROM
	AM_RANGE(0x00000000, 0x003fffff) AM_RAM
ADDRESS_MAP_END

/****************************************************\
* Input ports                                        *
\****************************************************/

static INPUT_PORTS_START( kn5000 )
	PORT_START("panel")
INPUT_PORTS_END

/****************************************************\
* Machine definition                                 *
\****************************************************/

DRIVER_INIT_MEMBER(kn5000_state, kn5000)
{
}

void kn5000_state::machine_reset()
{
}

static MACHINE_CONFIG_START( kn5000, kn5000_state )

	MCFG_CPU_ADD("maincpu", TMP95C063, XTAL_8MHz) //[IC5] Correct CPU is TMP94C241F
	MCFG_CPU_PROGRAM_MAP(maincpu_mem)

	MCFG_CPU_ADD("subcpu", TMP95C063, XTAL_10MHz) //[IC27] Correct CPU is TMP94C241F
	MCFG_CPU_PROGRAM_MAP(subcpu_mem)

MACHINE_CONFIG_END

ROM_START( kn5000 )
	ROM_REGION( 0x20000, "maincpu", 0 )
	ROM_LOAD( "kn5000_maincpu.bin", 0x0000, 0x1EF9A, CRC(0) SHA1(0))
ROM_END

/*   YEAR  NAME      PARENT    COMPAT    MACHINE   INPUT     INIT      COMPANY          FULLNAME */
CONS(2012, kn5000,    0,        0,        kn5000,    kn5000, kn5000_state,    kn5000,    "Technics", "kn5000 musical keyboard", GAME_NOT_WORKING)
