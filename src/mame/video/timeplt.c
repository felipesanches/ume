/***************************************************************************

    Time Pilot

    driver by Nicola Salmoria

***************************************************************************/

#include "emu.h"
#include "includes/timeplt.h"

/***************************************************************************

  Convert the color PROMs into a more useable format.

  Time Pilot has two 32x8 palette PROMs and two 256x4 lookup table PROMs
  (one for characters, one for sprites).
  The palette PROMs are connected to the RGB output this way:

  bit 7 -- 390 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 560 ohm resistor  -- BLUE
        -- 820 ohm resistor  -- BLUE
        -- 1.2kohm resistor  -- BLUE
        -- 390 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
  bit 0 -- 560 ohm resistor  -- GREEN

  bit 7 -- 820 ohm resistor  -- GREEN
        -- 1.2kohm resistor  -- GREEN
        -- 390 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
        -- 560 ohm resistor  -- RED
        -- 820 ohm resistor  -- RED
        -- 1.2kohm resistor  -- RED
  bit 0 -- not connected

***************************************************************************/

void timeplt_state::palette_init()
{
	const UINT8 *color_prom = memregion("proms")->base();
	rgb_t palette[32];
	int i;

	for (i = 0; i < 32; i++)
	{
		int bit0, bit1, bit2, bit3, bit4, r, g, b;

		bit0 = (color_prom[i + 1 * 32] >> 1) & 0x01;
		bit1 = (color_prom[i + 1 * 32] >> 2) & 0x01;
		bit2 = (color_prom[i + 1 * 32] >> 3) & 0x01;
		bit3 = (color_prom[i + 1 * 32] >> 4) & 0x01;
		bit4 = (color_prom[i + 1 * 32] >> 5) & 0x01;
		r = 0x19 * bit0 + 0x24 * bit1 + 0x35 * bit2 + 0x40 * bit3 + 0x4d * bit4;
		bit0 = (color_prom[i + 1 * 32] >> 6) & 0x01;
		bit1 = (color_prom[i + 1 * 32] >> 7) & 0x01;
		bit2 = (color_prom[i + 0 * 32] >> 0) & 0x01;
		bit3 = (color_prom[i + 0 * 32] >> 1) & 0x01;
		bit4 = (color_prom[i + 0 * 32] >> 2) & 0x01;
		g = 0x19 * bit0 + 0x24 * bit1 + 0x35 * bit2 + 0x40 * bit3 + 0x4d * bit4;
		bit0 = (color_prom[i + 0 * 32] >> 3) & 0x01;
		bit1 = (color_prom[i + 0 * 32] >> 4) & 0x01;
		bit2 = (color_prom[i + 0 * 32] >> 5) & 0x01;
		bit3 = (color_prom[i + 0 * 32] >> 6) & 0x01;
		bit4 = (color_prom[i + 0 * 32] >> 7) & 0x01;
		b = 0x19 * bit0 + 0x24 * bit1 + 0x35 * bit2 + 0x40 * bit3 + 0x4d * bit4;

		palette[i] = MAKE_RGB(r, g, b);
	}

	color_prom += 2*32;
	/* color_prom now points to the beginning of the lookup table */


	/* sprites */
	for (i = 0; i < 64 * 4; i++)
		palette_set_color(machine(), 32 * 4 + i, palette[*color_prom++ & 0x0f]);

	/* characters */
	for (i = 0; i < 32 * 4; i++)
		palette_set_color(machine(), i, palette[(*color_prom++ & 0x0f) + 0x10]);
}



/*************************************
 *
 *  Tilemap info callback
 *
 *************************************/

TILE_GET_INFO_MEMBER(timeplt_state::get_tile_info)
{
	int attr = m_colorram[tile_index];
	int code = m_videoram[tile_index] + 8 * (attr & 0x20);
	int color = attr & 0x1f;
	int flags = TILE_FLIPYX(attr >> 6);

	tileinfo.category = (attr & 0x10) >> 4;
	SET_TILE_INFO_MEMBER(0, code, color, flags);
}

TILE_GET_INFO_MEMBER(timeplt_state::get_chkun_tile_info)
{
	int attr = m_colorram[tile_index];
	int code = m_videoram[tile_index] + ((attr & 0x60) << 3);
	int color = attr & 0x1f;
	int flags = 0;//TILE_FLIPYX(attr >> 6);

	tileinfo.category = (attr & 0x80) >> 7;
	SET_TILE_INFO_MEMBER(0, code, color, flags);
}



/*************************************
 *
 *  Video startup
 *
 *************************************/

void timeplt_state::video_start()
{
	m_bg_tilemap = &machine().tilemap().create(tilemap_get_info_delegate(FUNC(timeplt_state::get_tile_info),this), TILEMAP_SCAN_ROWS, 8, 8, 32, 32);
}

VIDEO_START_MEMBER(timeplt_state,chkun)
{
	m_bg_tilemap = &machine().tilemap().create(tilemap_get_info_delegate(FUNC(timeplt_state::get_chkun_tile_info),this), TILEMAP_SCAN_ROWS, 8, 8, 32, 32);
}



/*************************************
 *
 *  Memory write handlers
 *
 *************************************/

WRITE8_MEMBER(timeplt_state::timeplt_videoram_w)
{
	m_videoram[offset] = data;
	m_bg_tilemap->mark_tile_dirty(offset);
}


WRITE8_MEMBER(timeplt_state::timeplt_colorram_w)
{
	m_colorram[offset] = data;
	m_bg_tilemap->mark_tile_dirty(offset);
}


WRITE8_MEMBER(timeplt_state::timeplt_flipscreen_w)
{
	flip_screen_set(~data & 1);
}


READ8_MEMBER(timeplt_state::timeplt_scanline_r)
{
	return m_screen->vpos();
}



/*************************************
 *
 *  Sprite rendering
 *
 *************************************/

void timeplt_state::draw_sprites( bitmap_ind16 &bitmap, const rectangle &cliprect )
{
	UINT8 *spriteram = m_spriteram;
	UINT8 *spriteram_2 = m_spriteram2;
	int offs;

	for (offs = 0x3e;offs >= 0x10;offs -= 2)
	{
		int sx = spriteram[offs];
		int sy = 241 - spriteram_2[offs + 1];

		int code = spriteram[offs + 1];
		int color = spriteram_2[offs] & 0x3f;
		int flipx = ~spriteram_2[offs] & 0x40;
		int flipy = spriteram_2[offs] & 0x80;

		drawgfx_transpen(bitmap,cliprect,machine().gfx[1],
				code,
				color,
				flipx,flipy,
				sx,sy,0);
	}
}



/*************************************
 *
 *  Video update
 *
 *************************************/

UINT32 timeplt_state::screen_update_timeplt(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	m_bg_tilemap->draw(screen, bitmap, cliprect, 0, 0);
	draw_sprites(bitmap, cliprect);
	m_bg_tilemap->draw(screen, bitmap, cliprect, 1, 0);
	return 0;
}
