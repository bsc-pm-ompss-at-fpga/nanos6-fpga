/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2023 Barcelona Supercomputing Center (BSC)
*/


#include <config.h>

#include "ConfigCentral.hpp"
#include "hardware/HardwareInfo.hpp"


ConfigCentral::ConfigCentral() :
	_descriptors(),
	_defaults(),
	_listDefaults()
{
	// CPU manager
	registerOption<integer_t>("cpumanager.busy_iters", 240000);
	registerOption<string_t>("cpumanager.policy", "default");
	registerOption<integer_t>("cpumanager.sponge_cpus", {});


	// DIRECTORY
	registerOption<bool_t>("devices.directory", true);

	// CUDA devices
	registerOption<string_t>("devices.cuda.kernels_folder", "nanos6-cuda-kernels");
	registerOption<bool_t>("devices.cuda.warning_on_incompatible_binary", true);
	registerOption<integer_t>("devices.cuda.page_size", 0x8000);
	registerOption<integer_t>("devices.cuda.streams", 16);
	registerOption<bool_t>("devices.cuda.prefetch", true);
	registerOption<bool_t>("devices.cuda.polling.pinned", true);
	registerOption<integer_t>("devices.cuda.polling.period_us", 1000);

	// OpenACC devices
	registerOption<integer_t>("devices.openacc.default_queues", 64);
	registerOption<integer_t>("devices.openacc.max_queues", 128);
	registerOption<bool_t>("devices.openacc.polling.pinned", true);
	registerOption<integer_t>("devices.openacc.polling.period_us", 1000);


	// FPGA Devices
	registerOption<integer_t>("devices.fpga.alignment", 16);
	registerOption<integer_t>("devices.fpga.page_size", 0x8000);
	registerOption<memory_t>("devices.fpga.requested_fpga_memory",0x40000000);
	registerOption<bool_t>("devices.fpga.reverse_offload", false);

	registerOption<integer_t>("devices.fpga.streams", 16);
	registerOption<bool_t>("devices.fpga.polling.pinned", true);
	registerOption<string_t>("devices.fpga.mem_sync_type", "sync");
	registerOption<integer_t>("devices.fpga.polling.period_us", 1000);

	// DLB
	registerOption<bool_t>("dlb.enabled", false);

	// Hardware counters
	registerOption<bool_t>("hardware_counters.verbose", false);
	registerOption<string_t>("hardware_counters.verbose_file", "nanos6-output-hwcounters.txt");

	// RAPL hardware counters
	registerOption<bool_t>("hardware_counters.rapl.enabled", false);

	// PAPI hardware counters
	registerOption<bool_t>("hardware_counters.papi.enabled", false);
	registerOption<string_t>("hardware_counters.papi.counters", {});

	// PQOS hardware counters
	registerOption<bool_t>("hardware_counters.pqos.enabled", false);
	registerOption<string_t>("hardware_counters.pqos.counters", {});

	// CTF instrumentation
	registerOption<bool_t>("instrument.ctf.converter.enabled", true);
	registerOption<bool_t>("instrument.ctf.converter.fast", false);
	registerOption<string_t>("instrument.ctf.converter.location", "");
	registerOption<string_t>("instrument.ctf.events.kernel.exclude", {});
	registerOption<string_t>("instrument.ctf.events.kernel.file", "");
	registerOption<string_t>("instrument.ctf.events.kernel.presets", {});
	registerOption<string_t>("instrument.ctf.tmpdir", "");

	// Ovni instrumentation
	registerOption<integer_t>("instrument.ovni.level", 2);

	// Extrae instrumentation
	registerOption<bool_t>("instrument.extrae.as_threads", false);
	registerOption<integer_t>("instrument.extrae.detail_level", 1);

	// Verbose instrumentation
	registerOption<string_t>("instrument.verbose.areas", {
			"all", "!ComputePlaceManagement", "!DependenciesByAccess",
			"!DependenciesByAccessLinks", "!DependenciesByGroup",
			"!LeaderThread", "!TaskStatus", "!ThreadManagement"
		});
	registerOption<bool_t>("instrument.verbose.dump_only_on_exit", false);
	registerOption<string_t>("instrument.verbose.output_file", "/dev/stderr");
	registerOption<bool_t>("instrument.verbose.timestamps", true);

	// Loader
	registerOption<bool_t>("loader.verbose", false);
	registerOption<bool_t>("loader.warn_envars", true);
	registerOption<string_t>("loader.library_path", "");
	registerOption<string_t>("loader.report_prefix", "");

	// Miscellaneous
	registerOption<memory_t>("misc.stack_size", 8 * 1024 * 1024);

	// Monitoring
	registerOption<integer_t>("monitoring.cpuusage_prediction_rate", 100);
	registerOption<bool_t>("monitoring.enabled", false);
	registerOption<integer_t>("monitoring.rolling_window", 20);
	registerOption<bool_t>("monitoring.verbose", true);
	registerOption<string_t>("monitoring.verbose_file", "output-monitoring.txt");
	registerOption<bool_t>("monitoring.wisdom", false);

	// NUMA support
	registerOption<bool_t>("numa.discover_pagesize", true);
	registerOption<bool_t>("numa.report", false);
	registerOption<bool_t>("numa.scheduling", true);
	registerOption<string_t>("numa.tracking", "auto");

	// Scheduler
	registerOption<float_t>("scheduler.immediate_successor", 1.0);
	registerOption<string_t>("scheduler.policy", "fifo");
	registerOption<bool_t>("scheduler.priority", true);

	// Taskfor. Not supported but do not fail if these options
	// are present.
	registerOption<integer_t>("taskfor.groups", 1);
	registerOption<bool_t>("taskfor.report", false);

	// Throttle
	registerOption<bool_t>("throttle.enabled", false);
	registerOption<memory_t>("throttle.max_memory", 0);
	registerOption<integer_t>("throttle.polling_period_us", 1000);
	registerOption<integer_t>("throttle.pressure", 70);
	registerOption<integer_t>("throttle.tasks", 5000000);

	// Turbo
	registerOption<bool_t>("turbo.enabled", false);
	registerOption<bool_t>("turbo.warmup", true);

	// Version details
	registerOption<bool_t>("version.debug", false);
	registerOption<string_t>("version.dependencies", "discrete");
	registerOption<string_t>("version.instrument", "none");
}
