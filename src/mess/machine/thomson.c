/**********************************************************************

  Copyright (C) Antoine Mine' 2006

  Thomson 8-bit computers

**********************************************************************/

#include "emu.h"
#include "machine/thomflop.h"
#include "formats/thom_dsk.h"
#include "includes/thomson.h"
#include "machine/6821pia.h"
#include "bus/centronics/ctronics.h"
#include "machine/ram.h"

#define VERBOSE       0
#define VERBOSE_IRQ   0
#define VERBOSE_KBD   0  /* TO8 / TO9 / TO9+ keyboard */
#define VERBOSE_BANK  0
#define VERBOSE_VIDEO 0  /* video & lightpen */
#define VERBOSE_IO    1  /* serial & parallel I/O */
#define VERBOSE_MIDI  0

#define PRINT(x) mame_printf_info x

#define LOG(x)  do { if (VERBOSE) logerror x; } while (0)
#define VLOG(x) do { if (VERBOSE > 1) logerror x; } while (0)
#define LOG_IRQ(x) do { if (VERBOSE_IRQ) logerror x; } while (0)
#define LOG_KBD(x) do { if (VERBOSE_KBD) logerror x; } while (0)
#define LOG_BANK(x) do { if (VERBOSE_BANK) logerror x; } while (0)
#define LOG_VIDEO(x) do { if (VERBOSE_VIDEO) logerror x; } while (0)
#define LOG_IO(x) do { if (VERBOSE_IO) logerror x; } while (0)
#define LOG_MIDI(x) do { if (VERBOSE_MIDI) logerror x; } while (0)

/* This set to 1 handle the .k7 files without passing through .wav */
/* It must be set accordingly in formats/thom_cas.c */
#define K7_SPEED_HACK 0


/*-------------- TO7 ------------*/


/* On the TO7 & compatible (TO7/70,TO8,TO9, but not MO5,MO6), bits are coded
   in FM format with a 1.1 ms period (900 bauds):
   - 0 is 5 periods at 4.5 kHz
   - 1 is 7 periods at 6.3 kHz

   Moreover, a byte is represented using 11 bits:
   - one 0 start bit
   - eight data bits (low bit first)
   - two 1 stop bits

   There are also long (1 s) sequences of 1 bits to re-synchronize the
   cassette at places the motor can be cut off and back on (e.g., between
   files).

   The computer outputs a modulated wave that is directly put on the cassette.
   However, the input is demodulated by the cassette-reader before being
   sent to the computer: we got 0 when the signal is around 4.5 kHz and
   1 when the signal is around 6.3 kHz.
*/

#define TO7_BIT_LENGTH 0.001114

/* 1-bit cassette input to the computer
   inside the controller, two frequency filters (adjusted to 6.3 and 4.5 kHz)
   and a comparator demodulate the raw signal into 0s and 1s.
*/
int thomson_state::to7_get_cassette()
{
	if ( m_cassette->exists() )
	{
		cassette_image* cass = m_cassette->get_image();
		cassette_state state = m_cassette->get_state();
		double pos = m_cassette->get_position();
		int bitpos = pos / TO7_BIT_LENGTH;

		if ( (state & CASSETTE_MASK_MOTOR) == CASSETTE_MOTOR_DISABLED )
			return 1;

		if ( K7_SPEED_HACK && m_to7_k7_bits )
		{
			/* hack, feed existing bits */
			if ( bitpos >= m_to7_k7_bitsize )
				bitpos = m_to7_k7_bitsize -1;
			VLOG (( "$%04x %f to7_get_cassette: state=$%X pos=%f samppos=%i bit=%i\n",
				m_maincpu->pc(), machine().time().as_double(), state, pos, bitpos,
				m_to7_k7_bits[ bitpos ] ));
			return m_to7_k7_bits[ bitpos ];
		}
		else
		{
			/* demodulate wave signal on-the-fly */
			/* we simply count sign changes... */
			int k, chg;
			INT8 data[40];
			cassette_get_samples( cass, 0, pos, TO7_BIT_LENGTH * 15. / 14., 40, 1, data, 0 );

			for ( k = 1, chg = 0; k < 40; k++ )
			{
				if ( data[ k - 1 ] >= 0 && data[ k ] < 0 )
					chg++;
				if ( data[ k - 1 ] <= 0 && data[ k ] > 0 )
					chg++;
			}
			k = ( chg >= 13 ) ? 1 : 0;
			VLOG (( "$%04x %f to7_get_cassette: state=$%X pos=%f samppos=%i bit=%i (%i)\n",
				m_maincpu->pc(), machine().time().as_double(), state, pos, bitpos,
				k, chg ));
			return k;
		}

	}
	else
		return 0;
}



/* 1-bit cassette output */
void thomson_state::to7_set_cassette( int data )
{
	m_cassette->output(data ? 1. : -1. );
}



WRITE_LINE_MEMBER( thomson_state::to7_set_cassette_motor )
{
	cassette_state cassstate =  m_cassette->get_state();
	double pos = m_cassette->get_position();

	LOG (( "$%04x %f to7_set_cassette_motor: cassette motor %s bitpos=%i\n",
			m_maincpu->pc(), machine().time().as_double(), state ? "off" : "on",
			(int) (pos / TO7_BIT_LENGTH) ));

	if ( (cassstate & CASSETTE_MASK_MOTOR) == CASSETTE_MOTOR_DISABLED && !state && pos > 0.3 )
	{
		/* rewind a little before starting the motor */
		m_cassette->seek(-0.3, SEEK_CUR );
	}

	m_cassette->change_state(state ? CASSETTE_MOTOR_DISABLED : CASSETTE_MOTOR_ENABLED,CASSETTE_MASK_MOTOR );
}



/*-------------- MO5 ------------*/


/* Each byte is represented as 8 bits without start or stop bit (unlike TO7).
   Bits are coded in MFM, and the MFM signal is directly fed to the
   computer which has to decode it in software (unlike TO7).
   A 1 bit is one period at 1200 Hz; a 0 bit is one half-period at 600 Hz.
   Bit-order is most significant bit first (unlike TO7).

   Double-density MO6 cassettes follow the exact same mechanism, but with
   at double frequency (perdiods at 2400 Hz, and half-perdios at 1200 Hz).
*/


#define MO5_BIT_LENGTH   0.000833
#define MO5_HBIT_LENGTH (MO5_BIT_LENGTH / 2.)


int thomson_state::mo5_get_cassette()
{
	if ( m_cassette->exists() )
	{
		cassette_image* cass = m_cassette->get_image();
		cassette_state state = m_cassette->get_state();
		double pos = m_cassette->get_position();
		INT32 hbit;

		if ( (state & CASSETTE_MASK_MOTOR) == CASSETTE_MOTOR_DISABLED )
			return 1;

		cassette_get_sample( cass, 0, pos, 0, &hbit );
		hbit = hbit >= 0;

		VLOG (( "$%04x %f mo5_get_cassette: state=$%X pos=%f hbitpos=%i hbit=%i\n",
			m_maincpu->pc(), machine().time().as_double(), state, pos,
			(int) (pos / MO5_HBIT_LENGTH), hbit ));
		return hbit;
	}
	else
		return 0;
}



void thomson_state::mo5_set_cassette( int data )
{
	m_cassette->output(data ? 1. : -1. );
}



WRITE_LINE_MEMBER( thomson_state::mo5_set_cassette_motor )
{
	cassette_state cassstate = m_cassette->get_state();
	double pos = m_cassette->get_position();

	LOG (( "$%04x %f mo5_set_cassette_motor: cassette motor %s hbitpos=%i\n",
			m_maincpu->pc(), machine().time().as_double(), state ? "off" : "on",
			(int) (pos / MO5_HBIT_LENGTH) ));

	if ( (cassstate & CASSETTE_MASK_MOTOR) == CASSETTE_MOTOR_DISABLED &&  !state && pos > 0.3 )
	{
		/* rewind a little before starting the motor */
		m_cassette->seek(-0.3, SEEK_CUR );
	}

	m_cassette->change_state(state ? CASSETTE_MOTOR_DISABLED : CASSETTE_MOTOR_ENABLED,CASSETTE_MASK_MOTOR );
}




/*************************** utilities ********************************/



/* ------------ IRQs ------------ */


void thomson_state::thom_set_irq( int line, int state )
{
	int old = m_thom_irq;

	if ( state )
		m_thom_irq |= 1 << line;
	else
		m_thom_irq &= ~(1 << line);

	if ( !old && m_thom_irq )
		LOG_IRQ(( "%f thom_set_irq: irq line up %i\n", machine().time().as_double(), line ));
	if ( old && !m_thom_irq )
		LOG_IRQ(( "%f thom_set_irq: irq line down %i\n", machine().time().as_double(), line ));

	m_maincpu->set_input_line(M6809_IRQ_LINE, m_thom_irq ? ASSERT_LINE : CLEAR_LINE);
}



void thomson_state::thom_set_firq ( int line, int state )
{
	int old = m_thom_firq;

	if ( state )
		m_thom_firq |= 1 << line;
	else
		m_thom_firq &= ~(1 << line);

	if ( !old && m_thom_firq )
		LOG_IRQ(( "%f thom_set_firq: firq line up %i\n", machine().time().as_double(), line ));
	if ( old && !m_thom_firq )
		LOG_IRQ(( "%f thom_set_firq: firq line down %i\n", machine().time().as_double(), line ));

	m_maincpu->set_input_line(M6809_FIRQ_LINE, m_thom_firq ? ASSERT_LINE : CLEAR_LINE);
}



void thomson_state::thom_irq_reset()
{
	m_thom_irq = 0;
	m_thom_firq = 0;
	m_maincpu->set_input_line(M6809_IRQ_LINE, CLEAR_LINE );
	m_maincpu->set_input_line(M6809_FIRQ_LINE, CLEAR_LINE );
}



void thomson_state::thom_irq_init()
{
	save_item(NAME(m_thom_irq));
	save_item(NAME(m_thom_firq));
}



void thomson_state::thom_irq_0( int state )
{
	thom_set_irq( 0, state );
}

WRITE_LINE_MEMBER( thomson_state::thom_dev_irq_0 )
{
	thom_irq_0( state );
}



WRITE_LINE_MEMBER( thomson_state::thom_irq_1 )
{
	thom_set_irq  ( 1, state );
}

void thomson_state::thom_irq_3( int state )
{
	thom_set_irq  ( 3, state );
}

WRITE_LINE_MEMBER( thomson_state::thom_firq_1 )
{
	thom_set_firq ( 1, state );
}

void thomson_state::thom_firq_2( int state )
{
	thom_set_firq ( 2, state );
}

void thomson_state::thom_irq_4( int state )
{
	thom_set_irq  ( 4, state );
}


/*
   current IRQ usage:

   line 0 => 6846 interrupt
   line 1 => 6821 interrupts (shared for all 6821)
   line 2 => TO8 lightpen interrupt (from gate-array)
   line 3 => TO9 keyboard interrupt (from 6850 ACIA)
   line 4 => MIDI interrupt (from 6850 ACIA)
*/



/* ------------ LED ------------ */



void thomson_state::thom_set_caps_led( int led )
{
	output_set_value( "led0", led );
}

/* ------------ 6850 defines ------------ */

#define ACIA_6850_RDRF  0x01    /* Receive data register full */
#define ACIA_6850_TDRE  0x02    /* Transmit data register empty */
#define ACIA_6850_dcd   0x04    /* Data carrier detect, active low */
#define ACIA_6850_cts   0x08    /* Clear to send, active low */
#define ACIA_6850_FE    0x10    /* Framing error */
#define ACIA_6850_OVRN  0x20    /* Receiver overrun */
#define ACIA_6850_PE    0x40    /* Parity error */
#define ACIA_6850_irq   0x80    /* Interrupt request, active low */



/***************************** TO7 / T9000 *************************/

DEVICE_IMAGE_LOAD_MEMBER( thomson_state, to7_cartridge )
{
	int i,j;
	UINT8* pos = memregion("maincpu" )->base() + 0x10000;
	offs_t size;
	char name[129];

	if (image.software_entry() == NULL)
		size = image.length();
	else
		size = image.get_software_region_length("rom");

	/* get size & number of 16-KB banks */
	if ( size <= 0x04000 )
		m_thom_cart_nb_banks = 1;
	else if ( size == 0x08000 )
		m_thom_cart_nb_banks = 2;
	else if ( size == 0x10000 )
		m_thom_cart_nb_banks = 4;
	else
	{
		astring errmsg;
		errmsg.printf("Invalid cartridge size %u", size);
		image.seterror(IMAGE_ERROR_UNSUPPORTED, errmsg.cstr());
		return IMAGE_INIT_FAIL;
	}

	if (image.software_entry() == NULL)
	{
		if ( image.fread( pos, size ) != size )
		{
			image.seterror(IMAGE_ERROR_INVALIDIMAGE, "Read error");
			return IMAGE_INIT_FAIL;
		}
	}
	else
	{
		memcpy(pos, image.get_software_region("rom"), size);
	}

	/* extract name */
	for ( i = 0; i < size && pos[i] != ' '; i++ );
	for ( i++, j = 0; i + j < size && j < 128 && pos[i+j] >= 0x20; j++)
		name[j] = pos[i+j];
	name[j] = 0;

	/* sanitize name */
	for ( i = 0; name[i]; i++)
	{
		if ( name[i] < ' ' || name[i] >= 127 )
			name[i] = '?';
	}

	PRINT (( "to7_cartridge_load: cartridge \"%s\" banks=%i, size=%i\n", name, m_thom_cart_nb_banks, size ));

	return IMAGE_INIT_PASS;
}



void thomson_state::to7_update_cart_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	int bank = 0;
	if ( m_thom_cart_nb_banks )
	{
		bank = m_thom_cart_bank % m_thom_cart_nb_banks;
		if ( bank != m_old_cart_bank && m_old_cart_bank < 0 )
		{
			space.install_read_handler(0x0000, 0x0003, read8_delegate(FUNC(thomson_state::to7_cartridge_r),this) );
		}
	}
	if ( bank != m_old_cart_bank )
	{
		membank( THOM_CART_BANK )->set_entry( bank );
		m_old_cart_bank = bank;
		LOG_BANK(( "to7_update_cart_bank: CART is cartridge bank %i\n", bank ));
	}
}



void thomson_state::to7_update_cart_bank_postload()
{
	to7_update_cart_bank();
}



/* write signal to 0000-1fff generates a bank switch */
WRITE8_MEMBER( thomson_state::to7_cartridge_w )
{
	if ( offset >= 0x2000 )
		return;

	m_thom_cart_bank = offset & 3;
	to7_update_cart_bank();
}



/* read signal to 0000-0003 generates a bank switch */
READ8_MEMBER( thomson_state::to7_cartridge_r )
{
	UINT8* pos = memregion( "maincpu" )->base() + 0x10000;
	UINT8 data = pos[offset + (m_thom_cart_bank % m_thom_cart_nb_banks) * 0x4000];
	if ( !space.debugger_access() )
	{
		m_thom_cart_bank = offset & 3;
		to7_update_cart_bank();
	}
	return data;
}



/* ------------ 6846 (timer, I/O) ------------ */



WRITE8_MEMBER( thomson_state::to7_timer_port_out )
{
	thom_set_mode_point( data & 1 );          /* bit 0: video bank switch */
	thom_set_caps_led( (data & 8) ? 1 : 0 ) ; /* bit 3: keyboard led */
	thom_set_border_color( ((data & 0x10) ? 1 : 0) |           /* bits 4-6: border color */
							((data & 0x20) ? 2 : 0) |
							((data & 0x40) ? 4 : 0) );
}



WRITE8_MEMBER( thomson_state::to7_timer_cp2_out )
{
	m_buzzer->write_unsigned8(data ? 0x80 : 0); /* 1-bit buzzer */
}



READ8_MEMBER( thomson_state::to7_timer_port_in )
{
	int lightpen = (ioport("lightpen_button")->read() & 1) ? 2 : 0;
	int cass = to7_get_cassette() ? 0x80 : 0;
	return lightpen | cass;
}



WRITE8_MEMBER( thomson_state::to7_timer_tco_out )
{
	/* 1-bit cassette output */
	to7_set_cassette( data );
}



const mc6846_interface to7_timer =
{
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_port_out),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_cp2_out),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_port_in),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_tco_out),
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, thom_dev_irq_0)
};



/* ------------ lightpen automaton ------------ */


void thomson_state::to7_lightpen_cb( int step )
{
	if ( ! m_to7_lightpen )
		return;

	LOG_VIDEO(( "%f to7_lightpen_cb: step=%i\n", machine().time().as_double(), step ));
	m_pia_sys->cb1_w( 1 );
	m_pia_sys->cb1_w( 0 );
	m_to7_lightpen_step = step;
}



/* ------------ video ------------ */



void thomson_state::to7_set_init( int init )
{
	/* INIT signal wired to system PIA 6821 */

	LOG_VIDEO(( "%f to7_set_init: init=%i\n", machine().time().as_double(), init ));
	m_pia_sys->ca1_w( init );
}



/* ------------ system PIA 6821 ------------ */



WRITE_LINE_MEMBER( thomson_state::to7_sys_cb2_out )
{
	m_to7_lightpen = !state;
}



WRITE8_MEMBER( thomson_state::to7_sys_portb_out )
{
	/* value fetched in to7_sys_porta_in */
}



#define TO7_LIGHTPEN_DECAL 17 /* horizontal lightpen shift, stored in $60D2 */



READ8_MEMBER( thomson_state::to7_sys_porta_in )
{
	if ( m_to7_lightpen )
	{
		/* lightpen hi */
		return to7_lightpen_gpl( TO7_LIGHTPEN_DECAL, m_to7_lightpen_step ) >> 8;
	}
	else
	{
		/* keyboard  */
		int keyline = m_pia_sys->b_output();
		UINT8 val = 0xff;
		int i;
		static const char *const keynames[] = {
			"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3",
			"keyboard_4", "keyboard_5", "keyboard_6", "keyboard_7"
		};

		for ( i = 0; i < 8; i++ )
		{
			if ( ! (keyline & (1 << i)) )
				val &= ioport(keynames[i])->read();
		}
		return val;
	}
}



READ8_MEMBER( thomson_state::to7_sys_portb_in )
{
	/* lightpen low */
	return to7_lightpen_gpl( TO7_LIGHTPEN_DECAL, m_to7_lightpen_step ) & 0xff;
}



/* ------------ CC 90-232 I/O extension ------------ */

/* Features:
   - 6821 PIA
   - serial RS232: bit-banging?
   - parallel CENTRONICS: a printer (-prin) is emulated
   - usable on TO7(/70), MO5(E) only; not on TO9 and higher

   Note: it seems impossible to connect both a serial & a parallel device
   because the Data Transmit Ready bit is shared in an incompatible way!
*/



/* test whether a parallel or a serial device is connected: both cannot
   be exploited at the same time!
*/
to7_io_dev thomson_state::to7_io_mode()
{
	if (m_centronics->pe_r() == TRUE)
		return TO7_IO_CENTRONICS;
	else if ( m_serial->exists())
		return TO7_IO_RS232;
	return TO7_IO_NONE;
}



