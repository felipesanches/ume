/***************************************************************************

    commodore c65 home computer
    PeT mess@utanet.at

    documention
     www.funet.fi

***************************************************************************/

/*

2008 - Driver Updates
---------------------

(most of the informations are taken from http://www.zimmers.net/cbmpics/ )


[CBM systems which belong to this driver]

* Commodore 65 (1989)

Also known as C64 DX at early stages of the project. It was cancelled
around 1990-1991. Only few units survive (they were sold after Commodore
liquidation in 1994).

CPU: CSG 4510 (3.54 MHz)
RAM: 128 kilobytes, expandable to 8 megabytes
ROM: 128 kilobytes
Video: CSG 4569 "VIC-III" (6 Video modes; Resolutions from 320x200 to
    1280x400; 80 columns text; Palette of 4096 colors)
Sound: CSG 8580 "SID" x2 (6 voice stereo synthesizer/digital sound
    capabilities)
Ports: CSG 4510 (2 Joystick/Mouse ports; CBM Serial port; CBM 'USER'
    port; CBM Monitor port; Power and reset switches; C65 bus drive
    port; RGBI video port; 2 RCA audio ports; RAM expansion port; C65
    expansion port)
Keyboard: Full-sized 77 key QWERTY (12 programmable function keys;
    4 direction cursor-pad)
Additional Hardware: Built in 3.5" DD disk drive (1581 compatible)
Miscellaneous: Partially implemented Commodore 64 emulation

[Notes]

The datasette port was removed here. C65 supports an additional "dumb"
drive externally. It also features, in addition to the standard CBM
bus serial (available in all modes), a Fast and a Burst serial bus
(both available in C65 mode only)

*/


#include "emu.h"
#include "cpu/m6502/m4510.h"
#include "sound/mos6581.h"
#include "machine/mos6526.h"
#include "video/vic4567.h"
#include "machine/cbm_snqk.h"
#include "includes/c65.h"
#include "bus/cbmiec/cbmiec.h"
#include "machine/ram.h"

static void cbm_c65_quick_sethiaddress( running_machine &machine, UINT16 hiaddress )
{
	address_space &space = machine.firstcpu->space(AS_PROGRAM);

	space.write_byte(0x82, hiaddress & 0xff);
	space.write_byte(0x83, hiaddress >> 8);
}

QUICKLOAD_LOAD_MEMBER( c65_state, cbm_c65 )
{
	return general_cbm_loadsnap(image, file_type, quickload_size, m_maincpu->space(AS_PROGRAM), 0, cbm_c65_quick_sethiaddress);
}

/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

static ADDRESS_MAP_START( c65_mem , AS_PROGRAM, 8, c65_state )
	AM_RANGE(0x00000, 0x07fff) AM_RAMBANK("bank11")
	AM_RANGE(0x08000, 0x09fff) AM_READ_BANK("bank1") AM_WRITE_BANK("bank12")
	AM_RANGE(0x0a000, 0x0bfff) AM_READ_BANK("bank2") AM_WRITE_BANK("bank13")
	AM_RANGE(0x0c000, 0x0cfff) AM_READ_BANK("bank3") AM_WRITE_BANK("bank14")
	AM_RANGE(0x0d000, 0x0d7ff) AM_READ_BANK("bank4") AM_WRITE_BANK("bank5")
	AM_RANGE(0x0d800, 0x0dbff) AM_READ_BANK("bank6") AM_WRITE_BANK("bank7")
	AM_RANGE(0x0dc00, 0x0dfff) AM_READ_BANK("bank8") AM_WRITE_BANK("bank9")
	AM_RANGE(0x0e000, 0x0ffff) AM_READ_BANK("bank10") AM_WRITE_BANK("bank15")
	AM_RANGE(0x10000, 0x1f7ff) AM_RAM
	AM_RANGE(0x1f800, 0x1ffff) AM_RAM AM_SHARE("colorram")

	AM_RANGE(0x20000, 0x23fff) AM_ROM /* &c65_dos,     maps to 0x8000    */
	AM_RANGE(0x24000, 0x28fff) AM_ROM /* reserved */
	AM_RANGE(0x29000, 0x29fff) AM_ROM AM_SHARE("c65_chargen")
	AM_RANGE(0x2a000, 0x2bfff) AM_ROM AM_SHARE("basic")
	AM_RANGE(0x2c000, 0x2cfff) AM_ROM AM_SHARE("interface")
	AM_RANGE(0x2d000, 0x2dfff) AM_ROM AM_SHARE("chargen")
	AM_RANGE(0x2e000, 0x2ffff) AM_ROM AM_SHARE("kernal")

	AM_RANGE(0x30000, 0x31fff) AM_ROM /*&c65_monitor,     monitor maps to 0x6000    */
	AM_RANGE(0x32000, 0x37fff) AM_ROM /*&c65_basic, */
	AM_RANGE(0x38000, 0x3bfff) AM_ROM /*&c65_graphics, */
	AM_RANGE(0x3c000, 0x3dfff) AM_ROM /* reserved */
	AM_RANGE(0x3e000, 0x3ffff) AM_ROM /* &c65_kernal, */

	AM_RANGE(0x40000, 0x7ffff) AM_NOP
	/* 8 megabyte full address space! */
