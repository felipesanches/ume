/******************************************************************************
 *  Microtan 65
 *
 *  system driver
 *
 *  Juergen Buchmueller <pullmoll@t-online.de>, Jul 2000
 *
 *  Thanks go to Geoff Macdonald <mail@geoff.org.uk>
 *  for his site http:://www.geo255.redhotant.com
 *  and to Fabrice Frances <frances@ensica.fr>
 *  for his site http://www.ifrance.com/oric/microtan.html
 *
 *  Microtan65 memory map
 *
 *  range     short     description
 *  0000-01FF SYSRAM    system ram
 *                      0000-003f variables
 *                      0040-00ff basic
 *                      0100-01ff stack
 *  0200-03ff VIDEORAM  display
 *  0400-afff RAM       main memory
 *  bc00-bc01 AY8912-0  sound chip #0
 *  bc02-bc03 AY8912-1  sound chip #1
 *  bc04      SPACEINV  space invasion sound (?)
 *  bfc0-bfcf VIA6522-0 VIA 6522 #0
 *  bfd0-bfd3 SIO       serial i/o
 *  bfe0-bfef VIA6522-1 VIA 6522 #0
 *  bff0      GFX_KBD   R: chunky graphics on W: reset KBD interrupt
 *  bff1      NMI       W: start delayed NMI
 *  bff2      HEX       W: hex. keypad column
 *  bff3      KBD_GFX   R: ASCII KBD / hex. keypad row W: chunky graphics off
 *  c000-e7ff BASIC     BASIC Interpreter ROM
 *  f000-f7ff XBUG      XBUG ROM
 *  f800-ffff TANBUG    TANBUG ROM
 *
 *****************************************************************************/

/* Core includes */
#include "emu.h"
#include "cpu/m6502/m6502.h"
#include "includes/microtan.h"

/* Components */
#include "sound/ay8910.h"
#include "sound/dac.h"
#include "sound/wave.h"
#include "machine/6522via.h"
#include "machine/mos6551.h"

/* Devices */
#include "imagedev/cassette.h"


static ADDRESS_MAP_START( microtan_map, AS_PROGRAM, 8, microtan_state )
	AM_RANGE(0x0000, 0x01ff) AM_RAM
	AM_RANGE(0x0200, 0x03ff) AM_RAM_WRITE(microtan_videoram_w) AM_SHARE("videoram")
	AM_RANGE(0xbc00, 0xbc00) AM_DEVWRITE("ay8910.1", ay8910_device, address_w)
	AM_RANGE(0xbc01, 0xbc01) AM_DEVREADWRITE("ay8910.1", ay8910_device, data_r, data_w)
	AM_RANGE(0xbc02, 0xbc02) AM_DEVWRITE("ay8910.2", ay8910_device, address_w)
	AM_RANGE(0xbc03, 0xbc03) AM_DEVREADWRITE("ay8910.2", ay8910_device, data_r, data_w)
	AM_RANGE(0xbfc0, 0xbfcf) AM_DEVREADWRITE("via6522_0", via6522_device, read, write)
	AM_RANGE(0xbfd0, 0xbfd3) AM_DEVREADWRITE("acia", mos6551_device, read, write)
	AM_RANGE(0xbfe0, 0xbfef) AM_DEVREADWRITE("via6522_1", via6522_device, read, write)
	AM_RANGE(0xbff0, 0xbfff) AM_READWRITE(microtan_bffx_r, microtan_bffx_w)
	AM_RANGE(0xc000, 0xe7ff) AM_ROM
	AM_RANGE(0xf000, 0xffff) AM_ROM
ADDRESS_MAP_END