WRITE_LINE_MEMBER( thomson_state::to7_io_ack )
{
	m_pia_io->cb1_w( state);
	//LOG_IO (( "%f to7_io_ack: CENTRONICS new state $%02X (ack=%i)\n", machine().time().as_double(), data, ack ));
}

const device_type TO7_IO_LINE = &device_creator<to7_io_line_device>;

//-------------------------------------------------
//  to7_io_line_device - constructor
//-------------------------------------------------

to7_io_line_device::to7_io_line_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: device_t(mconfig, TO7_IO_LINE, "T07 Serial source", tag, owner, clock, "to7_io_line", __FILE__),
		device_serial_interface(mconfig, *this)
{
}

void to7_io_line_device::device_start()
{
}

WRITE8_MEMBER( to7_io_line_device::porta_out )
{
	int tx  = data & 1;
	int dtr = ( data & 2 ) ? 1 : 0;

	LOG_IO(( "%s %f to7_io_porta_out: tx=%i, dtr=%i\n",  machine().describe_context(), machine().time().as_double(), tx, dtr ));
	if ( dtr )
		m_connection_state |=  device_serial_interface::DTR;
	else
		m_connection_state &= ~device_serial_interface::DTR;

	set_out_data_bit(tx);
	serial_connection_out();
}



READ8_MEMBER( to7_io_line_device::porta_in )
{
	centronics_device *printer = machine().device<centronics_device>("centronics");
	int cts = 1;
	int dsr = ( m_input_state & device_serial_interface::DSR ) ? 0 : 1;
	int rd  = get_in_data_bit();

	if ( machine().driver_data<thomson_state>()->to7_io_mode() == TO7_IO_RS232 )
		cts = m_input_state & device_serial_interface::CTS ? 0 : 1;
	else
		cts = !printer->busy_r();

	LOG_IO(( "%s %f to7_io_porta_in: mode=%i cts=%i, dsr=%i, rd=%i\n", machine().describe_context(), machine().time().as_double(), machine().driver_data<thomson_state>()->to7_io_mode(), cts, dsr, rd ));

	return (dsr ? 0x20 : 0) | (cts ? 0x40 : 0) | (rd ? 0x80: 0);
}



WRITE8_MEMBER( thomson_state::to7_io_portb_out )
{
	LOG_IO(( "$%04x %f to7_io_portb_out: CENTRONICS set data=$%02X\n", m_maincpu->pc(), machine().time().as_double(), data ));

	/* set 8-bit data */
	m_centronics->write( space, 0, data);
}



WRITE_LINE_MEMBER( thomson_state::to7_io_cb2_out )
{
	LOG_IO(( "$%04x %f to7_io_cb2_out: CENTRONICS set strobe=%i\n", m_maincpu->pc(), machine().time().as_double(), state ));

	/* send STROBE to printer */
	m_centronics->strobe_w(state);
}


void to7_io_line_device::input_callback(UINT8 state)
{
	m_input_state = state;

	LOG_IO(( "%f to7_io_in_callback:  cts=%i dsr=%i rd=%i\n", machine().time().as_double(), (state & device_serial_interface::CTS) ? 1 : 0, (state & device_serial_interface::DSR) ? 1 : 0, (int)get_in_data_bit() ));
}



const centronics_interface to7_centronics_config =
{
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, to7_io_ack),
	DEVCB_NULL,
	DEVCB_NULL
};


void to7_io_line_device::device_reset()
{
	pia6821_device *io_pia = machine().device<pia6821_device>(THOM_PIA_IO);

	LOG (( "to7_io_reset called\n" ));

	if (io_pia) io_pia->set_port_a_z_mask(0x03 );
	m_input_state = device_serial_interface::CTS;
	m_connection_state &= ~device_serial_interface::DTR;
	m_connection_state |=  device_serial_interface::RTS;  /* always ready to send */
	set_out_data_bit(1);
	serial_connection_out();
}

/* ------------ RF 57-932 RS232 extension ------------ */

/* Features:
   - SY 6551 ACIA.
   - higher transfer rates than the CC 90-232
   - usable on all computer, including TO9 and higher
 */



/* ------------  MD 90-120 MODEM extension (not functional) ------------ */

/* Features:
   - 6850 ACIA
   - 6821 PIA
   - asymetric 1200/ 75 bauds (reversable)

   TODO!
 */


WRITE_LINE_MEMBER( thomson_state::to7_modem_cb )
{
	LOG(( "to7_modem_cb: called %i\n", state ));
}



WRITE_LINE_MEMBER( thomson_state::to7_modem_tx_w )
{
	m_to7_modem_tx = state;
}

ACIA6850_INTERFACE( to7_modem )
{
	1200,
	1200, /* 1200 bauds, might be divided by 16 */
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, to7_modem_tx_w), /*&to7_modem_tx,*/
	DEVCB_NULL,
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, to7_modem_cb)
};



void thomson_state::to7_modem_reset()
{
	LOG (( "to7_modem_reset called\n" ));
	m_acia->write_rx(0);
	m_to7_modem_tx = 0;
	/* pia_reset() is called in machine_reset */
	/* acia_6850 has no reset (?) */
}



void thomson_state::to7_modem_init()
{
	LOG (( "to7_modem_init: MODEM not implemented!\n" ));
	save_item(NAME(m_to7_modem_tx));
}



/* ------------  dispatch MODEM / speech extension ------------ */


const mea8000_interface to7_speech = { "speech", DEVCB_NULL };


READ8_MEMBER( thomson_state::to7_modem_mea8000_r )
{
	if ( space.debugger_access() )
		{
		return 0;
		}

	if ( ioport("mconfig")->read() & 1 )
	{
		return m_mea8000->read(space, offset);
	}
	else
	{
		switch (offset) {
		case 0: return m_acia->status_read(space, offset );
		case 1: return m_acia->data_read(space, offset );
		default: return 0;
		}
	}
}



WRITE8_MEMBER( thomson_state::to7_modem_mea8000_w )
{
	if ( ioport("mconfig")->read() & 1 )
	{
		m_mea8000->write(space, offset, data);
	}
	else
	{
		switch (offset) {
		case 0: m_acia->control_write( space, offset, data );
		case 1: m_acia->data_write( space, offset, data );
		}
	}
}



/* ------------ SX 90-018 (model 2) music & game extension ------------ */

/* features:
   - 6821 PIA
   - two 8-position, 2-button game pads
   - 2-button mouse (exclusive with pads)
     do not confuse with the TO9-specific mouse
   - 6-bit DAC sound

   extends the CM 90-112 (model 1) with one extra button per pad and a mouse
*/



#define TO7_GAME_POLL_PERIOD  attotime::from_usec( 500 )


/* The mouse is a very simple phase quadrature one.
   Each axis provides two 1-bit signals, A and B, that are toggled by the
   axis rotation. The two signals are not in phase, so that whether A is
   toggled before B or B before A gives the direction of rotation.
   This is similar Atari & Amiga mouses.
   Returns: 0 0 0 0 0 0 YB XB YA XA
 */
UINT8 thomson_state::to7_get_mouse_signal()
{
	UINT8 xa, xb, ya, yb;
	UINT16 dx = ioport("mouse_x")->read(); /* x axis */
	UINT16 dy = ioport("mouse_y")->read(); /* y axis */
	xa = ((dx + 1) & 3) <= 1;
	xb = (dx & 3) <= 1;
	ya = ((dy + 1) & 3) <= 1;
	yb = (dy & 3) <= 1;
	return xa | (ya << 1) | (xb << 2) | (yb << 3);
}



void thomson_state::to7_game_sound_update()
{
	m_dac->write_unsigned8(m_to7_game_mute ? 0 : (m_to7_game_sound << 2) );
}



READ8_MEMBER( thomson_state::to7_game_porta_in )
{
	UINT8 data;
	if ( ioport("config")->read() & 1 )
	{
		/* mouse */
		data = to7_get_mouse_signal() & 0x0c;             /* XB, YB */
		data |= ioport("mouse_button")->read() & 3; /* buttons */
	}
	else
	{
		/* joystick */
		data = ioport("game_port_directions")->read();
		/* bit 0=0 => P1 up      bit 4=0 => P2 up
		   bit 1=0 => P1 down    bit 5=0 => P2 down
		   bit 2=0 => P1 left    bit 6=0 => P2 left
		   bit 3=0 => P1 right   bit 7=0 => P2 right
		*/
		/* remove impossible combinations: up+down, left+right */
		if ( ! ( data & 0x03 ) )
			data |= 0x03;
		if ( ! ( data & 0x0c ) )
			data |= 0x0c;
		if ( ! ( data & 0x30 ) )
			data |= 0x30;
		if ( ! ( data & 0xc0 ) )
			data |= 0xc0;
		if ( ! ( data & 0x03 ) )
			data |= 0x03;
		if ( ! ( data & 0x0c ) )
			data |= 0x0c;
		if ( ! ( data & 0x30 ) )
			data |= 0x30;
		if ( ! ( data & 0xc0 ) )
			data |= 0xc0;
	}
	return data;
}



READ8_MEMBER( thomson_state::to7_game_portb_in )
{
	UINT8 data;
	if ( ioport("config")->read() & 1 )
	{
		/* mouse */
		UINT8 mouse =  to7_get_mouse_signal();
		data = 0;
		if ( mouse & 1 )
			data |= 0x04; /* XA */
		if ( mouse & 2 )
			data |= 0x40; /* YA */
	}
	else
	{
		/* joystick */
		/* bits 6-7: action buttons A (0=pressed) */
		/* bits 2-3: action buttons B (0=pressed) */
		/* bits 4-5: unused (ouput) */
		/* bits 0-1: unknown! */
		data = ioport("game_port_buttons")->read();
	}
	return data;
}



WRITE8_MEMBER( thomson_state::to7_game_portb_out )
{
	/* 6-bit DAC sound */
	m_to7_game_sound = data & 0x3f;
	to7_game_sound_update();
}



WRITE_LINE_MEMBER( thomson_state::to7_game_cb2_out )
{
	/* undocumented */
	/* some TO8 games (e.g.: F15) seem to write here a lot */
}



/* this should be called periodically */
TIMER_CALLBACK_MEMBER(thomson_state::to7_game_update_cb)
{
	if ( ioport("config")->read() & 1 )
	{
		/* mouse */
		UINT8 mouse = to7_get_mouse_signal();
		m_pia_game->ca1_w( (mouse & 1) ? 1 : 0 ); /* XA */
		m_pia_game->ca2_w( (mouse & 2) ? 1 : 0 ); /* YA */
	}
	else
	{
		/* joystick */
		UINT8 in = ioport("game_port_buttons")->read();
		m_pia_game->cb2_w( (in & 0x80) ? 1 : 0 ); /* P2 action A */
		m_pia_game->ca2_w( (in & 0x40) ? 1 : 0 ); /* P1 action A */
		m_pia_game->cb1_w( (in & 0x08) ? 1 : 0 ); /* P2 action B */
		m_pia_game->ca1_w( (in & 0x04) ? 1 : 0 ); /* P1 action B */
		/* TODO:
		   it seems that CM 90-112 behaves differently
		   - ca1 is P1 action A, i.e., in & 0x40
		   - ca2 is P2 action A, i.e., in & 0x80
		   - cb1, cb2 are not connected (should not be a problem)
		*/
		/* Note: the MO6 & MO5NR have slightly different connections
		   (see mo6_game_update_cb)
		*/
	}
}



void thomson_state::to7_game_init()
{
	LOG (( "to7_game_init called\n" ));
	m_to7_game_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::to7_game_update_cb),this));
	m_to7_game_timer->adjust(TO7_GAME_POLL_PERIOD, 0, TO7_GAME_POLL_PERIOD);
	save_item(NAME(m_to7_game_sound));
	save_item(NAME(m_to7_game_mute));
}



void thomson_state::to7_game_reset()
{
	LOG (( "to7_game_reset called\n" ));
	m_pia_game->ca1_w( 0 );
	m_to7_game_sound = 0;
	m_to7_game_mute = 0;
	to7_game_sound_update();
}



/* ------------ MIDI extension ------------ */

/* IMPORTANT NOTE:
   The following is experimental and not compiled in by default.
   It relies on the existence of an hypothetical "character device" API able
   to transmit bytes between the MESS driver and the outside world
   (using, e.g., character device special files on some UNIX).
*/

#ifdef CHARDEV

#include "devices/chardev.h"

/* Features an EF 6850 ACIA

   MIDI protocol is a serial asynchronous protocol
   Each 8-bit byte is transmitted as:
   - 1 start bit
   - 8 data bits
   - 1 stop bits
   320 us per transmitted byte => 31250 baud

   Emulation is based on the Motorola 6850 documentation, not EF 6850.

   We do not emulate the seral line but pass bytes directly between the
   6850 registers and the MIDI device.
*/


static UINT8 to7_midi_status;   /* 6850 status word */
static UINT8 to7_midi_overrun;  /* pending overrun */
static UINT8 to7_midi_intr;     /* enabled interrupts */

static chardev* to7_midi_chardev;



void thomson_state::to7_midi_update_irq (  )
{
	if ( (to7_midi_intr & 4) && (to7_midi_status & ACIA_6850_RDRF) )
		to7_midi_status |= ACIA_6850_irq; /* byte received interrupt */

	if ( (to7_midi_intr & 4) && (to7_midi_status & ACIA_6850_OVRN) )
		to7_midi_status |= ACIA_6850_irq; /* overrun interrupt */

	if ( (to7_midi_intr & 3) == 1 && (to7_midi_status & ACIA_6850_TDRE) )
		to7_midi_status |= ACIA_6850_irq; /* ready to transmit interrupt */

	thom_irq_4( machine, to7_midi_status & ACIA_6850_irq );
}



void thomson_state::to7_midi_byte_received_cb( chardev_err s )
{
	to7_midi_status |= ACIA_6850_RDRF;
	if ( s == CHARDEV_OVERFLOW )
		to7_midi_overrun = 1;
	to7_midi_update_irq( machine );
}



void thomson_state::to7_midi_ready_to_send_cb(  )
{
	to7_midi_status |= ACIA_6850_TDRE;
	to7_midi_update_irq( machine );
}



READ8_MEMBER( thomson_state::to7_midi_r )
{
	/* ACIA 6850 registers */

	switch ( offset )
	{
	case 0: /* get status */
		/* bit 0:     data received */
		/* bit 1:     ready to transmit data */
		/* bit 2:     data carrier detect (ignored) */
		/* bit 3:     clear to send (ignored) */
		/* bit 4:     framing error (ignored) */
		/* bit 5:     overrun */
		/* bit 6:     parity error (ignored) */
		/* bit 7:     interrupt */
		LOG_MIDI(( "%s %f to7_midi_r: status $%02X (rdrf=%i, tdre=%i, ovrn=%i, irq=%i)\n",
				space.machine().describe_context(), space.machine().time().as_double(), to7_midi_status,
				(to7_midi_status & ACIA_6850_RDRF) ? 1 : 0,
				(to7_midi_status & ACIA_6850_TDRE) ? 1 : 0,
				(to7_midi_status & ACIA_6850_OVRN) ? 1 : 0,
				(to7_midi_status & ACIA_6850_irq) ? 1 : 0 ));
		return to7_midi_status;


	case 1: /* get input data */
	{
				UINT8 data = chardev_in( to7_midi_chardev );
				if ( !space.debugger_access() )
				{
						to7_midi_status &= ~(ACIA_6850_irq | ACIA_6850_RDRF);
						if ( to7_midi_overrun )
								to7_midi_status |= ACIA_6850_OVRN;
						else
								to7_midi_status &= ~ACIA_6850_OVRN;
						to7_midi_overrun = 0;
						LOG_MIDI(( "%s %f to7_midi_r: read data $%02X\n",
									space.machine().describe_context(), space.machine().time().as_double(), data ));
						to7_midi_update_irq();
				}
				return data;
	}


	default:
		logerror( "%s to7_midi_r: invalid offset %i\n",
				space.machine().describe_context(),  offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::to7_midi_w )
{
	/* ACIA 6850 registers */

	switch ( offset )
	{
	case 0: /* set control */
		/* bits 0-1: clock divide (ignored) or reset */
		if ( (data & 3) == 3 )
		{
			/* reset */
			LOG_MIDI(( "%s %f to7_midi_w: reset (data=$%02X)\n", space.machine().describe_context(), space.machine().time().as_double(), data ));
			to7_midi_overrun = 0;
			to7_midi_status = 2;
			to7_midi_intr = 0;
			chardev_reset( to7_midi_chardev );
		}
		else
		{
			/* bits 2-4: parity  */
			/* bits 5-6: interrupt on transmit */
			/* bit 7:    interrupt on receive */
			to7_midi_intr = data >> 5;
			{
				static const int bits[8] = { 7,7,7,7,8,8,8,8 };
				static const int stop[8] = { 2,2,1,1,2,1,1,1 };
				static const char parity[8] = { 'e','o','e','o','-','-','e','o' };
				LOG_MIDI(( "%s %f to7_midi_w: set control to $%02X (bits=%i, stop=%i, parity=%c, intr in=%i out=%i)\n",
						space.machine().describe_context(), space.machine().time().as_double(),
						data,
						bits[ (data >> 2) & 7 ],
						stop[ (data >> 2) & 7 ],
						parity[ (data >> 2) & 7 ],
						to7_midi_intr >> 2,
						(to7_midi_intr & 3) ? 1 : 0));
			}
		}
		to7_midi_update_irq( );
		break;


	case 1: /* output data */
		LOG_MIDI(( "%s %f to7_midi_w: write data $%02X\n", space.machine().describe_context(), space.machine().time().as_double(), data ));
		if ( data == 0x55 )
			/* cable-detect: shortcut */
			chardev_fake_in( to7_midi_chardev, 0x55 );
		else
		{
			/* send to MIDI */
			to7_midi_status &= ~(ACIA_6850_irq | ACIA_6850_TDRE);
			chardev_out( to7_midi_chardev, data );
		}
		break;


	default:
		logerror( "%s to7_midi_w: invalid offset %i (data=$%02X) \n", space.machine().describe_context(), offset, data );
	}
}



static const chardev_interface to7_midi_interface =
{
	to7_midi_byte_received_cb,
	to7_midi_ready_to_send_cb,
};



void thomson_state::to7_midi_reset(  )
{
	LOG (( "to7_midi_reset called\n" ));
	to7_midi_overrun = 0;
	to7_midi_status = 0;
	to7_midi_intr = 0;
	chardev_reset( to7_midi_chardev );
}



void thomson_state::to7_midi_init(  )
{
	LOG (( "to7_midi_init\n" ));
	to7_midi_chardev = chardev_open( &machine, "/dev/snd/midiC0D0", "/dev/snd/midiC0D1", &to7_midi_interface );
	save_item(NAME(to7_midi_status );
	save_item(NAME(to7_midi_overrun );
	save_item(NAME(to7_midi_intr );
}



#else



READ8_MEMBER( thomson_state::to7_midi_r )
{
	logerror( "to7_midi_r: not implemented\n" );
	return 0;
}



WRITE8_MEMBER( thomson_state::to7_midi_w )
{
	logerror( "to7_midi_w: not implemented\n" );
}



void thomson_state::to7_midi_reset()
{
	logerror( "to7_midi_reset: not implemented\n" );
}



void thomson_state::to7_midi_init()
{
	logerror( "to7_midi_init: not implemented\n" );
}



#endif



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, to7 )
{
	LOG (( "to7: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	to7_game_reset();
	to7_floppy_reset();
	to7_modem_reset();
	to7_midi_reset();

	/* video */
	thom_set_video_mode( THOM_VMODE_TO770 );
	m_thom_init_cb = &thomson_state::to7_set_init;
	m_thom_lightpen_cb = &thomson_state::to7_lightpen_cb;
	thom_set_lightpen_callback( 3 );
	thom_set_mode_point( 0 );
	thom_set_border_color( 0 );
	m_pia_sys->cb1_w( 0 );

	/* memory */
	m_old_cart_bank = -1;
	to7_update_cart_bank();
	/* thom_cart_bank not reset */

	/* lightpen */
	m_to7_lightpen = 0;
}



MACHINE_START_MEMBER( thomson_state, to7 )
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "to7: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	to7_game_init();
	to7_floppy_init(mem + 0x20000);
	to7_modem_init();
	to7_midi_init();

	/* memory */
	m_thom_cart_bank = 0;
	m_thom_vram = ram;
	membank( THOM_BASE_BANK )->configure_entry( 0, ram + 0x4000);
	membank( THOM_VRAM_BANK )->configure_entries( 0, 2, m_thom_vram, 0x2000 );
	membank( THOM_CART_BANK )->configure_entries( 0, 4, mem + 0x10000, 0x4000 );
	membank( THOM_BASE_BANK )->set_entry( 0 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );
	membank( THOM_CART_BANK )->set_entry( 0 );

	if ( m_ram->size() > 24*1024 )
	{
		/* install 16 KB or 16 KB + 8 KB memory extensions */
		/* BASIC instruction to see free memory: ?FRE(0) */
		int extram = m_ram->size() - 24*1024;
		space.install_write_bank(0x8000, 0x8000 + extram - 1, THOM_RAM_BANK);
		space.install_read_bank(0x8000, 0x8000 + extram - 1, THOM_RAM_BANK );
		membank( THOM_RAM_BANK )->configure_entry( 0, ram + 0x6000);
		membank( THOM_RAM_BANK )->set_entry( 0 );
	}

	/* force 2 topmost color bits to 1 */
	memset( m_thom_vram + 0x2000, 0xc0, 0x2000 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_pointer(NAME((mem + 0x10000)), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to7_update_cart_bank_postload),this));
}



/***************************** TO7/70 *************************/



/* ------------ system PIA 6821 ------------ */



WRITE_LINE_MEMBER( thomson_state::to770_sys_cb2_out )
{
	/* video overlay: black pixels are transparent and show TV image underneath */
	LOG(( "$%04x to770_sys_cb2_out: video overlay %i\n", m_maincpu->pc(), state ));
}



READ8_MEMBER( thomson_state::to770_sys_porta_in )
{
	/* keyboard */
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3",
		"keyboard_4", "keyboard_5", "keyboard_6", "keyboard_7"
	};
	int keyline = m_pia_sys->b_output() & 7;

	return ioport(keynames[7 - keyline])->read();
}



void thomson_state::to770_update_ram_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	UINT8 portb = m_pia_sys->port_b_z_mask();
	int bank;

	switch (portb & 0xf8)
	{
		/* 2 * 16 KB internal RAM */
	case 0xf0: bank = 0; break;
	case 0xe8: bank = 1; break;

		/* 4 * 16 KB extended RAM */
	case 0x18: bank = 2; break;
	case 0x98: bank = 3; break;
	case 0x58: bank = 4; break;
	case 0xd8: bank = 5; break;

		/* none selected */
	case 0xf8: return;

	default:
		logerror( "to770_update_ram_bank unknown bank $%02X\n", portb & 0xf8 );
		return;
	}

	if ( bank != m_old_ram_bank )
	{
		if ( m_ram->size() == 128*1024 || bank < 2 )
		{
			membank( THOM_RAM_BANK )->set_entry( bank );
		}
		else
		{
			/* RAM size is 48 KB only and unavailable bank
			 * requested */
			space.nop_readwrite(0xa000, 0xdfff);
		}
		m_old_ram_bank = bank;
		LOG_BANK(( "to770_update_ram_bank: RAM bank change %i\n", bank ));
	}
}



void thomson_state::to770_update_ram_bank_postload()
{
	to770_update_ram_bank();
}



WRITE8_MEMBER( thomson_state::to770_sys_portb_out )
{
	to770_update_ram_bank();
}



/* ------------ 6846 (timer, I/O) ------------ */



WRITE8_MEMBER( thomson_state::to770_timer_port_out )
{
	thom_set_mode_point( data & 1 );          /* bit 0: video bank switch */
	thom_set_caps_led( (data & 8) ? 1 : 0 ) ; /* bit 3: keyboard led */
	thom_set_border_color( ((data & 0x10) ? 1 : 0) |          /* 4-bit border color */
							((data & 0x20) ? 2 : 0) |
							((data & 0x40) ? 4 : 0) |
							((data & 0x04) ? 0 : 8) );
}



const mc6846_interface to770_timer =
{
	DEVCB_DRIVER_MEMBER(thomson_state, to770_timer_port_out),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_cp2_out),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_port_in),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_tco_out),
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, thom_dev_irq_0)
};