ADDRESS_MAP_END


/*************************************
 *
 *  Input Ports
 *
 *************************************/

static INPUT_PORTS_START( c65 )
	PORT_START( "ROW0" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Crsr Down Up") PORT_CODE(KEYCODE_RALT)        PORT_CHAR(UCHAR_MAMEKEY(DOWN)) PORT_CHAR(UCHAR_MAMEKEY(UP))
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F3)                                    PORT_CHAR(UCHAR_MAMEKEY(F5))
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F2)                                    PORT_CHAR(UCHAR_MAMEKEY(F3))
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F1)                                    PORT_CHAR(UCHAR_MAMEKEY(F1))
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F4)                                    PORT_CHAR(UCHAR_MAMEKEY(F7))
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Crsr Right Left") PORT_CODE(KEYCODE_RCONTROL) PORT_CHAR(UCHAR_MAMEKEY(RIGHT)) PORT_CHAR(UCHAR_MAMEKEY(LEFT))
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Return") PORT_CODE(KEYCODE_ENTER)             PORT_CHAR(13)
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("INST DEL") PORT_CODE(KEYCODE_BACKSPACE)       PORT_CHAR(8) PORT_CHAR(UCHAR_MAMEKEY(INSERT))

	PORT_START( "ROW1" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Shift (Left)") PORT_CODE(KEYCODE_LSHIFT)      PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_E)         PORT_CHAR('E')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_S)         PORT_CHAR('S')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Z)         PORT_CHAR('Z')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_4)         PORT_CHAR('4') PORT_CHAR('$')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_A)         PORT_CHAR('A')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_W)         PORT_CHAR('W')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_3)         PORT_CHAR('3') PORT_CHAR('#')

	PORT_START( "ROW2" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_X)         PORT_CHAR('X')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_T)         PORT_CHAR('T')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F)         PORT_CHAR('F')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_C)         PORT_CHAR('C')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_6)         PORT_CHAR('6') PORT_CHAR('&')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_D)         PORT_CHAR('D')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_R)         PORT_CHAR('R')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_5)         PORT_CHAR('5') PORT_CHAR('%')

	PORT_START( "ROW3" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_V)         PORT_CHAR('V')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_U)         PORT_CHAR('U')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_H)         PORT_CHAR('H')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_B)         PORT_CHAR('B')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_8)         PORT_CHAR('8') PORT_CHAR('(')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_G)         PORT_CHAR('G')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Y)         PORT_CHAR('Y')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_7)         PORT_CHAR('7') PORT_CHAR('\'')

	PORT_START( "ROW4" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_N)         PORT_CHAR('N')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_O)         PORT_CHAR('O')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_K)         PORT_CHAR('K')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_M)         PORT_CHAR('M')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_0)         PORT_CHAR('0')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_J)         PORT_CHAR('J')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_I)         PORT_CHAR('I')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_9)         PORT_CHAR('9') PORT_CHAR(')')

	PORT_START( "ROW5" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_COMMA)     PORT_CHAR(',') PORT_CHAR('<')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR('@')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_COLON)     PORT_CHAR(':') PORT_CHAR('[')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_STOP)      PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_EQUALS)    PORT_CHAR('-')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_L)         PORT_CHAR('L')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_P)         PORT_CHAR('P')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_MINUS)     PORT_CHAR('+')

	PORT_START( "ROW6" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_SLASH)                             PORT_CHAR('/') PORT_CHAR('?')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xE2\x86\x91  Pi") PORT_CODE(KEYCODE_DEL) PORT_CHAR(0x2191) PORT_CHAR(0x03C0)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_BACKSLASH)                         PORT_CHAR('=')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Shift (Right)") PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CLR HOME") PORT_CODE(KEYCODE_INSERT)      PORT_CHAR(UCHAR_MAMEKEY(HOME))
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_QUOTE)                             PORT_CHAR(';') PORT_CHAR(']')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_CLOSEBRACE)                        PORT_CHAR('*')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_BACKSLASH2)                        PORT_CHAR('\xA3')

	PORT_START( "ROW7" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("RUN STOP") PORT_CODE(KEYCODE_HOME)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Q)                                 PORT_CHAR('Q')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CBM") PORT_CODE(KEYCODE_LALT)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_SPACE)                             PORT_CHAR(' ')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_2)                                 PORT_CHAR('2') PORT_CHAR('"')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_TAB)                               PORT_CHAR(UCHAR_SHIFT_2)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xE2\x86\x90") PORT_CODE(KEYCODE_TILDE)   PORT_CHAR(0x2190)
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_1)                                 PORT_CHAR('1') PORT_CHAR('!')
	PORT_START("FUNCT")
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("ESC") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F13 F14") PORT_CODE(KEYCODE_F11)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F11 F12") PORT_CODE(KEYCODE_F10)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("F9 F10") PORT_CODE(KEYCODE_F9)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("HELP") PORT_CODE(KEYCODE_F12)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("ALT") PORT_CODE(KEYCODE_F2)       /* non blocking */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("TAB") PORT_CODE(KEYCODE_TAB)
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("NO SCRL") PORT_CODE(KEYCODE_F4)

	PORT_START( "SPECIAL" )  /* special keys */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Restore") PORT_CODE(KEYCODE_PRTSCR)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Shift Lock (switch)") PORT_CODE(KEYCODE_CAPSLOCK) PORT_TOGGLE PORT_CHAR(UCHAR_MAMEKEY(CAPSLOCK))
	PORT_CONFNAME( 0x20, 0x00, "(C65) Caps Lock (switch)") PORT_CODE(KEYCODE_F3)
	PORT_CONFSETTING(   0x00, DEF_STR( Off ) )
	PORT_CONFSETTING(   0x20, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("CTRLSEL")   /* Controller selection */
	PORT_CONFNAME( 0x07, 0x00, "Gameport A" )
	PORT_CONFSETTING( 0x00, DEF_STR( Joystick ) )
	PORT_CONFSETTING( 0x01, "Paddles 1 & 2" )
	PORT_CONFSETTING( 0x02, "Mouse 1351" )
	PORT_CONFSETTING( 0x03, "Mouse (Non Proportional) 1350" )
	PORT_CONFSETTING( 0x04, "Lightpen" )
//  PORT_CONFSETTING( 0x05, "Koala Pad" )
	PORT_CONFSETTING( 0x06, "Lightgun" )
	PORT_CONFSETTING( 0x07, "No Device Connected" )
	PORT_CONFNAME( 0x70, 0x00, "Gameport B" )
	PORT_CONFSETTING( 0x00, DEF_STR( Joystick ) )
	PORT_CONFSETTING( 0x10, "Paddles 3 & 4" )
//  PORT_CONFSETTING( 0x20, "Mouse 1351" )
//  PORT_CONFSETTING( 0x30, "Mouse (Non Proportional) 1350" )
	PORT_CONFSETTING( 0x70, "No Device Connected" )
	PORT_CONFNAME( 0x80, 0x00, "Swap Gameport A and B") PORT_CODE(KEYCODE_F1) PORT_TOGGLE
	PORT_CONFSETTING(   0x00, DEF_STR( No ) )
	PORT_CONFSETTING(   0x80, DEF_STR( Yes ) )

	PORT_START("JOY1_1B")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x00)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x00)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x00)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x00)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x00)
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("JOY2_1B")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_HOME) PORT_CODE(JOYCODE_Y_UP_SWITCH) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x00)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_END) PORT_CODE(JOYCODE_Y_DOWN_SWITCH) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x00)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_DEL) PORT_CODE(JOYCODE_X_LEFT_SWITCH) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x00)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_PGDN) PORT_CODE(JOYCODE_X_RIGHT_SWITCH) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x00)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_CODE(KEYCODE_INSERT) PORT_CODE(JOYCODE_BUTTON1) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x00)
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	/* Mouse Commodore 1350 was basically working as a Joystick */
	PORT_START("JOY1_2B")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1) PORT_NAME("Mouse 1350 Up") PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x03)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1) PORT_NAME("Mouse 1350 Down") PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x03)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1) PORT_NAME("Mouse 1350 Left") PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x03)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1) PORT_NAME("Mouse 1350 Right") PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x03)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(1) PORT_NAME("Mouse 1350 Button 1") PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x03)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(1) PORT_NAME("Mouse 1350 Button 2") PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x03)
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNUSED )

	/* Still to verify how many mices you were able to plug into a c64 */
	/* Only one, for now */
	PORT_START("JOY2_2B")
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )
/*  PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_HOME) PORT_CODE(JOYCODE_Y_UP_SWITCH)
    PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_END) PORT_CODE(JOYCODE_Y_DOWN_SWITCH) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x30)
    PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_DEL) PORT_CODE(JOYCODE_X_LEFT_SWITCH) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x30)
    PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2) PORT_CODE(KEYCODE_PGDN) PORT_CODE(JOYCODE_X_RIGHT_SWITCH) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x30)
    PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_CODE(KEYCODE_INSERT) PORT_CODE(JOYCODE_BUTTON1) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x30)
    PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(2) PORT_CODE(KEYCODE_PGUP) PORT_CODE(JOYCODE_BUTTON2) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x30)
    PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNUSED )
*/
	PORT_START("PADDLE1")
	PORT_BIT( 0xff, 128, IPT_PADDLE) PORT_SENSITIVITY(30) PORT_KEYDELTA(20) PORT_MINMAX(0,255) PORT_CENTERDELTA(0) PORT_CODE_DEC(KEYCODE_LEFT) PORT_CODE_INC(KEYCODE_RIGHT) PORT_CODE_DEC(JOYCODE_X_LEFT_SWITCH) PORT_CODE_INC(JOYCODE_X_RIGHT_SWITCH) PORT_PLAYER(1) PORT_REVERSE PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x01)

	PORT_START("PADDLE2")
	PORT_BIT( 0xff, 128, IPT_PADDLE) PORT_SENSITIVITY(30) PORT_KEYDELTA(20) PORT_MINMAX(0,255) PORT_CENTERDELTA(0) PORT_CODE_DEC(KEYCODE_DOWN) PORT_CODE_INC(KEYCODE_UP) PORT_CODE_DEC(JOYCODE_Y_UP_SWITCH) PORT_CODE_INC(JOYCODE_Y_DOWN_SWITCH) PORT_PLAYER(2) PORT_REVERSE PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x01)

	PORT_START("PADDLE3")
	PORT_BIT( 0xff, 128, IPT_PADDLE) PORT_SENSITIVITY(30) PORT_KEYDELTA(20) PORT_MINMAX(0,255) PORT_CENTERDELTA(0) PORT_CODE_DEC(KEYCODE_HOME) PORT_CODE_INC(KEYCODE_PGUP) PORT_PLAYER(3) PORT_REVERSE PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x10)

	PORT_START("PADDLE4")
	PORT_BIT( 0xff, 128, IPT_PADDLE) PORT_SENSITIVITY(30) PORT_KEYDELTA(20) PORT_MINMAX(0,255) PORT_CENTERDELTA(0) PORT_CODE_DEC(KEYCODE_END) PORT_CODE_INC(KEYCODE_PGDN) PORT_PLAYER(4) PORT_REVERSE PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x10)

	PORT_START("TRACKX")
	PORT_BIT( 0x7e, 0x00, IPT_TRACKBALL_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(0) PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x02)

	PORT_START("TRACKY")
	PORT_BIT( 0x7e, 0x00, IPT_TRACKBALL_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(0) PORT_PLAYER(1) PORT_REVERSE PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x02)

	PORT_START("LIGHTX")
	PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_X) PORT_NAME("Lightpen X Axis") PORT_CROSSHAIR(X, 1.0, 0.0, 0) PORT_SENSITIVITY(30) PORT_KEYDELTA(20) PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x04)

	PORT_START("LIGHTY")
	PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_Y) PORT_NAME("Lightpen Y Axis") PORT_CROSSHAIR(Y, 1.0, 0.0, 0) PORT_SENSITIVITY(30) PORT_KEYDELTA(20) PORT_PLAYER(1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x04)

	PORT_START("OTHER")
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Paddle 1 Button") PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(JOYCODE_BUTTON1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x01)
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Paddle 2 Button") PORT_CODE(KEYCODE_LALT) PORT_CODE(JOYCODE_BUTTON2) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x01)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("Paddle 3 Button") PORT_CODE(KEYCODE_INSERT) PORT_CODE(JOYCODE_BUTTON1) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x10)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON4) PORT_NAME("Paddle 4 Button") PORT_CODE(KEYCODE_DEL) PORT_CODE(JOYCODE_BUTTON2) PORT_CONDITION("CTRLSEL", 0xf0, EQUALS, 0x10)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Lightpen Signal") PORT_CODE(KEYCODE_LCONTROL) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x04)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Mouse Button Left") PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(JOYCODE_BUTTON1) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x02)
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Mouse Button Right") PORT_CODE(KEYCODE_LALT) PORT_CODE(JOYCODE_BUTTON2) PORT_CONDITION("CTRLSEL", 0x0f, EQUALS, 0x02)INPUT_PORTS_END


