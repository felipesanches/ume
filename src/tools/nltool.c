/***************************************************************************

    nltool.c

    Simple tool to debug netlists outside MAME.

****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "astring.h"
#include "corefile.h"
#include "corestr.h"
#include "sha1.h"
#include "netlist/nl_base.h"
#include "netlist/nl_setup.h"
#include "netlist/nl_util.h"
#include "options.h"

/***************************************************************************
    MAME COMPATIBILITY ...
***************************************************************************/

#if 0
void mame_printf_warning(const char *format, ...)
{
	va_list argptr;

	/* do the output */
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);
}
#endif

void *malloc_file_line(size_t size, const char *file, int line)
{
	// allocate the memory and fail if we can't
	void *ret = osd_malloc(size);
	memset(ret, 0, size);
	return ret;
}

void *malloc_array_file_line(size_t size, const char *file, int line)
{
	// allocate the memory and fail if we can't
	void *ret = osd_malloc_array(size);
	memset(ret, 0, size);
	return ret;
}

void free_file_line( void *memory, const char *file, int line )
{
	osd_free( memory );
}


struct options_entry oplist[] =
{
	{ "time_to_run;t",   "1.0", OPTION_FLOAT,   "time to run the emulation (seconds)" },
    { "logs;l",          "",    OPTION_STRING,  "colon separated list of terminals to log" },
	{ "f",               "-",   OPTION_STRING,  "file to process (default is stdin)" },
	{ "help;h",          "0",   OPTION_BOOLEAN, "display help" },
	{ NULL }
};

/***************************************************************************
    CORE IMPLEMENTATION
***************************************************************************/

const char *filetobuf(pstring fname)
{
	static pstring pbuf = "";

	if (fname == "-")
	{
		char lbuf[1024];
		while (!feof(stdin))
		{
			fgets(lbuf, 1024, stdin);
			pbuf += lbuf;
		}
		printf("%d\n",*(pbuf.right(1).cstr()+1));
		return pbuf.cstr();
	}
	else
	{
		FILE *f;
		f = fopen(fname, "rb");
		fseek(f, 0, SEEK_END);
		long fsize = ftell(f);
		fseek(f, 0, SEEK_SET);

		char *buf = (char *) malloc(fsize + 1);
		fread(buf, fsize, 1, f);
		buf[fsize] = 0;
		fclose(f);
		return buf;
	}
}

class netlist_tool_t : public netlist_base_t
{
public:

	netlist_tool_t()
	: netlist_base_t(), m_logs(""), m_setup(NULL)
	{
	}

	virtual ~netlist_tool_t() { };

    void read_netlist(const char *buffer)
    {
        m_setup = new netlist_setup_t(*this);
        this->init_object(*this, "netlist");
        m_setup->init();

		// read the netlist ...
		//m_setup_func(*m_setup);

		m_setup->parse(buffer);
		log_setup();

		// start devices
		m_setup->start_devices();
		m_setup->resolve_inputs();
		// reset
		this->reset();
	}

    void log_setup()
    {
        NL_VERBOSE_OUT(("Creating dynamic logs ...\n"));
        nl_util::pstring_list ll = nl_util::split(m_logs, ":");
        for (int i=0; i < ll.count(); i++)
        {
            netlist_device_t *nc = m_setup->factory().new_device_by_classname("nld_log", *m_setup);
            pstring name = "log_" + ll[i];
            m_setup->register_dev(nc, name);
            m_setup->register_link(name + ".I", ll[i]);
        }
    }

    pstring m_logs;
protected:

	void vfatalerror(const char *format, va_list ap) const
	{
		vprintf(format, ap);
		throw;
	}

private:
	netlist_setup_t *m_setup;
};


void usage(core_options &opts)
{
	astring buffer;
	fprintf(stderr,
		"Usage:\n"
		"  nltool -help\n"
		"  nltool [options]\n"
		"\n"
		"Where:\n"
	);
	fprintf(stderr, "%s\n", opts.output_help(buffer));
}

/*-------------------------------------------------
    main - primary entry point
-------------------------------------------------*/

int main(int argc, char *argv[])
{
	//int result;
	netlist_tool_t nt;
	core_options opts(oplist);
	astring aerror("");
	osd_ticks_t t = osd_ticks();

	fprintf(stderr, "%s", "WARNING: This is Work In Progress! - It may fail anytime\n");
	if (!opts.parse_command_line(argc, argv, OPTION_PRIORITY_DEFAULT, aerror))
	{
		fprintf(stderr, "%s\n", aerror.cstr());
		usage(opts);
		return 1;
	}
	if (opts.bool_value("h"))
	{
		usage(opts);
		return 1;
	}

	nt.m_logs = opts.value("l");
	nt.read_netlist(filetobuf(opts.value("f")));
	double ttr = opts.float_value("t");

	printf("startup time ==> %5.3f\n", (double) (osd_ticks() - t) / (double) osd_ticks_per_second() );
	printf("runnning ...\n");
	t = osd_ticks();

	nt.process_queue(netlist_time::from_double(ttr));

	double emutime = (double) (osd_ticks() - t) / (double) osd_ticks_per_second();
	printf("%f seconds emulation took %f real time ==> %5.2f%%\n", ttr, emutime, ttr/emutime*100.0);

	return 0;
}