/* ------------ gate-array ------------ */



READ8_MEMBER( thomson_state::to770_gatearray_r )
{
	struct thom_vsignal v = thom_get_vsignal();
	struct thom_vsignal l = thom_get_lightpen_vsignal( TO7_LIGHTPEN_DECAL, m_to7_lightpen_step - 1, 0 );
	int count, inil, init, lt3;
	count = m_to7_lightpen ? l.count : v.count;
	inil  = m_to7_lightpen ? l.inil  : v.inil;
	init  = m_to7_lightpen ? l.init  : v.init;
	lt3   = m_to7_lightpen ? l.lt3   : v.lt3;

	switch ( offset )
	{
	case 0: return (count >> 8) & 0xff;
	case 1: return count & 0xff;
	case 2: return (lt3 << 7) | (inil << 6);
	case 3: return (init << 7);
	default:
		logerror( "$%04x to770_gatearray_r: invalid offset %i\n", m_maincpu->pc(), offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::to770_gatearray_w )
{
	if ( ! offset )
		m_to7_lightpen = data & 1;
}



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, to770 )
{
	LOG (( "to770: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	to7_game_reset();
	to7_floppy_reset();
	to7_modem_reset();
	to7_midi_reset();

	/* video */
	thom_set_video_mode( THOM_VMODE_TO770 );
	m_thom_init_cb = &thomson_state::to7_set_init;
	m_thom_lightpen_cb = &thomson_state::to7_lightpen_cb;
	thom_set_lightpen_callback( 3 );
	thom_set_mode_point( 0 );
	thom_set_border_color( 8 );
	m_pia_sys->cb1_w( 0 );

	/* memory */
	m_old_ram_bank = -1;
	m_old_cart_bank = -1;
	to7_update_cart_bank();
	to770_update_ram_bank();
	/* thom_cart_bank not reset */

	/* lightpen */
	m_to7_lightpen = 0;
}



MACHINE_START_MEMBER( thomson_state, to770 )
{
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "to770: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	to7_game_init();
	to7_floppy_init( mem + 0x20000 );
	to7_modem_init();
	to7_midi_init();

	/* memory */
	m_thom_cart_bank = 0;
	m_thom_vram = ram;
	membank( THOM_BASE_BANK )->configure_entry( 0, ram + 0x4000);
	membank( THOM_RAM_BANK )->configure_entries( 0, 6, ram + 0x8000, 0x4000 );
	membank( THOM_VRAM_BANK )->configure_entries( 0, 2, m_thom_vram, 0x2000 );
	membank( THOM_CART_BANK )->configure_entries( 0, 4, mem + 0x10000, 0x4000 );
	membank( THOM_BASE_BANK )->set_entry( 0 );
	membank( THOM_RAM_BANK )->set_entry( 0 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );
	membank( THOM_CART_BANK )->set_entry( 0 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_pointer(NAME(mem + 0x10000), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to770_update_ram_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to7_update_cart_bank_postload), this));
}



/***************************** MO5 *************************/



/* ------------ lightpen automaton ------------ */



void thomson_state::mo5_lightpen_cb( int step )
{
	/* MO5 signals ca1 (TO7 signals cb1) */
	if ( ! m_to7_lightpen )
		return;

	m_pia_sys->ca1_w( 1 );
	m_pia_sys->ca1_w( 0 );
	m_to7_lightpen_step = step;
}


/* ------------ periodic interrupt ------------ */

/* the MO5 & MO6 do not have a MC 6846 timer,
   they have a fixed 50 Hz timer instead
*/


TIMER_CALLBACK_MEMBER(thomson_state::mo5_periodic_cb)
{
	/* pulse */
	m_pia_sys->cb1_w( 1 );
	m_pia_sys->cb1_w( 0 );
}



void thomson_state::mo5_init_timer()
{
	/* time is a little faster than 50 Hz to match video framerate */
	m_mo5_periodic_timer->adjust(attotime::zero, 0, attotime::from_usec( 19968 ));
}



/* ------------ system PIA 6821 ------------ */



WRITE8_MEMBER( thomson_state::mo5_sys_porta_out )
{
	thom_set_mode_point( data & 1 );       /* bit 0: video bank switch */
	thom_set_border_color( (data >> 1) & 15 ); /* bit 1-4: border color */
	mo5_set_cassette( (data & 0x40) ? 1 : 0 ); /* bit 6: cassette output */
}



READ8_MEMBER( thomson_state::mo5_sys_porta_in )
{
	return
		(mo5_get_cassette() ? 0x80 : 0) |     /* bit 7: cassette input */
		((ioport("lightpen_button")->read() & 1) ? 0x20 : 0)
		/* bit 5: lightpen button */;
}



WRITE8_MEMBER( thomson_state::mo5_sys_portb_out )
{
	m_buzzer->write_unsigned8((data & 1) ? 0x80 : 0); /* 1-bit buzzer */
}



READ8_MEMBER( thomson_state::mo5_sys_portb_in )
{
	UINT8 portb = m_pia_sys->b_output();
	int col = (portb >> 1) & 7;       /* key column */
	int lin = 7 - ((portb >> 4) & 7); /* key line */
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3",
		"keyboard_4", "keyboard_5", "keyboard_6", "keyboard_7"
	};

	return ( ioport(keynames[lin])->read() & (1 << col) ) ? 0x80 : 0;
}



/* ------------ gate-array ------------ */



#define MO5_LIGHTPEN_DECAL 12



READ8_MEMBER( thomson_state::mo5_gatearray_r )
{
	struct thom_vsignal v = thom_get_vsignal();
	struct thom_vsignal l = thom_get_lightpen_vsignal( MO5_LIGHTPEN_DECAL, m_to7_lightpen_step - 1, 0 );
	int count, inil, init, lt3;
	count = m_to7_lightpen ? l.count : v.count;
	inil  = m_to7_lightpen ? l.inil  : v.inil;
	init  = m_to7_lightpen ? l.init  : v.init;
	lt3   = m_to7_lightpen ? l.lt3   : v.lt3;

	switch ( offset ) {
	case 0: return (count >> 8) & 0xff;
	case 1: return count & 0xff;
	case 2: return (lt3 << 7) | (inil << 6);
	case 3: return (init << 7);
	default:
		logerror( "$%04x mo5_gatearray_r: invalid offset %i\n",  m_maincpu->pc(), offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::mo5_gatearray_w )
{
	if ( ! offset )
		m_to7_lightpen = data & 1;
}



/* ------------ cartridge / extended RAM ------------ */



DEVICE_IMAGE_LOAD_MEMBER( thomson_state, mo5_cartridge )
{
	UINT8* pos = memregion("maincpu")->base() + 0x10000;
	UINT64 size, i;
	int j;
	char name[129];

	if (image.software_entry() == NULL)
		size = image.length();
	else
		size = image.get_software_region_length("rom");

	/* get size & number of 16-KB banks */
	if ( size > 32 && size <= 0x04000 )
		m_thom_cart_nb_banks = 1;
	else if ( size == 0x08000 )
		m_thom_cart_nb_banks = 2;
	else if ( size == 0x10000 )
		m_thom_cart_nb_banks = 4;
	else
	{
		astring errmsg;
		errmsg.printf("Invalid cartridge size "I64FMT, size);
		image.seterror(IMAGE_ERROR_UNSUPPORTED, errmsg.cstr());
		return IMAGE_INIT_FAIL;
	}

	if (image.software_entry() == NULL)
	{
		if ( image.fread(pos, size ) != size )
		{
			image.seterror(IMAGE_ERROR_INVALIDIMAGE, "Read error");
			return IMAGE_INIT_FAIL;
		}
	}
	else
	{
		memcpy(pos, image.get_software_region("rom"), size);
	}

	/* extract name */
	i = size - 32;
	while ( i < size && !pos[i] ) i++;
	for ( j = 0; i < size && pos[i] >= 0x20; j++, i++)
		name[j] = pos[i];
	name[j] = 0;

	/* sanitize name */
	for ( j = 0; name[j]; j++)
	{
		if ( name[j] < ' ' || name[j] >= 127 ) name[j] = '?';
	}

	PRINT (( "mo5_cartridge_load: cartridge \"%s\" banks=%i, size=%u\n", name, m_thom_cart_nb_banks, (unsigned) size ));

	return IMAGE_INIT_PASS;
}



void thomson_state::mo5_update_cart_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	int rom_is_ram = m_mo5_reg_cart & 4;
	int bank = 0;
	int bank_is_read_only = 0;


	if ( rom_is_ram && m_thom_cart_nb_banks == 4 )
	{
		/* 64 KB ROM from "JANE" cartridge */
		bank = m_mo5_reg_cart & 3;
		if ( bank != m_old_cart_bank )
		{
			if ( m_old_cart_bank < 0 || m_old_cart_bank > 3 )
			{
				space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK);
				space.nop_write( 0xb000, 0xefff);
			}
			LOG_BANK(( "mo5_update_cart_bank: CART is cartridge bank %i (A7CB style)\n", bank ));
		}
	}
	else if ( rom_is_ram )
	{
		/* 64 KB RAM from network extension */
		bank = 4 + ( m_mo5_reg_cart & 3 );
		bank_is_read_only = (( m_mo5_reg_cart & 8 ) == 0);

		if ( bank != m_old_cart_bank || bank_is_read_only != m_old_cart_bank_was_read_only)
		{
			if ( bank_is_read_only )
			{
				space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK);
				space.nop_write( 0xb000, 0xefff );
			}
			else
			{
				space.install_readwrite_bank( 0xb000, 0xefff, THOM_CART_BANK);
			}
			LOG_BANK(( "mo5_update_cart_bank: CART is nanonetwork RAM bank %i (%s)\n",
						m_mo5_reg_cart & 3,
						bank_is_read_only ? "read-only":"read-write"));
			m_old_cart_bank_was_read_only = bank_is_read_only;
		}
	}
	else
	{
		/* regular cartridge bank switch */
		if ( m_thom_cart_nb_banks )
		{
			bank = m_thom_cart_bank % m_thom_cart_nb_banks;
			if ( bank != m_old_cart_bank )
			{
				if ( m_old_cart_bank < 0 )
				{
					space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK);
					space.install_write_handler( 0xb000, 0xefff, write8_delegate(FUNC(thomson_state::mo5_cartridge_w),this) );
					space.install_read_handler( 0xbffc, 0xbfff, read8_delegate(FUNC(thomson_state::mo5_cartridge_r),this) );
				}
				LOG_BANK(( "mo5_update_cart_bank: CART is cartridge bank %i\n", bank ));
			}
		}
		else
		{
			/* internal ROM */
			if ( m_old_cart_bank != 0 )
						{
				space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK);
				space.install_write_handler( 0xb000, 0xefff, write8_delegate(FUNC(thomson_state::mo5_cartridge_w),this) );
				LOG_BANK(( "mo5_update_cart_bank: CART is internal\n"));
			}
		}
	}
	if ( bank != m_old_cart_bank )
	{
		membank( THOM_CART_BANK )->set_entry( bank );
		m_old_cart_bank = bank;
	}
}



void thomson_state::mo5_update_cart_bank_postload()
{
	mo5_update_cart_bank();
}



/* write signal to b000-cfff generates a bank switch */
WRITE8_MEMBER( thomson_state::mo5_cartridge_w )
{
	if ( offset >= 0x2000 )
		return;

	m_thom_cart_bank = offset & 3;
	mo5_update_cart_bank();
}



/* read signal to bffc-bfff generates a bank switch */
READ8_MEMBER( thomson_state::mo5_cartridge_r )
{
	UINT8* pos = memregion( "maincpu" )->base() + 0x10000;
	UINT8 data = pos[offset + 0xbffc + (m_thom_cart_bank % m_thom_cart_nb_banks) * 0x4000];
	if ( !space.debugger_access() )
	{
		m_thom_cart_bank = offset & 3;
		mo5_update_cart_bank();
	}
	return data;
}



/* 0xa7cb bank-switch register */
WRITE8_MEMBER( thomson_state::mo5_ext_w )
{
	m_mo5_reg_cart = data;
	mo5_update_cart_bank();
}



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, mo5 )
{
	LOG (( "mo5: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	m_pia_sys->set_port_a_z_mask(0x5f );
	to7_game_reset();
	to7_floppy_reset();
	to7_modem_reset();
	to7_midi_reset();
	mo5_init_timer();

	/* video */
	thom_set_video_mode( THOM_VMODE_MO5 );
	m_thom_lightpen_cb = &thomson_state::mo5_lightpen_cb;
	thom_set_lightpen_callback( 3 );
	thom_set_mode_point( 0 );
	thom_set_border_color( 0 );
	m_pia_sys->ca1_w( 0 );

	/* memory */
	m_old_cart_bank = -1;
	mo5_update_cart_bank();
	/* mo5_reg_cart not reset */
	/* thom_cart_bank not reset */

	/* lightpen */
	m_to7_lightpen = 0;
}



MACHINE_START_MEMBER( thomson_state, mo5 )
{
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "mo5: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	to7_game_init();
	to7_floppy_init( mem + 0x20000 );
	to7_modem_init();
	to7_midi_init();
	m_mo5_periodic_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::mo5_periodic_cb),this));

	/* memory */
	m_thom_cart_bank = 0;
	m_mo5_reg_cart = 0;
	m_thom_vram = ram;
	membank( THOM_BASE_BANK )->configure_entry( 0, ram + 0x4000);
	membank( THOM_CART_BANK )->configure_entries( 0, 4, mem + 0x10000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 4, 4, ram + 0xc000, 0x4000 );
	membank( THOM_VRAM_BANK )->configure_entries( 0, 2, m_thom_vram, 0x2000 );
	membank( THOM_BASE_BANK )->set_entry( 0 );
	membank( THOM_CART_BANK )->set_entry( 0 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_item(NAME(m_mo5_reg_cart));
	save_pointer(NAME(mem + 0x10000), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::mo5_update_cart_bank_postload), this));
}



/***************************** TO9 *************************/



/* ------------ IEEE extension ------------ */



/* TODO: figure out what this extension is... IEEE-488 ??? */