static INPUT_PORTS_START( microtan )
	PORT_START("DSW")
	PORT_DIPNAME( 0x03, 0x00, "Memory size" )
	PORT_DIPSETTING(    0x00, "1K" )
	PORT_DIPSETTING(    0x01, "1K + 7K TANEX" )
	PORT_DIPSETTING(    0x02, "1K + 7K TANEX + 40K TANRAM" )
	PORT_BIT( 0xfc, 0xfc, IPT_UNUSED )

	PORT_START("ROW0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("ESC") PORT_CODE(KEYCODE_ESC)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("1 !") PORT_CODE(KEYCODE_1)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("2 \"") PORT_CODE(KEYCODE_2)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("3 #") PORT_CODE(KEYCODE_3)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("4 $") PORT_CODE(KEYCODE_4)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("5 %") PORT_CODE(KEYCODE_5)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("6 &") PORT_CODE(KEYCODE_6)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("7 '") PORT_CODE(KEYCODE_7)

	PORT_START("ROW1")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("8 *") PORT_CODE(KEYCODE_8)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("9 (") PORT_CODE(KEYCODE_9)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("0 )") PORT_CODE(KEYCODE_0)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("- _") PORT_CODE(KEYCODE_MINUS)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("= +") PORT_CODE(KEYCODE_EQUALS)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("` ~") PORT_CODE(KEYCODE_TILDE)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("BACK SPACE") PORT_CODE(KEYCODE_BACKSPACE)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("TAB") PORT_CODE(KEYCODE_TAB)

	PORT_START("ROW2")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Q) PORT_CHAR('q') PORT_CHAR('Q')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_W) PORT_CHAR('w') PORT_CHAR('W')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_E) PORT_CHAR('e') PORT_CHAR('E')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_R) PORT_CHAR('R') PORT_CHAR('r')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_T) PORT_CHAR('T') PORT_CHAR('t')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Y) PORT_CHAR('Y') PORT_CHAR('y')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_U) PORT_CHAR('U') PORT_CHAR('u')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_I) PORT_CHAR('I') PORT_CHAR('i')

	PORT_START("ROW3")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_O) PORT_CHAR('O') PORT_CHAR('o')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_P) PORT_CHAR('P') PORT_CHAR('p')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("[ {") PORT_CODE(KEYCODE_OPENBRACE)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("] }") PORT_CODE(KEYCODE_CLOSEBRACE)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("RETURN") PORT_CODE(KEYCODE_ENTER)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("DEL") PORT_CODE(KEYCODE_DEL)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CTRL") PORT_CODE(KEYCODE_LCONTROL)
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD ) PORT_NAME("CAPS LOCK") PORT_CODE(KEYCODE_CAPSLOCK) PORT_TOGGLE

	PORT_START("ROW4")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_A) PORT_CHAR('A') PORT_CHAR('a')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_S) PORT_CHAR('S') PORT_CHAR('s')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_D) PORT_CHAR('D') PORT_CHAR('d')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F) PORT_CHAR('F') PORT_CHAR('f')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_G) PORT_CHAR('G') PORT_CHAR('g')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_H) PORT_CHAR('H') PORT_CHAR('h')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_J) PORT_CHAR('J') PORT_CHAR('j')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_K) PORT_CHAR('K') PORT_CHAR('k')

	PORT_START("ROW5")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_L) PORT_CHAR('L') PORT_CHAR('l')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("; :") PORT_CODE(KEYCODE_COLON)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("' \"") PORT_CODE(KEYCODE_QUOTE)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\\ |") PORT_CODE(KEYCODE_ASTERISK)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("SHIFT (L)") PORT_CODE(KEYCODE_LSHIFT)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Z) PORT_CHAR('Z') PORT_CHAR('z')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_X) PORT_CHAR('X') PORT_CHAR('x')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_C) PORT_CHAR('C') PORT_CHAR('c')

	PORT_START("ROW6")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_V) PORT_CHAR('V') PORT_CHAR('v')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_B) PORT_CHAR('B') PORT_CHAR('b')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_N) PORT_CHAR('N') PORT_CHAR('n')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_M) PORT_CHAR('M') PORT_CHAR('m')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(", <") PORT_CODE(KEYCODE_COMMA)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(". >") PORT_CODE(KEYCODE_STOP)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("/ ?") PORT_CODE(KEYCODE_SLASH)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("SHIFT (R)") PORT_CODE(KEYCODE_RSHIFT)

	PORT_START("ROW7")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("LINE FEED")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("SPACE") PORT_CODE(KEYCODE_SPACE)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("- (KP)") PORT_CODE(KEYCODE_MINUS_PAD)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(", (KP)") PORT_CODE(KEYCODE_PLUS_PAD)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("ENTER (KP)") PORT_CODE(KEYCODE_ENTER_PAD)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(". (KP)") PORT_CODE(KEYCODE_DEL_PAD)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("0 (KP)") PORT_CODE(KEYCODE_0_PAD)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("1 (KP)") PORT_CODE(KEYCODE_1_PAD)

	PORT_START("ROW8")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("2 (KP)") PORT_CODE(KEYCODE_2_PAD)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("3 (KP)") PORT_CODE(KEYCODE_3_PAD)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("4 (KP)") PORT_CODE(KEYCODE_4_PAD)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("5 (KP)") PORT_CODE(KEYCODE_5_PAD)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("6 (KP)") PORT_CODE(KEYCODE_6_PAD)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("7 (KP)") PORT_CODE(KEYCODE_7_PAD)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("8 (KP)") PORT_CODE(KEYCODE_8_PAD)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("9 (KP)") PORT_CODE(KEYCODE_9_PAD)

	PORT_START("JOY") // VIA #1 PORT A
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START ) PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )  PORT_4WAY
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )    PORT_4WAY
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )  PORT_4WAY
INPUT_PORTS_END

