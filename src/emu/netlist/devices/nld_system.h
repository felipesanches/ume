// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_system.h
 *
 * netlist devices defined in the core
 */

#ifndef NLD_SYSTEM_H_
#define NLD_SYSTEM_H_

#include "../nl_setup.h"
#include "../nl_base.h"

// ----------------------------------------------------------------------------------------
// Macros
// ----------------------------------------------------------------------------------------

#define NETDEV_TTL_INPUT(_name, _v)                                                 \
		NET_REGISTER_DEV(ttl_input, _name)                                          \
		NETDEV_PARAM(_name.IN, _v)

#define NETDEV_ANALOG_INPUT(_name, _v)                                              \
		NET_REGISTER_DEV(analog_input, _name)                                       \
		NETDEV_PARAM(_name.IN, _v)

#define NETDEV_MAINCLOCK(_name)                                                     \
		NET_REGISTER_DEV(mainclock, _name)

#define NETDEV_CLOCK(_name)                                                         \
		NET_REGISTER_DEV(clock, _name)

// ----------------------------------------------------------------------------------------
// mainclock
// ----------------------------------------------------------------------------------------

NETLIB_DEVICE_WITH_PARAMS(mainclock,
public:
	netlist_ttl_output_t m_Q;

	netlist_param_double_t m_freq;
	netlist_time m_inc;

	ATTR_HOT inline static void mc_update(netlist_net_t &net, const netlist_time curtime);
);

// ----------------------------------------------------------------------------------------
// clock
// ----------------------------------------------------------------------------------------

NETLIB_DEVICE_WITH_PARAMS(clock,
	netlist_ttl_input_t m_feedback;
	netlist_ttl_output_t m_Q;

	netlist_param_double_t m_freq;
	netlist_time m_inc;
);


// ----------------------------------------------------------------------------------------
// Special support devices ...
// ----------------------------------------------------------------------------------------

NETLIB_DEVICE_WITH_PARAMS(ttl_input,
	netlist_ttl_output_t m_Q;

	netlist_param_logic_t m_IN;
);

NETLIB_DEVICE_WITH_PARAMS(analog_input,
	netlist_analog_output_t m_Q;

    netlist_param_double_t m_IN;
);



#endif /* NLD_SYSTEM_H_ */