WRITE8_MEMBER( thomson_state::to9_ieee_w )
{
	logerror( "$%04x %f to9_ieee_w: unhandled write $%02X to register %i\n", m_maincpu->pc(), machine().time().as_double(), data, offset );
}



READ8_MEMBER( thomson_state::to9_ieee_r )
{
	logerror( "$%04x %f to9_ieee_r: unhandled read from register %i\n", m_maincpu->pc(), machine().time().as_double(), offset );
	return 0;
}



/* ------------ system gate-array ------------ */



#define TO9_LIGHTPEN_DECAL 8



READ8_MEMBER( thomson_state::to9_gatearray_r )
{
	struct thom_vsignal v = thom_get_vsignal();
	struct thom_vsignal l = thom_get_lightpen_vsignal( TO9_LIGHTPEN_DECAL, m_to7_lightpen_step - 1, 0 );
	int count, inil, init, lt3;
	count = m_to7_lightpen ? l.count : v.count;
	inil  = m_to7_lightpen ? l.inil  : v.inil;
	init  = m_to7_lightpen ? l.init  : v.init;
	lt3   = m_to7_lightpen ? l.lt3   : v.lt3;

	switch ( offset )
	{
	case 0: return (count >> 8) & 0xff;
	case 1: return count & 0xff;
	case 2: return (lt3 << 7) | (inil << 6);
	case 3: return (v.init << 7) | (init << 6); /* != TO7/70 */
	default:
		logerror( "$%04x to9_gatearray_r: invalid offset %i\n", m_maincpu->pc(), offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::to9_gatearray_w )
{
	if ( ! offset )
		m_to7_lightpen = data & 1;
}



/* ------------ video gate-array ------------ */



/* style: 0 => TO9, 1 => TO8/TO9, 2 => MO6 */
void thomson_state::to9_set_video_mode( UINT8 data, int style )
{
	switch ( data & 0x7f )
	{
	case 0x00:
		if ( style == 2 )
			thom_set_video_mode( THOM_VMODE_MO5 );
		else if ( style == 1 )
			thom_set_video_mode( THOM_VMODE_TO770 );
		else
			thom_set_video_mode( THOM_VMODE_TO9 );
		break;

	case 0x21: thom_set_video_mode( THOM_VMODE_BITMAP4 );     break;

	case 0x41: thom_set_video_mode( THOM_VMODE_BITMAP4_ALT ); break;

	case 0x2a:
		if ( style==0 )
			thom_set_video_mode( THOM_VMODE_80_TO9 );
		else
			thom_set_video_mode( THOM_VMODE_80 );
		break;

	case 0x7b: thom_set_video_mode( THOM_VMODE_BITMAP16 );    break;

	case 0x24: thom_set_video_mode( THOM_VMODE_PAGE1 );       break;

	case 0x25: thom_set_video_mode( THOM_VMODE_PAGE2 );       break;

	case 0x26: thom_set_video_mode( THOM_VMODE_OVERLAY );     break;

	case 0x3f: thom_set_video_mode( THOM_VMODE_OVERLAY3 );    break;

	default:
		logerror( "to9_set_video_mode: unknown mode $%02X tr=%i phi=%i mod=%i\n", data, (data >> 5) & 3, (data >> 3) & 2, data & 7 );
	}
}



READ8_MEMBER( thomson_state::to9_vreg_r )
{
	switch ( offset )
	{
	case 0: /* palette data */
	{
		UINT8 c =  m_to9_palette_data[ m_to9_palette_idx ];
		if ( !space.debugger_access() )
		{
			m_to9_palette_idx = ( m_to9_palette_idx + 1 ) & 31;
		}
		return c;
	}

	case 1: /* palette address */
		return m_to9_palette_idx;

	case 2:
	case 3:
		return 0;

	default:
		logerror( "to9_vreg_r: invalid read offset %i\n", offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::to9_vreg_w )
{
	LOG_VIDEO(( "$%04x %f to9_vreg_w: off=%i ($%04X) data=$%02X\n", m_maincpu->pc(), machine().time().as_double(), offset, 0xe7da + offset, data ));

	switch ( offset )
	{
	case 0: /* palette data */
	{
		UINT16 color, idx;
		m_to9_palette_data[ m_to9_palette_idx ] = data;
		idx = m_to9_palette_idx / 2;
		color = m_to9_palette_data[ 2 * idx + 1 ];
		color = m_to9_palette_data[ 2 * idx ] | (color << 8);
		thom_set_palette( idx ^ 8, color & 0x1fff );

		m_to9_palette_idx = ( m_to9_palette_idx + 1 ) & 31;
	}
	break;

	case 1: /* palette address */
		m_to9_palette_idx = data & 31;
		break;

	case 2: /* video mode */
		to9_set_video_mode( data, 0 );
		break;

	case 3: /* border color */
		thom_set_border_color( data & 15 );
		break;

	default:
		logerror( "to9_vreg_w: invalid write offset %i data=$%02X\n", offset, data );
	}
}



void thomson_state::to9_palette_init()
{
	m_to9_palette_idx = 0;
	memset( m_to9_palette_data, 0, sizeof( m_to9_palette_data ) );
	save_item(NAME(m_to9_palette_idx));
	save_item(NAME(m_to9_palette_data));
}



/* ------------ RAM / ROM banking ------------ */


void thomson_state::to9_update_cart_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	int bank = 0;
	int slot = ( m_mc6846->get_output_port() >> 4 ) & 3; /* bits 4-5: ROM bank */

	switch ( slot )
	{
	case 0:
		/* BASIC (64 KB) */
		bank = 4 + m_to9_soft_bank;
		if ( bank != m_old_cart_bank )
		{
			if ( m_old_cart_bank < 4)
			{
				space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
			}
			LOG_BANK(( "to9_update_cart_bank: CART is BASIC bank %i\n", m_to9_soft_bank ));
		}
		break;
	case 1:
		/* software 1 (32 KB) */
		bank = 8 + (m_to9_soft_bank & 1);
		if ( bank != m_old_cart_bank )
		{
			if ( m_old_cart_bank < 4)
			{
				space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
			}
			LOG_BANK(( "to9_update_cart_bank: CART is software 1 bank %i\n", m_to9_soft_bank ));
		}
		break;
	case 2:
		/* software 2 (32 KB) */
		bank = 10 + (m_to9_soft_bank & 1);
		if ( bank != m_old_cart_bank )
		{
			if ( m_old_cart_bank < 4)
			{
				space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
			}
			LOG_BANK(( "to9_update_cart_bank: CART is software 2 bank %i\n", m_to9_soft_bank ));
		}
		break;
	case 3:
		/* external cartridge */
		if ( m_thom_cart_nb_banks )
		{
			bank = m_thom_cart_bank % m_thom_cart_nb_banks;
			if ( bank != m_old_cart_bank )
			{
				if ( m_old_cart_bank < 0 || m_old_cart_bank > 3 )
				{
					space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
					space.install_write_handler( 0x0000, 0x3fff, write8_delegate(FUNC(thomson_state::to9_cartridge_w),this) );
					space.install_read_handler( 0x0000, 0x0003, read8_delegate(FUNC(thomson_state::to9_cartridge_r),this) );
				}
				LOG_BANK(( "to9_update_cart_bank: CART is cartridge bank %i\n",  m_thom_cart_bank ));
			}
		}
		else
		{
			if ( m_old_cart_bank != 0 )
			{
				space.nop_read( 0x0000, 0x3fff);
				LOG_BANK(( "to9_update_cart_bank: CART is unmapped\n"));
			}
		}
		break;
	}
	if ( bank != m_old_cart_bank )
	{
		membank( THOM_CART_BANK )->set_entry( bank );
		m_old_cart_bank = bank;
	}
}



void thomson_state::to9_update_cart_bank_postload()
{
	to9_update_cart_bank();
}



/* write signal to 0000-1fff generates a bank switch */
WRITE8_MEMBER( thomson_state::to9_cartridge_w )
{
	int slot = ( m_mc6846->get_output_port() >> 4 ) & 3; /* bits 4-5: ROM bank */

	if ( offset >= 0x2000 )
		return;

	if ( slot == 3 )
		m_thom_cart_bank = offset & 3;
	else
		m_to9_soft_bank = offset & 3;
	to9_update_cart_bank();
}



/* read signal to 0000-0003 generates a bank switch */
READ8_MEMBER( thomson_state::to9_cartridge_r )
{
	UINT8* pos = memregion( "maincpu" )->base() + 0x10000;
	UINT8 data = pos[offset + (m_thom_cart_bank % m_thom_cart_nb_banks) * 0x4000];
	if ( !space.debugger_access() )
	{
		m_thom_cart_bank = offset & 3;
		to9_update_cart_bank();
	}
	return data;
}



void thomson_state::to9_update_ram_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	UINT8 port = m_mc6846->get_output_port();
	UINT8 portb = m_pia_sys->port_b_z_mask();
	UINT8 disk = ((port >> 2) & 1) | ((port >> 5) & 2); /* bits 6,2: RAM bank */
	int bank;

	switch ( portb & 0xf8 )
	{
		/* TO7/70 compatible */
	case 0xf0: bank = 0; break;
	case 0xe8: bank = 1; break;
	case 0x18: bank = 2; break;
	case 0x98: bank = 3; break;
	case 0x58: bank = 4; break;
	case 0xd8: bank = 5; break;

		/* 64 KB of virtual disk */
	case 0xf8: bank = 6 + disk ; break;

		/* none selected */
	case 0: return;

	default:
		logerror( "to9_update_ram_bank: unknown RAM bank pia=$%02X disk=%i\n", portb & 0xf8, disk );
		return;
	}

	if ( m_old_ram_bank != bank )
	{
		if ( m_ram->size() == 192*1024 || bank < 6 )
		{
			membank( THOM_RAM_BANK )->set_entry( bank );
		}
		else
		{
			space.nop_readwrite( 0xa000, 0xdfff);
		}
		m_old_ram_bank = bank;
		LOG_BANK(( "to9_update_ram_bank: bank %i selected (pia=$%02X disk=%i)\n", bank, portb & 0xf8, disk ));
	}
}



void thomson_state::to9_update_ram_bank_postload()
{
	to9_update_ram_bank();
}



/* ------------ keyboard (6850 ACIA + 6805 CPU) ------------ */

/* The 6805 chip scans the keyboard and sends ASCII codes to the 6909.
   Data between the 6809 and 6805 is serialized at 9600 bauds.
   On the 6809 side, a 6850 ACIA is used.
   We do not emulate the seral line but pass bytes directly between the
   keyboard and the 6850 registers.
   Note that the keyboard protocol uses the parity bit as an extra data bit.
*/



/* normal mode: polling interval */
#define TO9_KBD_POLL_PERIOD  attotime::from_msec( 10 )

/* peripherial mode: time between two bytes, and after last byte */
#define TO9_KBD_BYTE_SPACE   attotime::from_usec( 300 )
#define TO9_KBD_END_SPACE    attotime::from_usec( 9100 )

/* first and subsequent repeat periods, in TO9_KBD_POLL_PERIOD units */
#define TO9_KBD_REPEAT_DELAY  80 /* 800 ms */
#define TO9_KBD_REPEAT_PERIOD  7 /*  70 ms */



/* quick keyboard scan */
int thomson_state::to9_kbd_ktest()
{
	int line, bit;
	UINT8 port;
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3", "keyboard_4",
		"keyboard_5", "keyboard_6", "keyboard_7", "keyboard_8", "keyboard_9"
	};

	for ( line = 0; line < 10; line++ )
	{
		port = ioport(keynames[line])->read();

		if ( line == 7 || line == 9 )
			port |= 1; /* shift & control */

		for ( bit = 0; bit < 8; bit++ )
		{
			if ( ! (port & (1 << bit)) )
				return 1;
		}
	}
	return 0;
}



void thomson_state::to9_kbd_update_irq()
{
	if ( (m_to9_kbd_intr & 4) && (m_to9_kbd_status & ACIA_6850_RDRF) )
		m_to9_kbd_status |= ACIA_6850_irq; /* byte received interrupt */

	if ( (m_to9_kbd_intr & 4) && (m_to9_kbd_status & ACIA_6850_OVRN) )
		m_to9_kbd_status |= ACIA_6850_irq; /* overrun interrupt */

	if ( (m_to9_kbd_intr & 3) == 1 && (m_to9_kbd_status & ACIA_6850_TDRE) )
		m_to9_kbd_status |= ACIA_6850_irq; /* ready to transmit interrupt */

	thom_irq_3( m_to9_kbd_status & ACIA_6850_irq );
}



READ8_MEMBER( thomson_state::to9_kbd_r )
{
	/* ACIA 6850 registers */

	switch ( offset )
	{
	case 0: /* get status */
		/* bit 0:     data received */
		/* bit 1:     ready to transmit data (always 1) */
		/* bit 2:     data carrier detect (ignored) */
		/* bit 3:     clear to send (ignored) */
		/* bit 4:     framing error (ignored) */
		/* bit 5:     overrun */
		/* bit 6:     parity error */
		/* bit 7:     interrupt */

		LOG_KBD(( "$%04x %f to9_kbd_r: status $%02X (rdrf=%i, tdre=%i, ovrn=%i, pe=%i, irq=%i)\n",
				m_maincpu->pc(), machine().time().as_double(), m_to9_kbd_status,
				(m_to9_kbd_status & ACIA_6850_RDRF) ? 1 : 0,
				(m_to9_kbd_status & ACIA_6850_TDRE) ? 1 : 0,
				(m_to9_kbd_status & ACIA_6850_OVRN) ? 1 : 0,
				(m_to9_kbd_status & ACIA_6850_PE) ? 1 : 0,
				(m_to9_kbd_status & ACIA_6850_irq) ? 1 : 0 ));
		return m_to9_kbd_status;

	case 1: /* get input data */
		if ( !space.debugger_access() )
		{
			m_to9_kbd_status &= ~(ACIA_6850_irq | ACIA_6850_PE);
			if ( m_to9_kbd_overrun )
				m_to9_kbd_status |= ACIA_6850_OVRN;
			else
				m_to9_kbd_status &= ~(ACIA_6850_OVRN | ACIA_6850_RDRF);
			m_to9_kbd_overrun = 0;
			LOG_KBD(( "$%04x %f to9_kbd_r: read data $%02X\n", m_maincpu->pc(), machine().time().as_double(), m_to9_kbd_in ));
			to9_kbd_update_irq();
		}
		return m_to9_kbd_in;

	default:
		logerror( "$%04x to9_kbd_r: invalid offset %i\n", m_maincpu->pc(),  offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::to9_kbd_w )
{
	/* ACIA 6850 registers */

	switch ( offset )
	{
	case 0: /* set control */
		/* bits 0-1: clock divide (ignored) or reset */
		if ( (data & 3) == 3 )
		{
			/* reset */
			m_to9_kbd_overrun = 0;
			m_to9_kbd_status = ACIA_6850_TDRE;
			m_to9_kbd_intr = 0;
			LOG_KBD(( "$%04x %f to9_kbd_w: reset (data=$%02X)\n", m_maincpu->pc(), machine().time().as_double(), data ));
		}
		else
		{
			/* bits 2-4: parity */
			if ( (data & 0x18) == 0x10 )
				m_to9_kbd_parity = 2;
			else
				m_to9_kbd_parity = (data >> 2) & 1;
			/* bits 5-6: interrupt on transmit */
			/* bit 7:    interrupt on receive */
			m_to9_kbd_intr = data >> 5;

			LOG_KBD(( "$%04x %f to9_kbd_w: set control to $%02X (parity=%i, intr in=%i out=%i)\n",
					m_maincpu->pc(), machine().time().as_double(),
					data, m_to9_kbd_parity, m_to9_kbd_intr >> 2,
					(m_to9_kbd_intr & 3) ? 1 : 0 ));
		}
		to9_kbd_update_irq();
		break;

	case 1: /* output data */
		m_to9_kbd_status &= ~(ACIA_6850_irq | ACIA_6850_TDRE);
		to9_kbd_update_irq();
		/* TODO: 1 ms delay here ? */
		m_to9_kbd_status |= ACIA_6850_TDRE; /* data transmit ready again */
		to9_kbd_update_irq();

		switch ( data )
		{
		case 0xF8:
			/* reset */
			m_to9_kbd_caps = 1;
			m_to9_kbd_periph = 0;
			m_to9_kbd_pad = 0;
			break;

		case 0xF9: m_to9_kbd_caps = 1;   break;
		case 0xFA: m_to9_kbd_caps = 0;   break;
		case 0xFB: m_to9_kbd_pad = 1;    break;
		case 0xFC: m_to9_kbd_pad = 0;    break;
		case 0xFD: m_to9_kbd_periph = 1; break;
		case 0xFE: m_to9_kbd_periph = 0; break;

		default:
			logerror( "$%04x %f to9_kbd_w: unknown kbd command %02X\n", m_maincpu->pc(), machine().time().as_double(), data );
		}

		thom_set_caps_led( !m_to9_kbd_caps );

		LOG(( "$%04x %f to9_kbd_w: kbd command %02X (caps=%i, pad=%i, periph=%i)\n",
				m_maincpu->pc(), machine().time().as_double(), data,
				m_to9_kbd_caps, m_to9_kbd_pad, m_to9_kbd_periph ));

		break;

	default:
		logerror( "$%04x to9_kbd_w: invalid offset %i (data=$%02X) \n", m_maincpu->pc(), offset, data );
	}
}



/* send a key to the CPU, 8-bit + parity bit (0=even, 1=odd)
   note: parity is not used as a checksum but to actually transmit a 9-th bit
   of information!
*/
void thomson_state::to9_kbd_send( UINT8 data, int parity )
{
	if ( m_to9_kbd_status & ACIA_6850_RDRF )
	{
		/* overrun will be set when the current valid byte is read */
		m_to9_kbd_overrun = 1;
		LOG_KBD(( "%f to9_kbd_send: overrun => drop data=$%02X, parity=%i\n", machine().time().as_double(), data, parity ));
	}
	else
	{
		/* valid byte */
		m_to9_kbd_in = data;
		m_to9_kbd_status |= ACIA_6850_RDRF; /* raise data received flag */
		if ( m_to9_kbd_parity == 2 || m_to9_kbd_parity == parity )
			m_to9_kbd_status &= ~ACIA_6850_PE; /* parity OK */
		else
			m_to9_kbd_status |= ACIA_6850_PE;  /* parity error */
		LOG_KBD(( "%f to9_kbd_send: data=$%02X, parity=%i, status=$%02X\n", machine().time().as_double(), data, parity, m_to9_kbd_status ));
	}
	to9_kbd_update_irq();
}



/* keycode => TO9 code (extended ASCII), shifted and un-shifted */
static const int to9_kbd_code[80][2] =
{
	{ 145, 150 }, { '_', '6' }, { 'Y', 'Y' }, { 'H', 'H' },
	{ 11, 11 }, { 9, 9 }, { 30, 12 }, { 'N', 'N' },

	{ 146, 151 }, { '(', '5' }, { 'T', 'T' }, { 'G', 'G' },
	{ '=', '+' }, { 8, 8 }, { 28, 28 }, { 'B', 'B' },

	{ 147, 152 }, { '\'', '4' }, { 'R', 'R' }, { 'F', 'F' },
	{ 22, 22 },  { 155, 155 }, { 29, 127 }, { 'V', 'V' },

	{ 148, 153 }, { '"', '3' }, { 'E', 'E' }, { 'D', 'D' },
	{ 161, 161 }, { 158, 158 },
	{ 154, 154 }, { 'C', 'C' },

	{ 144, 149 }, { 128, '2' }, { 'Z', 'Z' }, { 'S', 'S' },
	{ 162, 162 }, { 156, 156 },
	{ 164, 164 }, { 'X', 'X' },

	{ '#', '@' }, { '*', '1' }, { 'A', 'A' }, { 'Q', 'Q' },
	{ '[', '{' }, { 159, 159 }, { 160, 160 }, { 'W', 'W' },

	{ 2, 2 }, { 129, '7' }, { 'U', 'U' }, { 'J', 'J' },
	{ ' ', ' ' }, { 163, 163 }, { 165, 165 },
	{ ',', '?' },

	{ 0, 0 }, { '!', '8' }, { 'I', 'I' }, { 'K', 'K' },
	{ '$', '&' }, { 10, 10 }, { ']', '}' },  { ';', '.' },

	{ 0, 0 }, { 130, '9' }, { 'O', 'O' }, { 'L', 'L' },
	{ '-', '\\' }, { 132, '%' }, { 13, 13 }, { ':', '/' },

	{ 0, 0 }, { 131, '0' }, { 'P', 'P' }, { 'M', 'M' },
	{ ')', 134 }, { '^', 133 }, { 157, 157 }, { '>', '<' }
};



/* returns the ASCII code for the key, or 0 for no key */
int thomson_state::to9_kbd_get_key()
{
	int control = ! (ioport("keyboard_7")->read() & 1);
	int shift   = ! (ioport("keyboard_9")->read() & 1);
	int key = -1, line, bit;
	UINT8 port;
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3", "keyboard_4",
		"keyboard_5", "keyboard_6", "keyboard_7", "keyboard_8", "keyboard_9"
	};

	for ( line = 0; line < 10; line++ )
	{
		port = ioport(keynames[line])->read();

		if ( line == 7 || line == 9 )
			port |= 1; /* shift & control */

		/* TODO: correct handling of simultaneous keystokes:
		   return the new key preferably & disable repeat
		*/
		for ( bit = 0; bit < 8; bit++ )
		{
			if ( ! (port & (1 << bit)) )
				key = line * 8 + bit;
		}
	}

	if ( key == -1 )
	{
		m_to9_kbd_last_key = 0xff;
		m_to9_kbd_key_count = 0;
		return 0;
	}
	else if ( key == 64 )
	{
		/* caps lock */
		if ( m_to9_kbd_last_key == key )
			return 0; /* no repeat */

		m_to9_kbd_last_key = key;
		m_to9_kbd_caps = !m_to9_kbd_caps;
		thom_set_caps_led( !m_to9_kbd_caps );
		return 0;
	}
	else
	{
		int asc;
		asc = to9_kbd_code[key][shift];
		if ( ! asc ) return 0;

		/* keypad */
		if ( ! m_to9_kbd_pad ) {
			if ( asc >= 154 && asc <= 163 )
				asc += '0' - 154;
			else if ( asc == 164 )
				asc = '.';
			else if ( asc == 165 )
				asc = 13;
		}

		/* shifted letter */
		if ( asc >= 'A' && asc <= 'Z' && ( ! m_to9_kbd_caps ) && ( ! shift ) )
			asc += 'a' - 'A';

		/* control */
		if ( control )
			asc &= ~0x40;

		if ( key == m_to9_kbd_last_key )
		{
			/* repeat */
			m_to9_kbd_key_count++;
			if ( m_to9_kbd_key_count < TO9_KBD_REPEAT_DELAY || (m_to9_kbd_key_count - TO9_KBD_REPEAT_DELAY) % TO9_KBD_REPEAT_PERIOD )
				return 0;
			LOG_KBD(( "to9_kbd_get_key: repeat key $%02X '%c'\n", asc, asc ));
			return asc;
		}
		else
		{
			m_to9_kbd_last_key = key;
			m_to9_kbd_key_count = 0;
			LOG_KBD(( "to9_kbd_get_key: key down $%02X '%c'\n", asc, asc ));
			return asc;
		}
	}
}



TIMER_CALLBACK_MEMBER(thomson_state::to9_kbd_timer_cb)
{
	if ( m_to9_kbd_periph )
	{
		/* peripherial mode: every 10 ms we send 4 bytes */

		switch ( m_to9_kbd_byte_count )
		{
		case 0: /* key */
			to9_kbd_send( to9_kbd_get_key(), 0 );
			break;

		case 1: /* x axis */
		{
			int newx = ioport("mouse_x")->read();
			UINT8 data = ( (newx - m_to9_mouse_x) & 0xf ) - 8;
			to9_kbd_send( data, 1 );
			m_to9_mouse_x = newx;
			break;
		}

		case 2: /* y axis */
		{
			int newy = ioport("mouse_y")->read();
			UINT8 data = ( (newy - m_to9_mouse_y) & 0xf ) - 8;
			to9_kbd_send( data, 1 );
			m_to9_mouse_y = newy;
			break;
		}

		case 3: /* axis overflow & buttons */
		{
			int b = ioport("mouse_button")->read();
			UINT8 data = 0;
			if ( b & 1 ) data |= 1;
			if ( b & 2 ) data |= 4;
			to9_kbd_send( data, 1 );
			break;
		}

		}

		m_to9_kbd_byte_count = ( m_to9_kbd_byte_count + 1 ) & 3;
		m_to9_kbd_timer->adjust(m_to9_kbd_byte_count ? TO9_KBD_BYTE_SPACE : TO9_KBD_END_SPACE);
	}
	else
	{
		int key = to9_kbd_get_key();
		/* keyboard mode: send a byte only if a key is down */
		if ( key )
			to9_kbd_send( key, 0 );
		m_to9_kbd_timer->adjust(TO9_KBD_POLL_PERIOD);
	}
}



void thomson_state::to9_kbd_reset()
{
	LOG(( "to9_kbd_reset called\n" ));
	m_to9_kbd_overrun = 0;  /* no byte lost */
	m_to9_kbd_status = ACIA_6850_TDRE;  /* clear to transmit */
	m_to9_kbd_intr = 0;     /* interrupt disabled */
	m_to9_kbd_caps = 1;
	m_to9_kbd_periph = 0;
	m_to9_kbd_pad = 0;
	m_to9_kbd_byte_count = 0;
	thom_set_caps_led( !m_to9_kbd_caps );
	m_to9_kbd_key_count = 0;
	m_to9_kbd_last_key = 0xff;
	to9_kbd_update_irq();
	m_to9_kbd_timer->adjust(TO9_KBD_POLL_PERIOD);
}



void thomson_state::to9_kbd_init()
{
	LOG(( "to9_kbd_init called\n" ));
	m_to9_kbd_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::to9_kbd_timer_cb),this));
	save_item(NAME(m_to9_kbd_parity));
	save_item(NAME(m_to9_kbd_intr));
	save_item(NAME(m_to9_kbd_in));
	save_item(NAME(m_to9_kbd_status));
	save_item(NAME(m_to9_kbd_overrun));
	save_item(NAME(m_to9_kbd_last_key));
	save_item(NAME(m_to9_kbd_key_count));
	save_item(NAME(m_to9_kbd_caps));
	save_item(NAME(m_to9_kbd_periph));
	save_item(NAME(m_to9_kbd_pad));
	save_item(NAME(m_to9_kbd_byte_count));
	save_item(NAME(m_to9_mouse_x));
	save_item(NAME(m_to9_mouse_y));
}


