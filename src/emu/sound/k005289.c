/***************************************************************************

    Konami 005289 - SCC sound as used in Bubblesystem

    This file is pieced together by Bryan McPhail from a combination of
    Namco Sound, Amuse by Cab, Nemesis schematics and whoever first
    figured out SCC!

    The 005289 is a 2 channel sound generator, each channel gets it's
    waveform from a prom (4 bits wide).

    (From Nemesis schematics)

    Address lines A0-A4 of the prom run to the 005289, giving 32 bytes
    per waveform.  Address lines A5-A7 of the prom run to PA5-PA7 of
    the AY8910 control port A, giving 8 different waveforms. PA0-PA3
    of the AY8910 control volume.

    The second channel is the same as above except port B is used.

    The 005289 has no data bus, so data values written don't matter.

    There are 4 unknown pins, LD1, LD2, TG1, TG2.  Two of them look to be
    the selector for changing frequency.  The other two seem unused.

***************************************************************************/

#include "emu.h"
#include "k005289.h"

// is this an actual hardware limit? or just an arbitrary divider
// to bring the output frequency down to a reasonable value for MAME?
#define CLOCK_DIVIDER 32

// device type definition
const device_type K005289 = &device_creator<k005289_device>;


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  k005289_device - constructor
//-------------------------------------------------

k005289_device::k005289_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: device_t(mconfig, K005289, "K005289", tag, owner, clock, "k005289", __FILE__),
		device_sound_interface(mconfig, *this),
	m_sound_prom(NULL),
	m_stream(NULL),
	m_rate(0),
	m_mixer_table(NULL),
	m_mixer_lookup(NULL),
	m_mixer_buffer(NULL)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void k005289_device::device_start()
{
	/* get stream channels */
	m_rate = clock() / CLOCK_DIVIDER;
	m_stream = stream_alloc(0, 1, m_rate);

	/* allocate a pair of buffers to mix into - 1 second's worth should be more than enough */
	m_mixer_buffer = auto_alloc_array(machine(), short, 2 * m_rate);

	/* build the mixer table */
	make_mixer_table(2);

	m_sound_prom = m_region->base();

	/* reset all the voices */
	for (int i = 0; i < 2; i++)
	{
		m_counter[i] = 0;
		m_frequency[i] = 0;
		m_freq_latch[i] = 0;
		m_waveform[i] = i * 0x100;
		m_volume[i] = 0;
	}

	save_item(NAME(m_counter));
	save_item(NAME(m_frequency));
	save_item(NAME(m_freq_latch));
	save_item(NAME(m_waveform));
	save_item(NAME(m_volume));
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void k005289_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	stream_sample_t *buffer = outputs[0];
	short *mix;
	int i,v,f;

	/* zap the contents of the mixer buffer */
	memset(m_mixer_buffer, 0, samples * sizeof(INT16));

	v=m_volume[0];
	f=m_frequency[0];
	if (v && f)
	{
		const unsigned char *w = m_sound_prom + m_waveform[0];
		int c = m_counter[0];

		mix = m_mixer_buffer;

		/* add our contribution */
		for (i = 0; i < samples; i++)
		{
			int offs;

			c += CLOCK_DIVIDER;
			offs = (c / f) & 0x1f;
			*mix++ += ((w[offs] & 0x0f) - 8) * v;
		}

		/* update the counter for this voice */
		m_counter[0] = c % (f * 0x20);
	}

	v=m_volume[1];
	f=m_frequency[1];
	if (v && f)
	{
		const unsigned char *w = m_sound_prom + m_waveform[1];
		int c = m_counter[1];

		mix = m_mixer_buffer;

		/* add our contribution */
		for (i = 0; i < samples; i++)
		{
			int offs;

			c += CLOCK_DIVIDER;
			offs = (c / f) & 0x1f;
			*mix++ += ((w[offs] & 0x0f) - 8) * v;
		}

		/* update the counter for this voice */
		m_counter[1] = c % (f * 0x20);
	}

	/* mix it down */
	mix = m_mixer_buffer;
	for (i = 0; i < samples; i++)
		*buffer++ = m_mixer_lookup[*mix++];
}




/********************************************************************************/

/* build a table to divide by the number of voices */
void k005289_device::make_mixer_table(int voices)
{
	int count = voices * 128;
	int i;
	int gain = 16;

	/* allocate memory */
	m_mixer_table = auto_alloc_array(machine(), INT16, 256 * voices);

	/* find the middle of the table */
	m_mixer_lookup = m_mixer_table + (128 * voices);

	/* fill in the table - 16 bit case */
	for (i = 0; i < count; i++)
	{
		int val = i * gain * 16 / voices;
		if (val > 32767) val = 32767;
		m_mixer_lookup[ i] = val;
		m_mixer_lookup[-i] = -val;
	}
}


WRITE8_MEMBER( k005289_device::k005289_control_A_w )
{
	m_stream->update();

	m_volume[0] = data & 0xf;
	m_waveform[0] = data & 0xe0;
}


WRITE8_MEMBER( k005289_device::k005289_control_B_w )
{
	m_stream->update();

	m_volume[1] = data & 0xf;
	m_waveform[1] = (data & 0xe0) + 0x100;
}


WRITE8_MEMBER( k005289_device::k005289_pitch_A_w )
{
	m_freq_latch[0] = 0x1000 - offset;
}


WRITE8_MEMBER( k005289_device::k005289_pitch_B_w )
{
	m_freq_latch[1] = 0x1000 - offset;
}


WRITE8_MEMBER( k005289_device::k005289_keylatch_A_w )
{
	m_stream->update();

	m_frequency[0] = m_freq_latch[0];
}


WRITE8_MEMBER( k005289_device::k005289_keylatch_B_w )
{
	m_stream->update();

	m_frequency[1] = m_freq_latch[1];
}
