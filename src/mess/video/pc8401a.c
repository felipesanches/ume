#include "includes/pc8401a.h"
#include "pc8500.lh"

/* PC-8401A */

PALETTE_INIT_MEMBER(pc8401a_state,pc8401a)
{
	palette_set_color(machine(), 0, MAKE_RGB(39, 108, 51));
	palette_set_color(machine(), 1, MAKE_RGB(16, 37, 84));
}

void pc8401a_state::video_start()
{
}

/* PC-8500 */

void pc8500_state::video_start()
{
}

UINT32 pc8500_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
//  m_lcdc->screen_update(screen, bitmap, cliprect);

	/*
	if (strcmp(screen.tag(), SCREEN_TAG) == 0)
	{
	    sed1330_update(m_lcdc, &bitmap, cliprect);
	}
	else
	{
	    m_crtc->update(bitmap, cliprect);
	}
	*/

	return 0;
}

/* SED1330 Interface */

static ADDRESS_MAP_START( pc8401a_lcdc, AS_0, 8, pc8401a_state )
	ADDRESS_MAP_GLOBAL_MASK(0x1fff)
	AM_RANGE(0x0000, 0x1fff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( pc8500_lcdc, AS_0, 8, pc8401a_state )
	ADDRESS_MAP_GLOBAL_MASK(0x3fff)
	AM_RANGE(0x0000, 0x3fff) AM_RAM
ADDRESS_MAP_END

/* MC6845 Interface */

static MC6845_UPDATE_ROW( pc8441a_update_row )
{
}


static MC6845_INTERFACE( pc8441a_mc6845_interface )
{
	false,
	0,0,0,0,
	6,
	NULL,
	pc8441a_update_row,
	NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	NULL
};

/* Machine Drivers */

MACHINE_CONFIG_FRAGMENT( pc8401a_video )
//  MCFG_DEFAULT_LAYOUT(layout_pc8401a)

	MCFG_PALETTE_LENGTH(2)
	MCFG_PALETTE_INIT_OVERRIDE(pc8401a_state,pc8401a)

	/* LCD */
	MCFG_SCREEN_ADD(SCREEN_TAG, LCD)
	MCFG_SCREEN_REFRESH_RATE(44)
	MCFG_SCREEN_UPDATE_DEVICE(SED1330_TAG, sed1330_device, screen_update)
	MCFG_SCREEN_SIZE(480, 128)
	MCFG_SCREEN_VISIBLE_AREA(0, 480-1, 0, 128-1)

	MCFG_SED1330_ADD(SED1330_TAG, 0, SCREEN_TAG, pc8401a_lcdc)
MACHINE_CONFIG_END

MACHINE_CONFIG_FRAGMENT( pc8500_video )
	MCFG_DEFAULT_LAYOUT(layout_pc8500)

	MCFG_PALETTE_LENGTH(2+8)
	MCFG_PALETTE_INIT_OVERRIDE(pc8401a_state,pc8401a)

	/* LCD */
	MCFG_SCREEN_ADD(SCREEN_TAG, LCD)
	MCFG_SCREEN_REFRESH_RATE(44)
	MCFG_SCREEN_UPDATE_DEVICE(SED1330_TAG, sed1330_device, screen_update)
	MCFG_SCREEN_SIZE(480, 208)
	MCFG_SCREEN_VISIBLE_AREA(0, 480-1, 0, 200-1)

	MCFG_SED1330_ADD(SED1330_TAG, 0, SCREEN_TAG, pc8500_lcdc)

	/* PC-8441A CRT */
	MCFG_SCREEN_ADD(CRT_SCREEN_TAG, RASTER)
	MCFG_SCREEN_UPDATE_DRIVER(pc8500_state, screen_update)
	MCFG_SCREEN_SIZE(80*8, 24*8)
	MCFG_SCREEN_VISIBLE_AREA(0, 80*8-1, 0, 24*8-1)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500))
	MCFG_SCREEN_REFRESH_RATE(50)

	MCFG_MC6845_ADD(MC6845_TAG, MC6845, CRT_SCREEN_TAG, 400000, pc8441a_mc6845_interface)
MACHINE_CONFIG_END