/* ------------ system PIA 6821 ------------ */

/* afaik, P2-P7 are not connected, so, the warning about undefined 0xf0 can be safely ignored */


READ8_MEMBER( thomson_state::to9_sys_porta_in )
{
	UINT8 ktest = to9_kbd_ktest();

	LOG_KBD(( "to9_sys_porta_in: ktest=%i\n", ktest ));

	return ktest;
}



WRITE8_MEMBER( thomson_state::to9_sys_porta_out )
{
	m_centronics->write(space, 0, data & 0xfe);
}



WRITE8_MEMBER( thomson_state::to9_sys_portb_out )
{
	m_centronics->d0_w(BIT(data, 0));
	m_centronics->strobe_w(BIT(data, 1));

	to9_update_ram_bank();

	if ( data & 4 ) /* bit 2: video overlay (TODO) */
		LOG(( "to9_sys_portb_out: video overlay not handled\n" ));
}



/* ------------ 6846 (timer, I/O) ------------ */



WRITE8_MEMBER( thomson_state::to9_timer_port_out )
{
	thom_set_mode_point( data & 1 ); /* bit 0: video bank */
	to9_update_ram_bank();
	to9_update_cart_bank();
}



const mc6846_interface to9_timer =
{
	DEVCB_DRIVER_MEMBER(thomson_state, to9_timer_port_out),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_cp2_out),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_port_in),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_tco_out),
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, thom_dev_irq_0)
};



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, to9 )
{
	LOG (( "to9: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	m_pia_sys->set_port_a_z_mask( 0xfe );
	to7_game_reset();
	to9_floppy_reset();
	to9_kbd_reset();
	to7_modem_reset();
	to7_midi_reset();

	/* video */
	thom_set_video_mode( THOM_VMODE_TO9 );
	m_thom_lightpen_cb = &thomson_state::to7_lightpen_cb;
	thom_set_lightpen_callback( 3 );
	thom_set_border_color( 8 );
	thom_set_mode_point( 0 );
	m_pia_sys->cb1_w( 0 );

	/* memory */
	m_old_ram_bank = -1;
	m_old_cart_bank = -1;
	m_to9_soft_bank = 0;
	to9_update_cart_bank();
	to9_update_ram_bank();
	/* thom_cart_bank not reset */

	/* lightpen */
	m_to7_lightpen = 0;
}



MACHINE_START_MEMBER( thomson_state, to9 )
{
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "to9: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	to7_game_init();
	to9_floppy_init( mem + 0xe000, mem + 0x40000 );
	to9_kbd_init();
	to9_palette_init();
	to7_modem_init();
	to7_midi_init();

	/* memory */
	m_thom_vram = ram;
	m_thom_cart_bank = 0;
	membank( THOM_VRAM_BANK )->configure_entries( 0,  2, m_thom_vram, 0x2000 );
	membank( THOM_CART_BANK )->configure_entries( 0, 12, mem + 0x10000, 0x4000 );
	membank( THOM_BASE_BANK )->configure_entry( 0,  ram + 0x4000);
	membank( THOM_RAM_BANK )->configure_entries( 0, 10, ram + 0x8000, 0x4000 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );
	membank( THOM_CART_BANK )->set_entry( 0 );
	membank( THOM_BASE_BANK )->set_entry( 0 );
	membank( THOM_RAM_BANK )->set_entry( 0 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_item(NAME(m_to9_soft_bank));
	save_pointer(NAME(mem + 0x10000), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to9_update_ram_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to9_update_cart_bank_postload), this));
}



/***************************** TO8 *************************/


/* ------------ keyboard (6804) ------------ */

/* The 6804 chip scans the keyboard and sends keycodes to the 6809.
   Data is serialized using variable pulse length encoding.
   Unlike the TO9, there is no decoding chip on the 6809 side, only
   1-bit PIA ports (6821 & 6846). The 6809 does the decoding.

   We do not emulate the 6804 but pass serialized data directly through the
   PIA ports.

   Note: if we conform to the (scarce) documentation the CPU tend to lock
   waitting for keyboard input.
   The protocol documentation is pretty scarce and does not account for these
   behaviors!
   The emulation code contains many hacks (delays, timeouts, spurious
   pulses) to improve the stability.
   This works well, but is not very accurate.
*/



/* polling interval */
#define TO8_KBD_POLL_PERIOD  attotime::from_msec( 1 )

/* first and subsequent repeat periods, in TO8_KBD_POLL_PERIOD units */
#define TO8_KBD_REPEAT_DELAY  800 /* 800 ms */
#define TO8_KBD_REPEAT_PERIOD  70 /*  70 ms */

/* timeout waiting for CPU */
#define TO8_KBD_TIMEOUT  attotime::from_msec( 100 )



/* quick keyboard scan */
int thomson_state::to8_kbd_ktest()
{
	int line, bit;
	UINT8 port;
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3", "keyboard_4",
		"keyboard_5", "keyboard_6", "keyboard_7", "keyboard_8", "keyboard_9"
	};

	if ( ioport("config")->read() & 2 )
		return 0; /* disabled */

	for ( line = 0; line < 10; line++ )
	{
		port = ioport(keynames[line])->read();

		if ( line == 7 || line == 9 )
			port |= 1; /* shift & control */

		for ( bit = 0; bit < 8; bit++ )
		{
			if ( ! (port & (1 << bit)) )
				return 1;
		}
	}

	return 0;
}



/* keyboard scan & return keycode (or -1) */
int thomson_state::to8_kbd_get_key()
{
	int control = (ioport("keyboard_7")->read() & 1) ? 0 : 0x100;
	int shift   = (ioport("keyboard_9")->read() & 1) ? 0 : 0x080;
	int key = -1, line, bit;
	UINT8 port;
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3", "keyboard_4",
		"keyboard_5", "keyboard_6", "keyboard_7", "keyboard_8", "keyboard_9"
	};

	if ( ioport("config")->read() & 2 )
		return -1; /* disabled */

	for ( line = 0; line < 10; line++ )
	{
		port = ioport(keynames[line])->read();

		if ( line == 7 || line == 9 )
			port |= 1; /* shift & control */

		/* TODO: correct handling of simultaneous keystokes:
		   return the new key preferably & disable repeat
		*/
		for ( bit = 0; bit < 8; bit++ )
		{
			if ( ! (port & (1 << bit)) )
				key = line * 8 + bit;
		}
	}

	if ( key == -1 )
	{
		m_to8_kbd_last_key = 0xff;
		m_to8_kbd_key_count = 0;
		return -1;
	}
	else if ( key == 64 )
	{
		/* caps lock */
		if ( m_to8_kbd_last_key == key )
			return -1; /* no repeat */
		m_to8_kbd_last_key = key;
		m_to8_kbd_caps = !m_to8_kbd_caps;
		if ( m_to8_kbd_caps )
			key |= 0x080; /* auto-shift */
		thom_set_caps_led( !m_to8_kbd_caps );
		return key;
	}
	else if ( key == m_to8_kbd_last_key )
	{
		/* repeat */
		m_to8_kbd_key_count++;
		if ( m_to8_kbd_key_count < TO8_KBD_REPEAT_DELAY || (m_to8_kbd_key_count - TO8_KBD_REPEAT_DELAY) % TO8_KBD_REPEAT_PERIOD )
			return -1;
		return key | shift | control;
	}
	else
	{
		m_to8_kbd_last_key = key;
		m_to8_kbd_key_count = 0;
		return key | shift | control;
	}
}


/* steps:
   0     = idle, key polling
   1     = wait for ack to go down (key to send)
   99-117 = key data transmit
   91-117 = signal
   255    = timeout
*/

/* keyboard automaton */
void thomson_state::to8_kbd_timer_func()
{
	attotime d;

	LOG_KBD(( "%f to8_kbd_timer_cb: step=%i ack=%i data=$%03X\n", machine().time().as_double(), m_to8_kbd_step, m_to8_kbd_ack, m_to8_kbd_data ));

	if( ! m_to8_kbd_step )
	{
		/* key polling */
		int k = to8_kbd_get_key();
		/* if not in transfer, send pulse from time to time
		   (helps avoiding CPU lock)
		*/
		if ( ! m_to8_kbd_ack )
			m_mc6846->set_input_cp1(0);
		m_mc6846->set_input_cp1(1);

		if ( k == -1 )
			d = TO8_KBD_POLL_PERIOD;
		else
		{
			/* got key! */
			LOG_KBD(( "to8_kbd_timer_cb: got key $%03X\n", k ));
			m_to8_kbd_data = k;
			m_to8_kbd_step = 1;
			d = attotime::from_usec( 100 );
		}
	}
	else if ( m_to8_kbd_step == 255 )
	{
		/* timeout */
		m_to8_kbd_last_key = 0xff;
		m_to8_kbd_key_count = 0;
		m_to8_kbd_step = 0;
		m_mc6846->set_input_cp1(1);
		d = TO8_KBD_POLL_PERIOD;
	}
	else if ( m_to8_kbd_step == 1 )
	{
		/* schedule timeout waiting for ack to go down */
		m_mc6846->set_input_cp1(0);
		m_to8_kbd_step = 255;
		d = TO8_KBD_TIMEOUT;
	}
	else if ( m_to8_kbd_step == 117 )
	{
		/* schedule timeout  waiting for ack to go up */
		m_mc6846->set_input_cp1(0);
		m_to8_kbd_step = 255;
		d = TO8_KBD_TIMEOUT;
	}
	else if ( m_to8_kbd_step & 1 )
	{
		/* send silence between bits */
		m_mc6846->set_input_cp1(0);
		d = attotime::from_usec( 100 );
		m_to8_kbd_step++;
	}
	else
	{
		/* send bit */
		int bpos = 8 - ( (m_to8_kbd_step - 100) / 2);
		int bit = (m_to8_kbd_data >> bpos) & 1;
		m_mc6846->set_input_cp1(1);
		d = attotime::from_usec( bit ? 56 : 38 );
		m_to8_kbd_step++;
	}
	m_to8_kbd_timer->adjust(d);
}



TIMER_CALLBACK_MEMBER(thomson_state::to8_kbd_timer_cb)
{
	to8_kbd_timer_func();
}



/* cpu <-> keyboard hand-check */
void thomson_state::to8_kbd_set_ack( int data )
{
	if ( data == m_to8_kbd_ack )
		return;
	m_to8_kbd_ack = data;

	if ( data )
	{
		double len = m_to8_kbd_signal->elapsed( ).as_double() * 1000. - 2.;
		LOG_KBD(( "%f to8_kbd_set_ack: CPU end ack, len=%f\n", machine().time().as_double(), len ));
		if ( m_to8_kbd_data == 0xfff )
		{
			/* end signal from CPU */
			if ( len >= 0.6 && len <= 0.8 )
			{
				LOG (( "%f to8_kbd_set_ack: INIT signal\n", machine().time().as_double() ));
				m_to8_kbd_last_key = 0xff;
				m_to8_kbd_key_count = 0;
				m_to8_kbd_caps = 1;
				/* send back signal: TODO returned codes ? */
				m_to8_kbd_data = 0;
				m_to8_kbd_step = 0;
				m_to8_kbd_timer->adjust(attotime::from_msec( 1 ));
			}
			else
			{
				m_to8_kbd_step = 0;
				m_to8_kbd_timer->adjust(TO8_KBD_POLL_PERIOD);
				if ( len >= 1.2 && len <= 1.4 )
				{
					LOG (( "%f to8_kbd_set_ack: CAPS on signal\n", machine().time().as_double() ));
					m_to8_kbd_caps = 1;
				}
				else if ( len >= 1.8 && len <= 2.0 )
				{
					LOG (( "%f to8_kbd_set_ack: CAPS off signal\n", machine().time().as_double() ));
					m_to8_kbd_caps = 0;
				}
			}
			thom_set_caps_led( !m_to8_kbd_caps );
		}
		else
		{
			/* end key transmission */
			m_to8_kbd_step = 0;
			m_to8_kbd_timer->adjust(TO8_KBD_POLL_PERIOD);
		}
	}

	else
	{
		if ( m_to8_kbd_step == 255 )
		{
			/* CPU accepts key */
			m_to8_kbd_step = 99;
			m_to8_kbd_timer->adjust(attotime::from_usec( 400 ));
		}
		else
		{
			/* start signal from CPU */
			m_to8_kbd_data = 0xfff;
			m_to8_kbd_step = 91;
			m_to8_kbd_timer->adjust(attotime::from_usec( 400 ));
			m_to8_kbd_signal->adjust(attotime::never);
		}
		LOG_KBD(( "%f to8_kbd_set_ack: CPU ack, data=$%03X\n", machine().time().as_double(), m_to8_kbd_data ));
	}
}