static const gfx_layout char_layout =
{
	8, 16,      /* 8 x 16 graphics */
	128,        /* 128 codes */
	1,          /* 1 bit per pixel */
	{ 0 },      /* no bitplanes */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8,10*8,11*8,12*8,13*8,14*8,15*8 },
	8 * 16      /* code takes 8 times 16 bits */
};

static const gfx_layout chunky_layout =
{
	8, 16,      /* 8 x 16 graphics */
	256,        /* 256 codes */
	1,          /* 1 bit per pixel */
	{ 0 },      /* no bitplanes */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8,10*8,11*8,12*8,13*8,14*8,15*8 },
	8 * 16      /* code takes 8 times 16 bits */
};

static GFXDECODE_START( microtan )
	GFXDECODE_ENTRY( "gfx1", 0, char_layout, 0, 1 )
	GFXDECODE_ENTRY( "gfx2", 0, chunky_layout, 0, 1 )
GFXDECODE_END

static const ay8910_interface microtan_ay8910_interface =
{
	AY8910_LEGACY_OUTPUT,
	AY8910_DEFAULT_LOADS,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL
};

static MACHINE_CONFIG_START( microtan, microtan_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M6502, 750000)  // 750 kHz
	MCFG_CPU_PROGRAM_MAP(microtan_map)
	MCFG_CPU_VBLANK_INT_DRIVER("screen", microtan_state,  microtan_interrupt)


	/* video hardware - include overscan */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_SIZE(32*8, 16*16)
	MCFG_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 0*16, 16*16-1)
	MCFG_SCREEN_UPDATE_DRIVER(microtan_state, screen_update_microtan)

	MCFG_GFXDECODE(microtan)
	MCFG_PALETTE_LENGTH(2)

	MCFG_PALETTE_INIT_OVERRIDE(driver_device, black_and_white)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("dac", DAC, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)
	MCFG_SOUND_WAVE_ADD(WAVE_TAG, "cassette")
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
	MCFG_SOUND_ADD("ay8910.1", AY8910, 1000000)
	MCFG_SOUND_CONFIG(microtan_ay8910_interface)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
	MCFG_SOUND_ADD("ay8910.2", AY8910, 1000000)
	MCFG_SOUND_CONFIG(microtan_ay8910_interface)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)

	/* snapshot/quickload */
	MCFG_SNAPSHOT_ADD("snapshot", microtan_state, microtan, "m65", 0.5)
	MCFG_QUICKLOAD_ADD("quickload", microtan_state, microtan, "hex", 0.5)

	/* cassette */
	MCFG_CASSETTE_ADD( "cassette", default_cassette_interface )

	/* acia */
	MCFG_DEVICE_ADD("acia", MOS6551, XTAL_1_8432MHz)

	/* via */
	MCFG_DEVICE_ADD("via6522_0", VIA6522, 0)
	MCFG_VIA6522_READPA_HANDLER(READ8(microtan_state, via_0_in_a))
	MCFG_VIA6522_WRITEPA_HANDLER(WRITE8(microtan_state, via_0_out_a))
	MCFG_VIA6522_WRITEPB_HANDLER(WRITE8(microtan_state, via_0_out_b))
	MCFG_VIA6522_CA2_HANDLER(WRITELINE(microtan_state, via_0_out_ca2))
	MCFG_VIA6522_CB2_HANDLER(WRITELINE(microtan_state, via_0_out_cb2))
	MCFG_VIA6522_IRQ_HANDLER(WRITELINE(microtan_state, via_0_irq))

	MCFG_DEVICE_ADD("via6522_1", VIA6522, 0)
	MCFG_VIA6522_WRITEPA_HANDLER(WRITE8(microtan_state, via_1_out_a))
	MCFG_VIA6522_WRITEPB_HANDLER(WRITE8(microtan_state, via_1_out_b))
	MCFG_VIA6522_CA2_HANDLER(WRITELINE(microtan_state, via_1_out_ca2))
	MCFG_VIA6522_CB2_HANDLER(WRITELINE(microtan_state, via_1_out_cb2))
	MCFG_VIA6522_IRQ_HANDLER(WRITELINE(microtan_state, via_1_irq))