static INPUT_PORTS_START( c65ger )
	PORT_INCLUDE( c65 )

	PORT_MODIFY( "ROW1" )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Z  { Y }") PORT_CODE(KEYCODE_Z)                   PORT_CHAR('Z')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("3  #  { 3  Paragraph }") PORT_CODE(KEYCODE_3)     PORT_CHAR('3') PORT_CHAR('#')

	PORT_MODIFY( "ROW3" )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Y  { Z }") PORT_CODE(KEYCODE_Y)                   PORT_CHAR('Y')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("7 ' { 7  / }") PORT_CODE(KEYCODE_7)               PORT_CHAR('7') PORT_CHAR('\'')

	PORT_MODIFY( "ROW4" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("0  { = }") PORT_CODE(KEYCODE_0)                   PORT_CHAR('0')

	PORT_MODIFY( "ROW5" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(",  <  { ; }") PORT_CODE(KEYCODE_COMMA)            PORT_CHAR(',') PORT_CHAR('<')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Paragraph  \xE2\x86\x91  { \xc3\xbc }") PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR(0x00A7) PORT_CHAR(0x2191)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(":  [  { \xc3\xa4 }") PORT_CODE(KEYCODE_COLON)     PORT_CHAR(':') PORT_CHAR('[')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(".  >  { : }") PORT_CODE(KEYCODE_STOP)             PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("-  { '  ` }") PORT_CODE(KEYCODE_EQUALS)           PORT_CHAR('-')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("+  { \xc3\x9f ? }") PORT_CODE(KEYCODE_MINUS)      PORT_CHAR('+')

	PORT_MODIFY( "ROW6" )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("/  ?  { -  _ }") PORT_CODE(KEYCODE_SLASH)         PORT_CHAR('/') PORT_CHAR('?')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("Sum Pi  { ]  \\ }") PORT_CODE(KEYCODE_DEL)        PORT_CHAR(0x03A3) PORT_CHAR(0x03C0)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("=  { #  ' }") PORT_CODE(KEYCODE_BACKSLASH)        PORT_CHAR('=')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME(";  ]  { \xc3\xb6 }") PORT_CODE(KEYCODE_QUOTE)     PORT_CHAR(';') PORT_CHAR(']')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("*  `  { +  * }") PORT_CODE(KEYCODE_CLOSEBRACE)    PORT_CHAR('*') PORT_CHAR('`')
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\\  { [ \xE2\x86\x91 }") PORT_CODE(KEYCODE_BACKSLASH2) PORT_CHAR('\xa3')

	PORT_MODIFY( "ROW7" )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("_  { <  > }") PORT_CODE(KEYCODE_TILDE)            PORT_CHAR('_')

	PORT_MODIFY("SPECIAL") /* special keys */
	PORT_DIPNAME( 0x20, 0x00, "(C65) DIN ASC (switch)") PORT_CODE(KEYCODE_F3)
	PORT_DIPSETTING(    0x00, "ASC" )
	PORT_DIPSETTING(    0x20, "DIN" )
INPUT_PORTS_END



/*************************************
 *
 *  Graphics definitions
 *
 *************************************/

PALETTE_INIT_MEMBER(c65_state,c65)
{
	int i;

	for ( i = 0; i < 0x100; i++ )
	{
		palette_set_color_rgb(machine(), i, 0, 0, 0);
	}
}


/*************************************
 *
 *  Sound definitions
 *
 *************************************/

int c65_state::c64_paddle_read( device_t *device, address_space &space, int which )
{
	int pot1 = 0xff, pot2 = 0xff, pot3 = 0xff, pot4 = 0xff, temp;
	UINT8 cia0porta = machine().device<mos6526_device>("cia_0")->pa_r(space, 0);
	int controller1 = ioport("CTRLSEL")->read() & 0x07;
	int controller2 = ioport("CTRLSEL")->read() & 0x70;
	/* Notice that only a single input is defined for Mouse & Lightpen in both ports */
	switch (controller1)
	{
		case 0x01:
			if (which)
				pot2 = ioport("PADDLE2")->read();
			else
				pot1 = ioport("PADDLE1")->read();
			break;

		case 0x02:
			if (which)
				pot2 = ioport("TRACKY")->read();
			else
				pot1 = ioport("TRACKX")->read();
			break;

		case 0x03:
			if (which && (ioport("JOY1_2B")->read() & 0x20))  /* Joy1 Button 2 */
				pot1 = 0x00;
			break;

		case 0x04:
			if (which)
				pot2 = ioport("LIGHTY")->read();
			else
				pot1 = ioport("LIGHTX")->read();
			break;

		case 0x06:
			if (which && (ioport("OTHER")->read() & 0x04))    /* Lightpen Signal */
				pot2 = 0x00;
			break;

		case 0x00:
		case 0x07:
			break;

		default:
			logerror("Invalid Controller Setting %d\n", controller1);
			break;
	}

	switch (controller2)
	{
		case 0x10:
			if (which)
				pot4 = ioport("PADDLE4")->read();
			else
				pot3 = ioport("PADDLE3")->read();
			break;

		case 0x20:
			if (which)
				pot4 = ioport("TRACKY")->read();
			else
				pot3 = ioport("TRACKX")->read();
			break;

		case 0x30:
			if (which && (ioport("JOY2_2B")->read() & 0x20))  /* Joy2 Button 2 */
				pot4 = 0x00;
			break;

		case 0x40:
			if (which)
				pot4 = ioport("LIGHTY")->read();
			else
				pot3 = ioport("LIGHTX")->read();
			break;

		case 0x60:
			if (which && (ioport("OTHER")->read() & 0x04))    /* Lightpen Signal */
				pot4 = 0x00;
			break;

		case 0x00:
		case 0x70:
			break;

		default:
			logerror("Invalid Controller Setting %d\n", controller1);
			break;
	}

	if (ioport("CTRLSEL")->read() & 0x80)     /* Swap */
	{
		temp = pot1; pot1 = pot3; pot3 = temp;
		temp = pot2; pot2 = pot4; pot4 = temp;
	}

	switch (cia0porta & 0xc0)
	{
		case 0x40:
			return which ? pot2 : pot1;

		case 0x80:
			return which ? pot4 : pot3;

		case 0xc0:
			return which ? pot2 : pot1;

		default:
			return 0;
	}
}

READ8_MEMBER( c65_state::sid_potx_r )
{
	device_t *sid = machine().device("sid_r");

	return c64_paddle_read(sid, space, 0);
}

READ8_MEMBER( c65_state::sid_poty_r )
{
	device_t *sid = machine().device("sid_r");

	return c64_paddle_read(sid, space, 1);
}


/*************************************
 *
 *  VIC III interfaces
 *
 *************************************/

UINT32 c65_state::screen_update_c65(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	machine().device<vic3_device>("vic3")->video_update(bitmap, cliprect);
	return 0;
}

READ8_MEMBER(c65_state::c65_lightpen_x_cb)
{
	return ioport("LIGHTX")->read() & ~0x01;
}

READ8_MEMBER(c65_state::c65_lightpen_y_cb)
{
	return ioport("LIGHTY")->read() & ~0x01;
}

READ8_MEMBER(c65_state::c65_lightpen_button_cb)
{
	return ioport("OTHER")->read() & 0x04;
}

READ8_MEMBER(c65_state::c65_c64_mem_r)
{
	return m_memory[offset];
}

static const vic3_interface c65_vic3_ntsc_intf = {
	"maincpu",
	VIC4567_NTSC,
	DEVCB_DRIVER_MEMBER(c65_state,c65_lightpen_x_cb),
	DEVCB_DRIVER_MEMBER(c65_state,c65_lightpen_y_cb),
	DEVCB_DRIVER_MEMBER(c65_state,c65_lightpen_button_cb),
	DEVCB_DRIVER_MEMBER(c65_state,c65_dma_read),
	DEVCB_DRIVER_MEMBER(c65_state,c65_dma_read_color),
	DEVCB_DRIVER_LINE_MEMBER(c65_state,c65_vic_interrupt),
	DEVCB_DRIVER_MEMBER(c65_state,c65_bankswitch_interface),
	DEVCB_DRIVER_MEMBER(c65_state,c65_c64_mem_r)
};

static const vic3_interface c65_vic3_pal_intf = {
	"maincpu",
	VIC4567_PAL,
	DEVCB_DRIVER_MEMBER(c65_state,c65_lightpen_x_cb),
	DEVCB_DRIVER_MEMBER(c65_state,c65_lightpen_y_cb),
	DEVCB_DRIVER_MEMBER(c65_state,c65_lightpen_button_cb),
	DEVCB_DRIVER_MEMBER(c65_state,c65_dma_read),
	DEVCB_DRIVER_MEMBER(c65_state,c65_dma_read_color),
	DEVCB_DRIVER_LINE_MEMBER(c65_state,c65_vic_interrupt),
	DEVCB_DRIVER_MEMBER(c65_state,c65_bankswitch_interface),
	DEVCB_DRIVER_MEMBER(c65_state,c65_c64_mem_r)
};

INTERRUPT_GEN_MEMBER(c65_state::vic3_raster_irq)
{
	machine().device<vic3_device>("vic3")->raster_interrupt_gen();
}

/*************************************
 *
 *  Machine driver
 *
 *************************************/

static MACHINE_CONFIG_START( c65, c65_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M4510, 3500000)  /* or VIC6567_CLOCK, */
	MCFG_CPU_PROGRAM_MAP(c65_mem)
	MCFG_CPU_VBLANK_INT_DRIVER("screen", c65_state,  c65_frame_interrupt)
	MCFG_CPU_PERIODIC_INT_DRIVER(c65_state, vic3_raster_irq,  VIC6567_HRETRACERATE)

	MCFG_MACHINE_START_OVERRIDE(c65_state, c65 )

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(VIC6567_VRETRACERATE)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_SIZE(525 * 2, 520 * 2)
	MCFG_SCREEN_VISIBLE_AREA(VIC6567_STARTVISIBLECOLUMNS ,(VIC6567_STARTVISIBLECOLUMNS + VIC6567_VISIBLECOLUMNS - 1) * 2, VIC6567_STARTVISIBLELINES, VIC6567_STARTVISIBLELINES + VIC6567_VISIBLELINES - 1)
	MCFG_SCREEN_UPDATE_DRIVER(c65_state, screen_update_c65)

	MCFG_PALETTE_LENGTH(0x100)
	MCFG_PALETTE_INIT_OVERRIDE(c65_state, c65 )

	MCFG_VIC3_ADD("vic3", c65_vic3_ntsc_intf)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")
	MCFG_SOUND_ADD("sid_r", MOS8580, 985248)
	MCFG_MOS6581_POTXY_CALLBACKS(READ8(c65_state, sid_potx_r), READ8(c65_state, sid_poty_r))
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.50)
	MCFG_SOUND_ADD("sid_l", MOS8580, 985248)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.50)

	/* quickload */
	MCFG_QUICKLOAD_ADD("quickload", c65_state, cbm_c65, "p00,prg", CBM_QUICKLOAD_DELAY_SECONDS)

	/* cia */
	MCFG_MOS6526_ADD("cia_0", 3500000, 60, WRITELINE(c65_state, c65_cia0_interrupt))
	MCFG_MOS6526_PORT_A_CALLBACKS(READ8(c65_state, c65_cia0_port_a_r), NULL)
	MCFG_MOS6526_PORT_B_CALLBACKS(READ8(c65_state, c65_cia0_port_b_r), WRITE8(c65_state, c65_cia0_port_b_w), NULL)

	MCFG_MOS6526_ADD("cia_1", 3500000, 60, WRITELINE(c65_state, c65_cia1_interrupt))
	MCFG_MOS6526_PORT_A_CALLBACKS(READ8(c65_state, c65_cia1_port_a_r), WRITE8(c65_state, c65_cia1_port_a_w))

	/* floppy from serial bus */
	MCFG_CBM_IEC_ADD(NULL)

	/* internal ram */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("128K")
	MCFG_RAM_EXTRA_OPTIONS("640K,4224K")
MACHINE_CONFIG_END

static MACHINE_CONFIG_DERIVED( c65pal, c65 )
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_REFRESH_RATE(VIC6569_VRETRACERATE)
	MCFG_SCREEN_SIZE(625 * 2, 520 * 2)
	MCFG_SCREEN_VISIBLE_AREA(VIC6569_STARTVISIBLECOLUMNS, (VIC6569_STARTVISIBLECOLUMNS + VIC6569_VISIBLECOLUMNS - 1) * 2, VIC6569_STARTVISIBLELINES, VIC6569_STARTVISIBLELINES + VIC6569_VISIBLELINES - 1)

	MCFG_DEVICE_REMOVE("vic3")
	MCFG_VIC3_ADD("vic3", c65_vic3_pal_intf)

	/* sound hardware */
	MCFG_SOUND_REPLACE("sid_r", MOS8580, 1022727)
	MCFG_MOS6581_POTXY_CALLBACKS(READ8(c65_state, sid_potx_r), READ8(c65_state, sid_poty_r))
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.50)
	MCFG_SOUND_REPLACE("sid_l", MOS8580, 1022727)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.50)

	/* cia */
	MCFG_DEVICE_REMOVE("cia_0")
	MCFG_DEVICE_REMOVE("cia_1")
	MCFG_MOS6526_ADD("cia_0", 3500000, 50, WRITELINE(c65_state, c65_cia0_interrupt))
	MCFG_MOS6526_PORT_A_CALLBACKS(READ8(c65_state, c65_cia0_port_a_r), NULL)
	MCFG_MOS6526_PORT_B_CALLBACKS(READ8(c65_state, c65_cia0_port_b_r), WRITE8(c65_state, c65_cia0_port_b_w), NULL)

	MCFG_MOS6526_ADD("cia_1", 3500000, 50, WRITELINE(c65_state, c65_cia1_interrupt))
	MCFG_MOS6526_PORT_A_CALLBACKS(READ8(c65_state, c65_cia1_port_a_r), WRITE8(c65_state, c65_cia1_port_a_w))
MACHINE_CONFIG_END


/*************************************
 *
 *  ROM definition(s)
 *
 *************************************/


ROM_START( c65 )
	ROM_REGION( 0x400000, "maincpu", 0 )
	ROM_SYSTEM_BIOS( 0, "910111", "V0.9.910111" )
	ROMX_LOAD( "910111.bin", 0x20000, 0x20000, CRC(c5d8d32e) SHA1(71c05f098eff29d306b0170e2c1cdeadb1a5f206), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "910523", "V0.9.910523" )
	ROMX_LOAD( "910523.bin", 0x20000, 0x20000, CRC(e8235dd4) SHA1(e453a8e7e5b95de65a70952e9d48012191e1b3e7), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "910626", "V0.9.910626" )
	ROMX_LOAD( "910626.bin", 0x20000, 0x20000, CRC(12527742) SHA1(07c185b3bc58410183422f7ac13a37ddd330881b), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "910828", "V0.9.910828" )
	ROMX_LOAD( "910828.bin", 0x20000, 0x20000, CRC(3ee40b06) SHA1(b63d970727a2b8da72a0a8e234f3c30a20cbcb26), ROM_BIOS(4) )
	ROM_SYSTEM_BIOS( 4, "911001", "V0.9.911001" )
	ROMX_LOAD( "911001.bin", 0x20000, 0x20000, CRC(0888b50f) SHA1(129b9a2611edaebaa028ac3e3f444927c8b1fc5d), ROM_BIOS(5) )
ROM_END

ROM_START( c64dx )
	ROM_REGION( 0x400000, "maincpu", 0 )
	ROM_LOAD( "910429.bin", 0x20000, 0x20000, CRC(b025805c) SHA1(c3b05665684f74adbe33052a2d10170a1063ee7d) )
ROM_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

/*    YEAR  NAME    PARENT  COMPAT  MACHINE INPUT   INIT    COMPANY                         FULLNAME                                              FLAGS */

COMP( 1991, c65,    0,      0,      c65,    c65, c65_state,    c65,    "Commodore Business Machines",  "Commodore 65 Development System (Prototype, NTSC)", GAME_NOT_WORKING )
COMP( 1991, c64dx,  c65,    0,      c65pal, c65ger, c65_state, c65pal, "Commodore Business Machines",  "Commodore 64DX Development System (Prototype, PAL, German)", GAME_NOT_WORKING )