void thomson_state::to8_kbd_reset()
{
	m_to8_kbd_last_key = 0xff;
	m_to8_kbd_key_count = 0;
	m_to8_kbd_step = 0;
	m_to8_kbd_data = 0;
	m_to8_kbd_ack = 1;
	m_to8_kbd_caps = 1;
	thom_set_caps_led( !m_to8_kbd_caps );
	to8_kbd_timer_func();
}



void thomson_state::to8_kbd_init()
{
	m_to8_kbd_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::to8_kbd_timer_cb),this));
	m_to8_kbd_signal = machine().scheduler().timer_alloc(FUNC_NULL);
	save_item(NAME(m_to8_kbd_ack));
	save_item(NAME(m_to8_kbd_data));
	save_item(NAME(m_to8_kbd_step));
	save_item(NAME(m_to8_kbd_last_key));
	save_item(NAME(m_to8_kbd_key_count));
	save_item(NAME(m_to8_kbd_caps));
}



/* ------------ RAM / ROM banking ------------ */

void thomson_state::to8_update_floppy_bank()
{
	int bank = (m_to8_reg_sys1 & 0x80) ? to7_floppy_bank : (m_to8_bios_bank + TO7_NB_FLOP_BANK);

	if ( bank != m_old_floppy_bank )
	{
		LOG_BANK(( "to8_update_floppy_bank: floppy ROM is %s bank %i\n",
							(m_to8_reg_sys1 & 0x80) ? "external" : "internal",
							bank % TO7_NB_FLOP_BANK ));
		membank( THOM_FLOP_BANK )->set_entry( bank );
		m_old_floppy_bank = bank;
	}
}



void thomson_state::to8_update_floppy_bank_postload()
{
	to8_update_floppy_bank();
}



void thomson_state::to8_update_ram_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	UINT8 bank = 0;

	if ( m_to8_reg_sys1 & 0x10 )
	{
		bank = m_to8_reg_ram & 31;
	}
	else
	{
		UINT8 portb = m_pia_sys->port_b_z_mask();

		switch ( portb & 0xf8 )
		{
			/*  in compatibility mode, banks 5 and 6 are reversed wrt TO7/70 */
		case 0xf0: bank = 2; break;
		case 0xe8: bank = 3; break;
		case 0x18: bank = 4; break;
		case 0x58: bank = 5; break;
		case 0x98: bank = 6; break;
		case 0xd8: bank = 7; break;
		case 0xf8: return;
		default:
			logerror( "to8_update_ram_bank: unknown RAM bank=$%02X\n", portb & 0xf8 );
			return;
		}
	}

	/*  due to addressing distortion, the 16 KB banked memory space is
	    split into two 8 KB spaces:
	    - 0xa000-0xbfff maps to 0x2000-0x3fff in 16 KB bank
	    - 0xc000-0xdfff maps to 0x0000-0x1fff in 16 KB bank
	    this is important if we map a bank that is also reachable by another,
	    undistorted space, such as cartridge, page 0 (video), or page 1
	*/
	if ( bank != m_old_ram_bank)
	{
		if (m_ram->size() == 512*1024 || m_to8_data_vpage < 16)
		{
			membank( TO8_DATA_LO )->set_entry( bank );
			membank( TO8_DATA_HI )->set_entry( bank );
		}
		else
		{
			/* RAM size is 256 KB only and unavailable
			 * bank requested */
			space.nop_readwrite( 0xa000, 0xbfff);
			space.nop_readwrite( 0xc000, 0xdfff);
		}
		m_to8_data_vpage = bank;
		m_old_ram_bank = bank;
		LOG_BANK(( "to8_update_ram_bank: select bank %i (%s style)\n", bank, (m_to8_reg_sys1 & 0x10) ? "new" : "old"));
	}
}



void thomson_state::to8_update_ram_bank_postload()
{
	to8_update_ram_bank();
}



void thomson_state::to8_update_cart_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	int bank = 0;
	int bank_is_read_only = 0;

	if ( m_to8_reg_cart & 0x20 )
	{
		/* RAM space */
		m_to8_cart_vpage = m_to8_reg_cart & 31;
		bank = 8 + m_to8_cart_vpage;
		bank_is_read_only = (( m_to8_reg_cart & 0x40 ) == 0);
		if ( bank != m_old_cart_bank )
		{
			/* mapping to VRAM */
			if (m_ram->size() == 512*1024 || m_to8_cart_vpage < 16)
			{
				if (m_to8_cart_vpage < 4)
				{
					if (m_old_cart_bank < 8 || m_old_cart_bank > 11)
					{
						space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
						if ( bank_is_read_only )
						{
							space.nop_write( 0x0000, 0x3fff);
						}
						else
						{
							space.install_write_handler( 0x0000, 0x3fff, write8_delegate(FUNC(thomson_state::to8_vcart_w),this));
						}
					}
				}
				else
				{
					if (m_old_cart_bank < 12)
					{
						if ( bank_is_read_only )
						{
							space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
							space.nop_write( 0x0000, 0x3fff);
						}
						else
						{
							space.install_readwrite_bank( 0x0000, 0x3fff,THOM_CART_BANK);
						}
					}
				}
			}
			else
			{
				/* RAM size is 256 KB only and unavailable
				 * bank requested */
				space.nop_readwrite( 0x0000, 0x3fff);
			}
			LOG_BANK(( "to8_update_cart_bank: CART is RAM bank %i (%s)\n",
									m_to8_cart_vpage,
									bank_is_read_only ? "read-only":"read-write"));
		}
		else
		{
			if ( bank_is_read_only != m_old_cart_bank_was_read_only )
			{
				if ( bank_is_read_only )
				{
					space.nop_write( 0x0000, 0x3fff);
				}
				else
				{
					if (m_to8_cart_vpage < 4)
					{
						space.install_write_handler( 0x0000, 0x3fff, write8_delegate(FUNC(thomson_state::to8_vcart_w),this));
					}
					else
					{
						space.install_readwrite_bank( 0x0000, 0x3fff, THOM_CART_BANK );
					}
				}
				LOG_BANK(( "to8_update_cart_bank: update CART bank %i write status to %s\n",
											m_to8_cart_vpage,
											bank_is_read_only ? "read-only":"read-write"));
			}
		}
		m_old_cart_bank_was_read_only = bank_is_read_only;
	}
	else
	{
		if ( m_to8_soft_select )
		{
			/* internal software ROM space */
			bank = 4 + m_to8_soft_bank;
			if ( bank != m_old_cart_bank )
			{
				if ( m_old_cart_bank < 4 || m_old_cart_bank > 7 )
				{
					space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
					space.install_write_handler( 0x0000, 0x3fff, write8_delegate(FUNC(thomson_state::to8_cartridge_w),this) );
				}
				LOG_BANK(( "to8_update_cart_bank: CART is internal bank %i\n", m_to8_soft_bank ));
			}
		}
		else
		{
			/* external cartridge ROM space */
			if ( m_thom_cart_nb_banks )
			{
				bank = m_thom_cart_bank % m_thom_cart_nb_banks;
				if ( bank != m_old_cart_bank )
				{
					if ( m_old_cart_bank < 0 || m_old_cart_bank > 3 )
					{
						space.install_read_bank( 0x0000, 0x3fff, THOM_CART_BANK );
						space.install_write_handler( 0x0000, 0x3fff, write8_delegate(FUNC(thomson_state::to8_cartridge_w),this) );
						space.install_read_handler( 0x0000, 0x0003, read8_delegate(FUNC(thomson_state::to8_cartridge_r),this) );
					}
					LOG_BANK(( "to8_update_cart_bank: CART is external cartridge bank %i\n", bank ));
				}
			}
			else
			{
				if ( m_old_cart_bank != 0 )
				{
					space.nop_read( 0x0000, 0x3fff);
					LOG_BANK(( "to8_update_cart_bank: CART is unmapped\n"));
				}
			}
		}
	}
	if ( bank != m_old_cart_bank )
	{
		membank( THOM_CART_BANK )->set_entry( bank );
		m_old_cart_bank = bank;
	}
}



void thomson_state::to8_update_cart_bank_postload()
{
	to8_update_cart_bank();
}



/* ROM bank switch */
WRITE8_MEMBER( thomson_state::to8_cartridge_w )
{
	if ( offset >= 0x2000 )
		return;

	if ( m_to8_soft_select )
		m_to8_soft_bank = offset & 3;
	else
		m_thom_cart_bank = offset & 3;

	to8_update_cart_bank();
}



/* read signal to 0000-0003 generates a bank switch */
READ8_MEMBER( thomson_state::to8_cartridge_r )
{
	UINT8* pos = memregion( "maincpu" )->base() + 0x10000;
	UINT8 data = pos[offset + (m_thom_cart_bank % m_thom_cart_nb_banks) * 0x4000];
	if ( !space.debugger_access() )
	{
		m_thom_cart_bank = offset & 3;
		to8_update_cart_bank();
	}
	return data;
}


/* ------------ floppy / network controller dispatch ------------ */



void thomson_state::to8_floppy_init()
{
	UINT8* mem = memregion("maincpu")->base();
	to7_floppy_init( mem + 0x34000 );
}



void thomson_state::to8_floppy_reset()
{
	UINT8* mem = memregion("maincpu")->base();
	to7_floppy_reset();
	if ( THOM_FLOPPY_INT )
		thmfc_floppy_reset();
	membank( THOM_FLOP_BANK )->configure_entries( TO7_NB_FLOP_BANK, 2, mem + 0x30000, 0x2000 );
}



READ8_MEMBER( thomson_state::to8_floppy_r )
{
	if ( space.debugger_access() )
		return 0;

	if ( (m_to8_reg_sys1 & 0x80) && THOM_FLOPPY_EXT )
		/* external controller */
		return to7_floppy_r( space, offset );
	else if ( ! (m_to8_reg_sys1 & 0x80) && THOM_FLOPPY_INT )
		/* internal controller */
		return thmfc_floppy_r( space, offset );
	else
		/* no controller */
		return 0;
}



WRITE8_MEMBER( thomson_state::to8_floppy_w )
{
	if ( (m_to8_reg_sys1 & 0x80) && THOM_FLOPPY_EXT )
		/* external controller */
		to7_floppy_w( space, offset, data );
	else if ( ! (m_to8_reg_sys1 & 0x80) && THOM_FLOPPY_INT )
		/* internal controller */
		thmfc_floppy_w( space, offset, data );
}



/* ------------ system gate-array ------------ */



#define TO8_LIGHTPEN_DECAL 16



READ8_MEMBER( thomson_state::to8_gatearray_r )
{
	struct thom_vsignal v = thom_get_vsignal();
	struct thom_vsignal l = thom_get_lightpen_vsignal( TO8_LIGHTPEN_DECAL, m_to7_lightpen_step - 1, 6 );
	int count, inil, init, lt3;
	UINT8 res;
	count = m_to7_lightpen ? l.count : v.count;
	inil  = m_to7_lightpen ? l.inil  : v.inil;
	init  = m_to7_lightpen ? l.init  : v.init;
	lt3   = m_to7_lightpen ? l.lt3   : v.lt3;

	switch ( offset )
	{
	case 0: /* system 2 / lightpen register 1 */
		if ( m_to7_lightpen )
			res = (count >> 8) & 0xff;
		else
			res = m_to8_reg_sys2 & 0xf0;
		break;

	case 1: /* ram register / lightpen register 2 */
		if ( m_to7_lightpen )
		{
			if ( !space.debugger_access() )
			{
				thom_firq_2( 0 );
				m_to8_lightpen_intr = 0;
			}
			res = count & 0xff;
		}
		else
			res = m_to8_reg_ram & 0x1f;
		break;

	case 2: /* cartrige register / lightpen register 3 */
		if ( m_to7_lightpen )
			res = (lt3 << 7) | (inil << 6);
		else
			res = m_to8_reg_cart;
		break;

	case 3: /* lightpen register 4 */
		res = (v.init << 7) | (init << 6) | (v.inil << 5) | (m_to8_lightpen_intr << 1) | m_to7_lightpen;
		break;

	default:
		logerror( "$%04x to8_gatearray_r: invalid offset %i\n", m_maincpu->pc(), offset );
		res = 0;
	}

	LOG_VIDEO(( "$%04x %f to8_gatearray_r: off=%i ($%04X) res=$%02X lightpen=%i\n",
			m_maincpu->pc(), machine().time().as_double(),
			offset, 0xe7e4 + offset, res, m_to7_lightpen ));

	return res;
}



WRITE8_MEMBER( thomson_state::to8_gatearray_w )
{
	LOG_VIDEO(( "$%04x %f to8_gatearray_w: off=%i ($%04X) data=$%02X\n",
			m_maincpu->pc(), machine().time().as_double(),
			offset, 0xe7e4 + offset, data ));

	switch ( offset )
	{
	case 0: /* switch */
		m_to7_lightpen = data & 1;
		break;

	case 1: /* ram register */
		if ( m_to8_reg_sys1 & 0x10 )
		{
			m_to8_reg_ram = data;
			to8_update_ram_bank();
		}
		break;

	case 2: /* cartridge register */
		m_to8_reg_cart = data;
		to8_update_cart_bank();
		break;

	case 3: /* system register 1 */
		m_to8_reg_sys1 = data;
		to8_update_floppy_bank();
		to8_update_ram_bank();
		to8_update_cart_bank();
		break;

	default:
		logerror( "$%04x to8_gatearray_w: invalid offset %i (data=$%02X)\n",
				m_maincpu->pc(), offset, data );
	}
}



/* ------------ video gate-array ------------ */



READ8_MEMBER( thomson_state::to8_vreg_r )
{
	/* 0xe7dc from external floppy drive aliases the video gate-array */
	if ( ( offset == 3 ) && ( m_to8_reg_ram & 0x80 ) && ( m_to8_reg_sys1 & 0x80 ) )
	{
		if ( space.debugger_access() )
			return 0;

		if ( THOM_FLOPPY_EXT )
			return to7_floppy_r( space, 0xc );
		else
			return 0;
	}

	switch ( offset )
	{
	case 0: /* palette data */
	{
		UINT8 c =  m_to9_palette_data[ m_to9_palette_idx ];
		if ( !space.debugger_access() )
		{
			m_to9_palette_idx = ( m_to9_palette_idx + 1 ) & 31;
		}
		return c;
	}

	case 1: /* palette address */
		return m_to9_palette_idx;

	case 2:
	case 3:
		return 0;

	default:
		logerror( "to8_vreg_r: invalid read offset %i\n", offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::to8_vreg_w )
{
	LOG_VIDEO(( "$%04x %f to8_vreg_w: off=%i ($%04X) data=$%02X\n",
			m_maincpu->pc(), machine().time().as_double(),
			offset, 0xe7da + offset, data ));

	switch ( offset )
	{
	case 0: /* palette data */
	{
		UINT16 color, idx;
		m_to9_palette_data[ m_to9_palette_idx ] = data;
		idx = m_to9_palette_idx / 2;
		color = m_to9_palette_data[ 2 * idx + 1 ];
		color = m_to9_palette_data[ 2 * idx ] | (color << 8);
		thom_set_palette( idx, color & 0x1fff );
		m_to9_palette_idx = ( m_to9_palette_idx + 1 ) & 31;
	}
	break;

	case 1: /* palette address */
		m_to9_palette_idx = data & 31;
		break;

	case 2: /* display register */
		to9_set_video_mode( data, 1 );
		break;

	case 3: /* system register 2 */
		/* 0xe7dc from external floppy drive aliases the video gate-array */
		if ( ( offset == 3 ) && ( m_to8_reg_ram & 0x80 ) && ( m_to8_reg_sys1 & 0x80 ) )
		{
			if ( THOM_FLOPPY_EXT )
				to7_floppy_w( space, 0xc, data );
		}
		else
		{
			m_to8_reg_sys2 = data;
			thom_set_video_page( data >> 6 );
			thom_set_border_color( data & 15 );
		}
		break;

	default:
		logerror( "to8_vreg_w: invalid write offset %i data=$%02X\n", offset, data );
	}
}



/* ------------ system PIA 6821 ------------ */



READ8_MEMBER( thomson_state::to8_sys_porta_in )
{
	int ktest = to8_kbd_ktest();

	LOG_KBD(( "$%04x %f: to8_sys_porta_in ktest=%i\n", m_maincpu->pc(), machine().time().as_double(), ktest ));

	return ktest;
}



WRITE8_MEMBER( thomson_state::to8_sys_portb_out )
{
	m_centronics->d0_w(BIT(data, 0));
	m_centronics->strobe_w(BIT(data, 1));

	to8_update_ram_bank();

	if ( data & 4 ) /* bit 2: video overlay (TODO) */
		LOG(( "to8_sys_portb_out: video overlay not handled\n" ));
}



/* ------------ 6846 (timer, I/O) ------------ */



READ8_MEMBER( thomson_state::to8_timer_port_in )
{
	int lightpen = (ioport("lightpen_button")->read() & 1) ? 2 : 0;
	int cass = to7_get_cassette() ? 0x80 : 0;
	int dtr = m_centronics->busy_r() << 6;
	int lock = m_to8_kbd_caps ? 0 : 8; /* undocumented! */
	return lightpen | cass | dtr | lock;
}



WRITE8_MEMBER( thomson_state::to8_timer_port_out )
{
	int ack = (data & 0x20) ? 1 : 0;       /* bit 5: keyboard ACK */
	m_to8_bios_bank = (data & 0x10) ? 1 : 0; /* bit 4: BIOS bank*/
	thom_set_mode_point( data & 1 );       /* bit 0: video bank switch */
	membank( TO8_BIOS_BANK )->set_entry( m_to8_bios_bank );
	m_to8_soft_select = (data & 0x04) ? 1 : 0; /* bit 2: internal ROM select */
	to8_update_floppy_bank();
	to8_update_cart_bank();
	to8_kbd_set_ack(ack);
}



WRITE8_MEMBER( thomson_state::to8_timer_cp2_out )
{
	/* mute */
	m_to7_game_mute = data;
	to7_game_sound_update();
}



const mc6846_interface to8_timer =
{
	DEVCB_DRIVER_MEMBER(thomson_state, to8_timer_port_out),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(thomson_state, to8_timer_cp2_out),
	DEVCB_DRIVER_MEMBER(thomson_state, to8_timer_port_in),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_tco_out),
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, thom_dev_irq_0)
};