MACHINE_CONFIG_END

ROM_START( microtan )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "tanex_j2.rom", 0xc000, 0x1000, CRC(3e09d384) SHA1(15a98941a672ff16242cc73f1dcf1d81fccd8910) )
	ROM_LOAD( "tanex_h2.rom", 0xd000, 0x1000, CRC(75105113) SHA1(c6fea4d65b7c52f43aa1589cace9467349a0f290) )
	ROM_LOAD( "tanex_d3.rom", 0xe000, 0x0800, CRC(ee6e8412) SHA1(7e1bca84bab79d94a4ab8554d23e2bc28ccd0384) )
	ROM_LOAD( "tanex_e2.rom", 0xe800, 0x0800, CRC(bd87fd34) SHA1(f41895df4a733dddfaf1c89ecff5040addcab804) )
	ROM_LOAD( "tanex_g2.rom", 0xf000, 0x0800, CRC(9fd233ee) SHA1(7b0be2d0402229ec80b062f7a0bed793686bcbf9) )
	ROM_LOAD( "tanbug_2.rom", 0xf800, 0x0400, CRC(7e215313) SHA1(c8fb3d33ce2beaf624dc75ec57d34c216b086274) )
	ROM_LOAD( "tanbug.rom",   0xfc00, 0x0400, CRC(c8221d9e) SHA1(c7fe4c174523aaaab30be7a8c9baf2bc08b33968) )

	ROM_REGION( 0x00800, "gfx1", 0 )
	ROM_LOAD( "charset.rom",  0x0000, 0x0800, CRC(3b3c5360) SHA1(a3a2f74149107f8b8f35b15069c71f3aa843d12f) )

	ROM_REGION( 0x01000, "gfx2", ROMREGION_ERASEFF )
	// initialized in init_microtan
ROM_END


//    YEAR  NAME      PARENT    COMPAT  MACHINE   INPUT     INIT      COMPANY      FULLNAME
COMP( 1979, microtan, 0,        0,      microtan, microtan, microtan_state, microtan, "Tangerine", "Microtan 65" , 0)
