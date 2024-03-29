/*
 * nld_74107.c
 *
 */

#include "nld_74107.h"

NETLIB_START(nic74107Asub)
{
	register_input("CLK", m_clk);
	register_output("Q", m_Q);
	register_output("QQ", m_QQ);

	save(NAME(m_Q1));
	save(NAME(m_Q2));
	save(NAME(m_F));
}

NETLIB_RESET(nic74107Asub)
{
    m_clk.set_state(netlist_input_t::STATE_INP_HL);
    m_Q.initial(0);
    m_QQ.initial(1);

    m_Q1 = 0;
    m_Q2 = 0;
    m_F = 0;
}

NETLIB_START(nic74107A)
{
	register_sub(sub, "sub");

	register_subalias("CLK", sub.m_clk);
	register_input("J", m_J);
	register_input("K", m_K);
	register_input("CLRQ", m_clrQ);
	register_subalias("Q", sub.m_Q);
	register_subalias("QQ", sub.m_QQ);

}

NETLIB_RESET(nic74107A)
{
    sub.reset();
}

ATTR_HOT inline void NETLIB_NAME(nic74107Asub)::newstate(const netlist_sig_t state)
{
	const netlist_time delay[2] = { NLTIME_FROM_NS(40), NLTIME_FROM_NS(25) };

	OUTLOGIC(m_Q, state, delay[state ^ 1]);
	OUTLOGIC(m_QQ, state ^ 1, delay[state]);
}

NETLIB_UPDATE(nic74107Asub)
{
	const netlist_sig_t t = m_Q.net().Q();
	newstate((!t & m_Q1) | (t & m_Q2) | m_F);
	if (!m_Q1)
		m_clk.inactivate();
}

NETLIB_UPDATE(nic74107A)
{
	const UINT8 JK = (INPLOGIC(m_J) << 1) | INPLOGIC(m_K);

	switch (JK)
	{
		case 0:
			sub.m_Q1 = 0;
			sub.m_Q2 = 1;
			sub.m_F  = 0;
			sub.m_clk.inactivate();
			break;
		case 1:             // (!INPLOGIC(m_J) & INPLOGIC(m_K))
			sub.m_Q1 = 0;
			sub.m_Q2 = 0;
			sub.m_F  = 0;
			break;
		case 2:             // (INPLOGIC(m_J) & !INPLOGIC(m_K))
			sub.m_Q1 = 0;
			sub.m_Q2 = 0;
			sub.m_F  = 1;
			break;
		case 3:             // (INPLOGIC(m_J) & INPLOGIC(m_K))
			sub.m_Q1 = 1;
			sub.m_Q2 = 0;
			sub.m_F  = 0;
			break;
		default:
			break;
	}

	if (!INPLOGIC(m_clrQ))
	{
		sub.m_clk.inactivate();
		sub.newstate(0);
	}
	else if (!sub.m_Q2)
		sub.m_clk.activate_hl();
}