/* ------------ lightpen ------------ */



/* direct connection to interrupt line instead of through a PIA */
void thomson_state::to8_lightpen_cb( int step )
{
	if ( ! m_to7_lightpen )
		return;

	thom_firq_2( 1 );
	m_to7_lightpen_step = step;
	m_to8_lightpen_intr = 1;
}



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, to8 )
{
	LOG (( "to8: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	m_pia_sys->set_port_a_z_mask( 0xfe );
	to7_game_reset();
	to8_floppy_reset();
	to8_kbd_reset();
	to7_modem_reset();
	to7_midi_reset();

	/* gate-array */
	m_to7_lightpen = 0;
	m_to8_reg_ram = 0;
	m_to8_reg_cart = 0;
	m_to8_reg_sys1 = 0;
	m_to8_reg_sys2 = 0;
	m_to8_lightpen_intr = 0;
	m_to8_soft_select = 0;

	/* video */
	thom_set_video_mode( THOM_VMODE_TO770 );
	m_thom_lightpen_cb = &thomson_state::to8_lightpen_cb;
	thom_set_lightpen_callback( 4 );
	thom_set_border_color( 0 );
	thom_set_mode_point( 0 );
	m_pia_sys->cb1_w( 0 );

	/* memory */
	m_old_ram_bank = -1;
	m_old_cart_bank = -1;
	m_old_cart_bank_was_read_only = 0;
	m_old_floppy_bank = -1;
	m_to8_cart_vpage = 0;
	m_to8_data_vpage = 0;
	m_to8_soft_bank = 0;
	m_to8_bios_bank = 0;
	to8_update_ram_bank();
	to8_update_cart_bank();
	to8_update_floppy_bank();
	membank( TO8_BIOS_BANK )->set_entry( 0 );
	/* thom_cart_bank not reset */
}



MACHINE_START_MEMBER( thomson_state, to8 )
{
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "to8: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	to7_game_init();
	to8_floppy_init();
	to8_kbd_init();
	to9_palette_init();
	to7_modem_init();
	to7_midi_init();

	/* memory */
	m_thom_cart_bank = 0;
	m_thom_vram = ram;
	membank( THOM_CART_BANK )->configure_entries( 0,  8, mem + 0x10000, 0x4000 );
	if ( m_ram->size() == 256*1024 )
	{
		membank( THOM_CART_BANK )->configure_entries( 8,    16, ram, 0x4000 );
		membank( THOM_CART_BANK )->configure_entries( 8+16, 16, ram, 0x4000 );
		membank( TO8_DATA_LO )->configure_entries( 0, 16, ram + 0x2000, 0x4000 );
		membank( TO8_DATA_LO )->configure_entries( 16, 16, ram + 0x2000, 0x4000 );
		membank( TO8_DATA_HI )->configure_entries( 0, 16, ram + 0x0000, 0x4000 );
		membank( TO8_DATA_HI )->configure_entries( 16, 16, ram + 0x0000, 0x4000 );
	}
	else
	{
		membank( THOM_CART_BANK )->configure_entries( 8, 32, ram, 0x4000 );
		membank( TO8_DATA_LO )->configure_entries( 0, 32, ram + 0x2000, 0x4000 );
		membank( TO8_DATA_HI )->configure_entries( 0, 32, ram + 0x0000, 0x4000 );
	}
	membank( THOM_VRAM_BANK )->configure_entries( 0,  2, ram, 0x2000 );
	membank( TO8_SYS_LO )->configure_entry( 0,  ram + 0x6000);
	membank( TO8_SYS_HI )->configure_entry( 0,  ram + 0x4000);
	membank( TO8_BIOS_BANK )->configure_entries( 0,  2, mem + 0x30800, 0x2000 );
	membank( THOM_CART_BANK )->set_entry( 0 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );
	membank( TO8_SYS_LO )->set_entry( 0 );
	membank( TO8_SYS_HI )->set_entry( 0 );
	membank( TO8_DATA_LO )->set_entry( 0 );
	membank( TO8_DATA_HI )->set_entry( 0 );
	membank( TO8_BIOS_BANK )->set_entry( 0 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_item(NAME(m_to8_reg_ram));
	save_item(NAME(m_to8_reg_cart));
	save_item(NAME(m_to8_reg_sys1));
	save_item(NAME(m_to8_reg_sys2));
	save_item(NAME(m_to8_soft_select));
	save_item(NAME(m_to8_soft_bank));
	save_item(NAME(m_to8_bios_bank));
	save_item(NAME(m_to8_lightpen_intr));
	save_item(NAME(m_to8_data_vpage));
	save_item(NAME(m_to8_cart_vpage));
	save_pointer(NAME(mem + 0x10000), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to8_update_ram_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to8_update_cart_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to8_update_floppy_bank_postload), this));
}



/***************************** TO9+ *************************/



/* ------------ system PIA 6821 ------------ */



/* ------------ 6846 (timer, I/O) ------------ */



READ8_MEMBER( thomson_state::to9p_timer_port_in )
{
	int lightpen = (ioport("lightpen_button")->read() & 1) ? 2 : 0;
	int cass = to7_get_cassette() ? 0x80 : 0;
	int dtr = m_centronics->busy_r() << 6;
	return lightpen | cass | dtr;
}



WRITE8_MEMBER( thomson_state::to9p_timer_port_out )
{
	int bios_bank = (data & 0x10) ? 1 : 0; /* bit 4: BIOS bank */
	thom_set_mode_point( data & 1 );       /* bit 0: video bank switch */
	membank( TO8_BIOS_BANK )->set_entry( bios_bank );
	m_to8_soft_select = (data & 0x04) ? 1 : 0; /* bit 2: internal ROM select */
	to8_update_floppy_bank();
	to8_update_cart_bank();
}



const mc6846_interface to9p_timer =
{
	DEVCB_DRIVER_MEMBER(thomson_state, to9p_timer_port_out),
	DEVCB_NULL,
	DEVCB_DRIVER_MEMBER(thomson_state, to8_timer_cp2_out),
	DEVCB_DRIVER_MEMBER(thomson_state, to9p_timer_port_in),
	DEVCB_DRIVER_MEMBER(thomson_state, to7_timer_tco_out),
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, thom_dev_irq_0)
};



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, to9p )
{
	LOG (( "to9p: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	m_pia_sys->set_port_a_z_mask( 0xfe );
	to7_game_reset();
	to8_floppy_reset();
	to9_kbd_reset();
	to7_modem_reset();
	to7_midi_reset();

	/* gate-array */
	m_to7_lightpen = 0;
	m_to8_reg_ram = 0;
	m_to8_reg_cart = 0;
	m_to8_reg_sys1 = 0;
	m_to8_reg_sys2 = 0;
	m_to8_lightpen_intr = 0;
	m_to8_soft_select = 0;

	/* video */
	thom_set_video_mode( THOM_VMODE_TO770 );
	m_thom_lightpen_cb = &thomson_state::to8_lightpen_cb;
	thom_set_lightpen_callback( 4 );
	thom_set_border_color( 0 );
	thom_set_mode_point( 0 );
	m_pia_sys->cb1_w( 0 );

	/* memory */
	m_old_ram_bank = -1;
	m_old_cart_bank = -1;
	m_old_floppy_bank = -1;
	m_to8_cart_vpage = 0;
	m_to8_data_vpage = 0;
	m_to8_soft_bank = 0;
	m_to8_bios_bank = 0;
	to8_update_ram_bank();
	to8_update_cart_bank();
	to8_update_floppy_bank();
	membank( TO8_BIOS_BANK )->set_entry( 0 );
	/* thom_cart_bank not reset */
}



MACHINE_START_MEMBER( thomson_state, to9p )
{
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "to9p: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	to7_game_init();
	to8_floppy_init();
	to9_kbd_init();
	to9_palette_init();
	to7_modem_init();
	to7_midi_init();

	/* memory */
	m_thom_cart_bank = 0;
	m_thom_vram = ram;
	membank( THOM_CART_BANK )->configure_entries( 0,  8, mem + 0x10000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 8, 32, ram, 0x4000 );
	membank( THOM_VRAM_BANK )->configure_entries( 0,  2, ram, 0x2000 );
	membank( TO8_SYS_LO )->configure_entry( 0,  ram + 0x6000);
	membank( TO8_SYS_HI )->configure_entry( 0,  ram + 0x4000);
	membank( TO8_DATA_LO )->configure_entries( 0, 32, ram + 0x2000, 0x4000 );
	membank( TO8_DATA_HI )->configure_entries( 0, 32, ram + 0x0000, 0x4000 );
	membank( TO8_BIOS_BANK )->configure_entries( 0,  2, mem + 0x30800, 0x2000 );
	membank( THOM_CART_BANK )->set_entry( 0 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );
	membank( TO8_SYS_LO )->set_entry( 0 );
	membank( TO8_SYS_HI )->set_entry( 0 );
	membank( TO8_DATA_LO )->set_entry( 0 );
	membank( TO8_DATA_HI )->set_entry( 0 );
	membank( TO8_BIOS_BANK )->set_entry( 0 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_item(NAME(m_to8_reg_ram));
	save_item(NAME(m_to8_reg_cart));
	save_item(NAME(m_to8_reg_sys1));
	save_item(NAME(m_to8_reg_sys2));
	save_item(NAME(m_to8_soft_select));
	save_item(NAME(m_to8_soft_bank));
	save_item(NAME(m_to8_bios_bank));
	save_item(NAME(m_to8_lightpen_intr));
	save_item(NAME(m_to8_data_vpage));
	save_item(NAME(m_to8_cart_vpage));
	save_pointer(NAME(mem + 0x10000), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to8_update_ram_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to8_update_cart_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::to8_update_floppy_bank_postload), this));
}



/****************** MO6 / Olivetti Prodest PC 128 *******************/



/* ------------ RAM / ROM banking ------------ */



void thomson_state::mo6_update_ram_bank()
{
	UINT8 bank = 0;

	if ( m_to8_reg_sys1 & 0x10 )
	{
		bank = m_to8_reg_ram & 7; /* 128 KB RAM only = 8 pages */
	}
	if ( bank != m_to8_data_vpage ) {
		membank( TO8_DATA_LO )->set_entry( bank );
		membank( TO8_DATA_HI )->set_entry( bank );
		m_to8_data_vpage = bank;
		m_old_ram_bank = bank;
		LOG_BANK(( "mo6_update_ram_bank: select bank %i (new style)\n", bank ));
	}
}



void thomson_state::mo6_update_ram_bank_postload()
{
	mo6_update_ram_bank();
}



void thomson_state::mo6_update_cart_bank()
{
	address_space& space = m_maincpu->space(AS_PROGRAM);
	int b = (m_pia_sys->a_output() >> 5) & 1;
	int bank = 0;
	int bank_is_read_only = 0;

	// space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK );

	if ( ( ( m_to8_reg_sys1 & 0x40 ) && ( m_to8_reg_cart & 0x20 ) ) || ( ! ( m_to8_reg_sys1 & 0x40 ) && ( m_mo5_reg_cart & 4 ) ) )
	{
		/* RAM space */
		if ( m_to8_reg_sys1 & 0x40 )
		{
			/* use a7e6 */
			m_to8_cart_vpage = m_to8_reg_cart & 7; /* 128 KB RAM only = 8 pages */
			bank = 8 + m_to8_cart_vpage;
			bank_is_read_only = (( m_to8_reg_cart & 0x40 ) == 0);
			if ( bank != m_old_cart_bank )
			{
				if (m_to8_cart_vpage < 4)
				{
					if (m_old_cart_bank < 8 || m_old_cart_bank > 11)
					{
						space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK );
						if ( bank_is_read_only )
						{
							space.nop_write( 0xb000, 0xefff);
						}
						else
						{
							space.install_write_handler( 0xb000, 0xefff, write8_delegate(FUNC(thomson_state::to8_vcart_w),this));
						}
					}
				}
				else
				{
					if (m_old_cart_bank < 12)
					{
						if ( bank_is_read_only )
						{
							space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK );
							space.nop_write( 0xb000, 0xefff);
						}
						else
						{
							space.install_readwrite_bank( 0xb000, 0xefff,THOM_CART_BANK);
						}
					}
				}
				LOG_BANK(( "mo6_update_cart_bank: CART is RAM bank %i (%s)\n",
					m_to8_cart_vpage,
					bank_is_read_only ? "read-only":"read-write"));
			}
			else if ( bank_is_read_only != m_old_cart_bank_was_read_only )
			{
				if ( bank_is_read_only )
				{
					space.nop_write( 0xb000, 0xefff);
				}
				else
				{
					if (m_to8_cart_vpage < 4)
					{
						space.install_write_handler( 0xb000, 0xefff, write8_delegate(FUNC(thomson_state::to8_vcart_w),this));
					}
					else
					{
						space.install_readwrite_bank( 0xb000, 0xefff, THOM_CART_BANK );
					}
				}
				LOG_BANK(( "mo6_update_cart_bank: update CART bank %i write status to %s\n",
											m_to8_cart_vpage,
											bank_is_read_only ? "read-only":"read-write"));
			}
			m_old_cart_bank_was_read_only = bank_is_read_only;
		}
		else if ( m_thom_cart_nb_banks == 4 )
		{
			/* "JANE"-style cartridge bank switching */
			bank = m_mo5_reg_cart & 3;
			if ( bank != m_old_cart_bank )
			{
				if ( m_old_cart_bank < 0 || m_old_cart_bank > 3 )
				{
					space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK );
					space.nop_write( 0xb000, 0xefff);
				}
				LOG_BANK(( "mo6_update_cart_bank: CART is external cartridge bank %i (A7CB style)\n", bank ));
			}
		}
		else
		{
			/* RAM from MO5 network extension */
			int bank_is_read_only = (( m_mo5_reg_cart & 8 ) == 0);
			m_to8_cart_vpage = (m_mo5_reg_cart & 3) | 4;
			bank = 8 + m_to8_cart_vpage;
			if ( bank != m_old_cart_bank )
			{
				if ( m_old_cart_bank < 12 )
				{
					if ( bank_is_read_only )
					{
						space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK);
						space.nop_write( 0xb000, 0xefff);
					}
					else
					{
						space.install_readwrite_bank( 0xb000, 0xefff, THOM_CART_BANK);
					}
				}
				LOG_BANK(( "mo6_update_cart_bank: CART is RAM bank %i (MO5 compat.) (%s)\n",
											m_to8_cart_vpage,
											bank_is_read_only ? "read-only":"read-write"));
			}
			else if ( bank_is_read_only != m_old_cart_bank_was_read_only )
			{
				if ( bank_is_read_only )
				{
					space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK);
					space.nop_write( 0xb000, 0xefff);
				}
				else
				{
					space.install_readwrite_bank( 0xb000, 0xefff, THOM_CART_BANK);
				}
				LOG_BANK(( "mo5_update_cart_bank: update CART bank %i write status to %s\n",
											m_to8_cart_vpage,
											bank_is_read_only ? "read-only":"read-write"));
			}
			m_old_cart_bank_was_read_only = bank_is_read_only;
		}
	}
	else
	{
		/* ROM space */
		if ( m_to8_reg_sys2 & 0x20 )
		{
			/* internal ROM */
			if ( m_to8_reg_sys2 & 0x10)
			{
				bank = b + 6; /* BASIC 128 */
			}
			else
			{
				bank = b + 4;                      /* BASIC 1 */
			}
			if ( bank != m_old_cart_bank )
			{
				if ( m_old_cart_bank < 4 || m_old_cart_bank > 7 )
				{
					space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK);
					space.install_write_handler( 0xb000, 0xefff, write8_delegate(FUNC(thomson_state::mo6_cartridge_w),this) );
				}
				LOG_BANK(( "mo6_update_cart_bank: CART is internal ROM bank %i\n", b ));
			}
		}
		else
		{
			/* cartridge */
			if ( m_thom_cart_nb_banks )
			{
				bank = m_thom_cart_bank % m_thom_cart_nb_banks;
				if ( bank != m_old_cart_bank )
				{
					if ( m_old_cart_bank < 0 || m_old_cart_bank > 3 )
					{
						space.install_read_bank( 0xb000, 0xefff, THOM_CART_BANK );
						space.install_write_handler( 0xb000, 0xefff, write8_delegate(FUNC(thomson_state::mo6_cartridge_w),this) );
						space.install_read_handler( 0xbffc, 0xbfff, read8_delegate(FUNC(thomson_state::mo6_cartridge_r),this) );
					}
					LOG_BANK(( "mo6_update_cart_bank: CART is external cartridge bank %i\n", bank ));
				}
			}
			else
			{
				if ( m_old_cart_bank != 0 )
				{
					space.nop_read( 0xb000, 0xefff );
					LOG_BANK(( "mo6_update_cart_bank: CART is unmapped\n"));
				}
			}
		}
	}
	if ( bank != m_old_cart_bank )
		{
		membank( THOM_CART_BANK )->set_entry( bank );
		membank( TO8_BIOS_BANK )->set_entry( b );
		m_old_cart_bank = bank;
	}
}



void thomson_state::mo6_update_cart_bank_postload()
{
	mo6_update_cart_bank();
}



/* write signal generates a bank switch */
WRITE8_MEMBER( thomson_state::mo6_cartridge_w )
{
	if ( offset >= 0x2000 )
		return;

	m_thom_cart_bank = offset & 3;
	mo6_update_cart_bank();
}



/* read signal generates a bank switch */
READ8_MEMBER( thomson_state::mo6_cartridge_r )
{
	UINT8* pos = memregion( "maincpu" )->base() + 0x10000;
	UINT8 data = pos[offset + 0xbffc + (m_thom_cart_bank % m_thom_cart_nb_banks) * 0x4000];
	if ( !space.debugger_access() )
	{
		m_thom_cart_bank = offset & 3;
		mo6_update_cart_bank();
	}
	return data;
}



WRITE8_MEMBER( thomson_state::mo6_ext_w )
{
	/* MO5 network extension compatible */
	m_mo5_reg_cart = data;
	mo6_update_cart_bank();
}



/* ------------ game 6821 PIA ------------ */

/* similar to SX 90-018, but with a few differences: mute, printer */


WRITE_LINE_MEMBER( thomson_state::mo6_centronics_busy )
{
	m_pia_game->cb1_w(state);
}


const centronics_interface mo6_centronics_config =
{
	DEVCB_NULL,
	DEVCB_DRIVER_LINE_MEMBER(thomson_state, mo6_centronics_busy),
	DEVCB_NULL
};


WRITE8_MEMBER( thomson_state::mo6_game_porta_out )
{
	LOG (( "$%04x %f mo6_game_porta_out: CENTRONICS set data=$%02X\n", m_maincpu->pc(), machine().time().as_double(), data ));

	/* centronics data */
	m_centronics->write( space, 0, data);
}



WRITE_LINE_MEMBER( thomson_state::mo6_game_cb2_out )
{
	LOG (( "$%04x %f mo6_game_cb2_out: CENTRONICS set strobe=%i\n", m_maincpu->pc(), machine().time().as_double(), state ));

	/* centronics strobe */
	m_centronics->strobe_w(state);
}



TIMER_CALLBACK_MEMBER(thomson_state::mo6_game_update_cb)
{
	/* unlike the TO8, CB1 & CB2 are not connected to buttons */
	if ( ioport("config")->read() & 1 )
	{
		UINT8 mouse = to7_get_mouse_signal();
		m_pia_game->ca1_w( BIT(mouse, 0) ); /* XA */
		m_pia_game->ca2_w( BIT(mouse, 1) ); /* YA */
	}
	else
	{
		/* joystick */
		UINT8 in = ioport("game_port_buttons")->read();
		m_pia_game->ca1_w( BIT(in, 2) ); /* P1 action B */
		m_pia_game->ca2_w( BIT(in, 6) ); /* P1 action A */
	}
}



void thomson_state::mo6_game_init()
{
	LOG (( "mo6_game_init called\n" ));
	m_to7_game_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::mo6_game_update_cb),this));
	m_to7_game_timer->adjust(TO7_GAME_POLL_PERIOD, 0, TO7_GAME_POLL_PERIOD);
	save_item(NAME(m_to7_game_sound));
	save_item(NAME(m_to7_game_mute));
}



void thomson_state::mo6_game_reset()
{
	LOG (( "mo6_game_reset called\n" ));
	m_pia_game->ca1_w( 0 );
	m_to7_game_sound = 0;
	m_to7_game_mute = 0;
	to7_game_sound_update();
}



/* ------------ system PIA 6821 ------------ */



READ8_MEMBER( thomson_state::mo6_sys_porta_in )
{
	return
		(mo5_get_cassette() ? 0x80 : 0) |     /* bit 7: cassette input */
		8 |                                   /* bit 3: kbd-line float up to 1 */
		((ioport("lightpen_button")->read() & 1) ? 2 : 0);
	/* bit 1: lightpen button */;
}



READ8_MEMBER( thomson_state::mo6_sys_portb_in )
{
	/* keyboard: 9 lines of 8 keys */
	UINT8 porta = m_pia_sys->a_output();
	UINT8 portb = m_pia_sys->b_output();
	int col = (portb >> 4) & 7;    /* B bits 4-6: kbd column */
	int lin = (portb >> 1) & 7;    /* B bits 1-3: kbd line */
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3", "keyboard_4",
		"keyboard_5", "keyboard_6", "keyboard_7", "keyboard_8", "keyboard_9"
	};

	if ( ! (porta & 8) )
		lin = 8;     /* A bit 3: 9-th kbd line select */

	return
		( ioport(keynames[lin])->read() & (1 << col) ) ?  0x80 : 0;
	/* bit 7: key up */
}



WRITE8_MEMBER( thomson_state::mo6_sys_porta_out )
{
	thom_set_mode_point( data & 1 );                /* bit 0: video bank switch */
	m_to7_game_mute = data & 4;                       /* bit 2: sound mute */
	thom_set_caps_led( (data & 16) ? 0 : 1 ) ;      /* bit 4: keyboard led */
	mo5_set_cassette( (data & 0x40) ? 1 : 0 );     /* bit 6: cassette output */
	mo6_update_cart_bank();                  /* bit 5: rom bank */
	to7_game_sound_update();
}



WRITE8_MEMBER( thomson_state::mo6_sys_portb_out )
{
	m_buzzer->write_unsigned8((data & 1) ? 0x80 : 0); /* bit 0: buzzer */
}



WRITE_LINE_MEMBER( thomson_state::mo6_sys_cb2_out )
{
	/* SCART pin 8 = slow switch (?) */
	LOG(( "mo6_sys_cb2_out: SCART slow switch set to %i\n", state ));
}



/* ------------ system gate-array ------------ */

#define MO6_LIGHTPEN_DECAL 12



READ8_MEMBER( thomson_state::mo6_gatearray_r )
{
	struct thom_vsignal v = thom_get_vsignal();
	struct thom_vsignal l = thom_get_lightpen_vsignal( MO6_LIGHTPEN_DECAL, m_to7_lightpen_step - 1, 6 );
	int count, inil, init, lt3;
	UINT8 res;
	count = m_to7_lightpen ? l.count : v.count;
	inil  = m_to7_lightpen ? l.inil  : v.inil;
	init  = m_to7_lightpen ? l.init  : v.init;
	lt3   = m_to7_lightpen ? l.lt3   : v.lt3;

	switch ( offset )
	{
	case 0: /* system 2 / lightpen register 1 */
		if ( m_to7_lightpen )
			res = (count >> 8) & 0xff;
		else
			res = m_to8_reg_sys2 & 0xf0;
		break;

	case 1: /* ram register / lightpen register 2 */
		if ( m_to7_lightpen )
		{
			if ( !space.debugger_access() )
			{
				thom_firq_2( 0 );
				m_to8_lightpen_intr = 0;
			}
			res =  count & 0xff;
		}
		else
			res = m_to8_reg_ram & 0x1f;
		break;

	case 2: /* cartrige register / lightpen register 3 */
		if ( m_to7_lightpen )
			res = (lt3 << 7) | (inil << 6);
		else
			res = 0;
		break;

	case 3: /* lightpen register 4 */
		res = (v.init << 7) | (init << 6) | (v.inil << 5) | (m_to8_lightpen_intr << 1) | m_to7_lightpen;
		break;

	default:
		logerror( "$%04x mo6_gatearray_r: invalid offset %i\n", m_maincpu->pc(), offset );
		res = 0;
	}

	LOG_VIDEO(( "$%04x %f mo6_gatearray_r: off=%i ($%04X) res=$%02X lightpen=%i\n",
			m_maincpu->pc(), machine().time().as_double(),
			offset, 0xa7e4 + offset, res, m_to7_lightpen ));

	return res;
}



WRITE8_MEMBER( thomson_state::mo6_gatearray_w )
{
	LOG_VIDEO(( "$%04x %f mo6_gatearray_w: off=%i ($%04X) data=$%02X\n",
			m_maincpu->pc(), machine().time().as_double(),
			offset, 0xa7e4 + offset, data ));

	switch ( offset )
	{
	case 0: /* switch */
		m_to7_lightpen = data & 1;
		break;

	case 1: /* ram register */
		if ( m_to8_reg_sys1 & 0x10 )
		{
			m_to8_reg_ram = data;
			mo6_update_ram_bank();
		}
		break;

	case 2: /* cartridge register */
		m_to8_reg_cart = data;
		mo6_update_cart_bank();
		break;

	case 3: /* system register 1 */
		m_to8_reg_sys1 = data;
		mo6_update_ram_bank();
		mo6_update_cart_bank();
		break;

	default:
		logerror( "$%04x mo6_gatearray_w: invalid offset %i (data=$%02X)\n", m_maincpu->pc(), offset, data );
	}
}


/* ------------ video gate-array ------------ */



READ8_MEMBER( thomson_state::mo6_vreg_r )
{
	/* 0xa7dc from external floppy drive aliases the video gate-array */
	if ( ( offset == 3 ) && ( m_to8_reg_ram & 0x80 ) )
		{
		if ( !space.debugger_access() )
			return to7_floppy_r( space, 0xc );
		}

	switch ( offset )
	{
	case 0: /* palette data */
	case 1: /* palette address */
		return to8_vreg_r( space, offset );

	case 2:
	case 3:
		return 0;

	default:
		logerror( "mo6_vreg_r: invalid read offset %i\n", offset );
		return 0;
	}
}



WRITE8_MEMBER( thomson_state::mo6_vreg_w )
{
	LOG_VIDEO(( "$%04x %f mo6_vreg_w: off=%i ($%04X) data=$%02X\n",
			m_maincpu->pc(), machine().time().as_double(),
			offset, 0xa7da + offset, data ));

	switch ( offset )
	{
	case 0: /* palette data */
	case 1: /* palette address */
		to8_vreg_w( space, offset, data );
		return;

	case 2: /* display / external floppy register */
		if ( ( m_to8_reg_sys1 & 0x80 ) && ( m_to8_reg_ram & 0x80 ) )
			to7_floppy_w( space, 0xc, data );
		else
			to9_set_video_mode( data, 2 );
		break;

	case 3: /* system register 2 */
		/* 0xa7dc from external floppy drive aliases the video gate-array */
		if ( ( offset == 3 ) && ( m_to8_reg_ram & 0x80 ) )
			to7_floppy_w( space, 0xc, data );
		else
		{
			m_to8_reg_sys2 = data;
			thom_set_video_page( data >> 6 );
			thom_set_border_color( data & 15 );
			mo6_update_cart_bank();
		}
		break;

	default:
		logerror( "mo6_vreg_w: invalid write offset %i data=$%02X\n", offset, data );
	}
}



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, mo6 )
{
	LOG (( "mo6: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	m_pia_sys->set_port_a_z_mask( 0x75 );
	mo6_game_reset();
	to7_floppy_reset();
	to7_modem_reset();
	to7_midi_reset();
	mo5_init_timer();

	/* gate-array */
	m_to7_lightpen = 0;
	m_to8_reg_ram = 0;
	m_to8_reg_cart = 0;
	m_to8_reg_sys1 = 0;
	m_to8_reg_sys2 = 0;
	m_to8_lightpen_intr = 0;

	/* video */
	thom_set_video_mode( THOM_VMODE_MO5 );
	m_thom_lightpen_cb = &thomson_state::to8_lightpen_cb;
	thom_set_lightpen_callback( 3 );
	thom_set_border_color( 0 );
	thom_set_mode_point( 0 );
	m_pia_sys->ca1_w( 0 );

	/* memory */
	m_old_ram_bank = -1;
	m_old_cart_bank = -1;
	m_to8_cart_vpage = 0;
	m_to8_data_vpage = 0;
	mo6_update_ram_bank();
	mo6_update_cart_bank();
	/* mo5_reg_cart not reset */
	/* thom_cart_bank not reset */
}



MACHINE_START_MEMBER( thomson_state, mo6 )
{
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "mo6: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	mo6_game_init();
	to7_floppy_init( mem + 0x30000 );
	to9_palette_init();
	to7_modem_init();
	to7_midi_init();
	m_mo5_periodic_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::mo5_periodic_cb),this));

	/* memory */
	m_thom_cart_bank = 0;
	m_mo5_reg_cart = 0;
	m_thom_vram = ram;
	membank( THOM_CART_BANK )->configure_entries( 0, 4, mem + 0x10000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 4, 2, mem + 0x1f000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 6, 2, mem + 0x28000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 8, 8, ram, 0x4000 );
	membank( THOM_VRAM_BANK )->configure_entries( 0, 2, ram, 0x2000 );
	membank( TO8_SYS_LO )->configure_entry( 0, ram + 0x6000);
	membank( TO8_SYS_HI )->configure_entry( 0, ram + 0x4000);
	membank( TO8_DATA_LO )->configure_entries( 0, 8, ram + 0x2000, 0x4000 );
	membank( TO8_DATA_HI )->configure_entries( 0, 8, ram + 0x0000, 0x4000 );
	membank( TO8_BIOS_BANK )->configure_entries( 0, 2, mem + 0x23000, 0x4000 );
	membank( THOM_CART_BANK )->set_entry( 0 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );
	membank( TO8_SYS_LO )->set_entry( 0 );
	membank( TO8_SYS_HI )->set_entry( 0 );
	membank( TO8_DATA_LO )->set_entry( 0 );
	membank( TO8_DATA_HI )->set_entry( 0 );
	membank( TO8_BIOS_BANK )->set_entry( 0 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_item(NAME(m_to8_reg_ram));
	save_item(NAME(m_to8_reg_cart));
	save_item(NAME(m_to8_reg_sys1));
	save_item(NAME(m_to8_reg_sys2));
	save_item(NAME(m_to8_lightpen_intr));
	save_item(NAME(m_to8_data_vpage));
	save_item(NAME(m_to8_cart_vpage));
	save_item(NAME(m_mo5_reg_cart));
	save_pointer(NAME(mem + 0x10000), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::mo6_update_ram_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::mo6_update_cart_bank_postload), this));
}



/***************************** MO5 NR *************************/



/* ------------ network ( & external floppy) ------------ */



READ8_MEMBER( thomson_state::mo5nr_net_r )
{
	if ( space.debugger_access() )
		return 0;

	if ( to7_controller_type )
		return to7_floppy_r ( space, offset );

	logerror( "$%04x %f mo5nr_net_r: read from reg %i\n", m_maincpu->pc(), machine().time().as_double(), offset );

	return 0;
}



WRITE8_MEMBER( thomson_state::mo5nr_net_w )
{
	if ( to7_controller_type )
		to7_floppy_w ( space, offset, data );
	else
		logerror( "$%04x %f mo5nr_net_w: write $%02X to reg %i\n",
				m_maincpu->pc(), machine().time().as_double(), data, offset );
}


/* ------------ printer ------------ */

/* Unlike the TO8, TO9, TO9+, MO6, the printer has its own ports and does not
   go through the 6821 PIA.
*/


READ8_MEMBER( thomson_state::mo5nr_prn_r )
{
	UINT8 result = 0;

	result |= !m_centronics->busy_r() << 7;

	return result;
}


WRITE8_MEMBER( thomson_state::mo5nr_prn_w )
{
	/* TODO: understand other bits */
	m_centronics->strobe_w(BIT(data, 3));
}



/* ------------ system PIA 6821 ------------ */



READ8_MEMBER( thomson_state::mo5nr_sys_portb_in )
{
	/* keyboard: only 8 lines of 8 keys (MO6 has 9 lines) */
	UINT8 portb = m_pia_sys->b_output();
	int col = (portb >> 4) & 7;    /* B bits 4-6: kbd column */
	int lin = (portb >> 1) & 7;    /* B bits 1-3: kbd line */
	static const char *const keynames[] = {
		"keyboard_0", "keyboard_1", "keyboard_2", "keyboard_3",
		"keyboard_4", "keyboard_5", "keyboard_6", "keyboard_7"
	};

	return ( ioport(keynames[lin])->read() & (1 << col) ) ? 0x80 : 0;
	/* bit 7: key up */
}



WRITE8_MEMBER( thomson_state::mo5nr_sys_porta_out )
{
	/* no keyboard LED */
	thom_set_mode_point( data & 1 );           /* bit 0: video bank switch */
	m_to7_game_mute = data & 4;                       /* bit 2: sound mute */
	mo5_set_cassette( (data & 0x40) ? 1 : 0 );     /* bit 6: cassette output */
	mo6_update_cart_bank();                  /* bit 5: rom bank */
	to7_game_sound_update();
}



/* ------------ game 6821 PIA ------------ */

/* similar to the MO6, without the printer */



void thomson_state::mo5nr_game_init()
{
	LOG (( "mo5nr_game_init called\n" ));
	m_to7_game_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::mo6_game_update_cb),this));
	m_to7_game_timer->adjust( TO7_GAME_POLL_PERIOD, 0, TO7_GAME_POLL_PERIOD );
	save_item(NAME(m_to7_game_sound));
	save_item(NAME(m_to7_game_mute));
}



void thomson_state::mo5nr_game_reset()
{
	LOG (( "mo5nr_game_reset called\n" ));
	m_pia_game->ca1_w( 0 );
	m_to7_game_sound = 0;
	m_to7_game_mute = 0;
	to7_game_sound_update();
}



/* ------------ init / reset ------------ */



MACHINE_RESET_MEMBER( thomson_state, mo5nr )
{
	LOG (( "mo5nr: machine reset called\n" ));

	/* subsystems */
	thom_irq_reset();
	m_pia_sys->set_port_a_z_mask( 0x65 );
	mo5nr_game_reset();
	to7_floppy_reset();
	to7_modem_reset();
	to7_midi_reset();
	mo5_init_timer();

	/* gate-array */
	m_to7_lightpen = 0;
	m_to8_reg_ram = 0;
	m_to8_reg_cart = 0;
	m_to8_reg_sys1 = 0;
	m_to8_reg_sys2 = 0;
	m_to8_lightpen_intr = 0;

	/* video */
	thom_set_video_mode( THOM_VMODE_MO5 );
	m_thom_lightpen_cb = &thomson_state::to8_lightpen_cb;
	thom_set_lightpen_callback( 3 );
	thom_set_border_color( 0 );
	thom_set_mode_point( 0 );
	m_pia_sys->ca1_w( 0 );

	/* memory */
	m_old_ram_bank = -1;
	m_old_cart_bank = -1;
	m_to8_cart_vpage = 0;
	m_to8_data_vpage = 0;
	mo6_update_ram_bank();
	mo6_update_cart_bank();
	/* mo5_reg_cart not reset */
	/* thom_cart_bank not reset */
}



MACHINE_START_MEMBER( thomson_state, mo5nr )
{
	UINT8* mem = memregion("maincpu")->base();
	UINT8* ram = m_ram->pointer();

	LOG (( "mo5nr: machine start called\n" ));

	/* subsystems */
	thom_irq_init();
	mo5nr_game_init();
	to7_floppy_init( mem + 0x30000 );
	to9_palette_init();
	to7_modem_init();
	to7_midi_init();
	m_mo5_periodic_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(thomson_state::mo5_periodic_cb),this));

	/* memory */
	m_thom_cart_bank = 0;
	m_mo5_reg_cart = 0;
	m_thom_vram = ram;
	membank( THOM_CART_BANK )->configure_entries( 0, 4, mem + 0x10000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 4, 2, mem + 0x1f000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 6, 2, mem + 0x28000, 0x4000 );
	membank( THOM_CART_BANK )->configure_entries( 8, 8, ram, 0x4000 );
	membank( THOM_VRAM_BANK )->configure_entries( 0, 2, ram, 0x2000 );
	membank( TO8_SYS_LO )->configure_entry( 0, ram + 0x6000);
	membank( TO8_SYS_HI )->configure_entry( 0, ram + 0x4000);
	membank( TO8_DATA_LO )->configure_entries( 0, 8, ram + 0x2000, 0x4000 );
	membank( TO8_DATA_HI )->configure_entries( 0, 8, ram + 0x0000, 0x4000 );
	membank( TO8_BIOS_BANK )->configure_entries( 0, 2, mem + 0x23000, 0x4000 );
	membank( THOM_CART_BANK )->set_entry( 0 );
	membank( THOM_VRAM_BANK )->set_entry( 0 );
	membank( TO8_SYS_LO )->set_entry( 0 );
	membank( TO8_SYS_HI )->set_entry( 0 );
	membank( TO8_DATA_LO )->set_entry( 0 );
	membank( TO8_DATA_HI )->set_entry( 0 );
	membank( TO8_BIOS_BANK )->set_entry( 0 );

	/* save-state */
	save_item(NAME(m_thom_cart_nb_banks));
	save_item(NAME(m_thom_cart_bank));
	save_item(NAME(m_to7_lightpen));
	save_item(NAME(m_to7_lightpen_step));
	save_item(NAME(m_to8_reg_ram));
	save_item(NAME(m_to8_reg_cart));
	save_item(NAME(m_to8_reg_sys1));
	save_item(NAME(m_to8_reg_sys2));
	save_item(NAME(m_to8_lightpen_intr));
	save_item(NAME(m_to8_data_vpage));
	save_item(NAME(m_to8_cart_vpage));
	save_item(NAME(m_mo5_reg_cart));
	save_pointer(NAME(mem + 0x10000), 0x10000 );
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::mo6_update_ram_bank_postload), this));
	machine().save().register_postload(save_prepost_delegate(FUNC(thomson_state::mo6_update_cart_bank_postload), this));
}
