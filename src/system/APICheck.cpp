/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2018-2020 Barcelona Supercomputing Center (BSC)
*/

#include <string.h>

#include <nanos6/api-check.h>


namespace APICheck {
	static bool _apiHasBeenCheckedSuccessfully = false;
}


static const nanos6_api_versions_t apiVersions = {
	.api_check_api_version = nanos6_api_check_api,
	.major_api_version = nanos6_major_api,

	.blocking_api_version = nanos6_blocking_api,
	.bootstrap_api_version = nanos6_bootstrap_api,
	.cluster_api_version = nanos6_cluster_api,
	.config_api_version = nanos6_config_api,
	.cuda_device_api_version = nanos6_cuda_device_api,
	.openacc_device_api_version = nanos6_openacc_device_api,
	.fpga_device_api_version = nanos6_fpga_device_api,
	.distributed_api_version = nanos6_distributed_api,
	.final_api_version = nanos6_final_api,
	.instantiation_api_version = nanos6_instantiation_api,
	.library_mode_api_version = nanos6_library_mode_api,
	.lint_api_version = nanos6_lint_api,
	.locking_api_version = nanos6_locking_api,
	.monitoring_api_version = nanos6_monitoring_api,
	.polling_api_version = nanos6_polling_api,
	.reductions_api_version = nanos6_reductions_api,
	.task_constraints_api_version = nanos6_task_constraints_api,
	.task_execution_api_version = nanos6_task_execution_api,
	.task_info_registration_api_version = nanos6_task_info_registration_api,
	.loop_api_version = nanos6_loop_api,
	.taskwait_api_version = nanos6_taskwait_api,
};


extern "C" int nanos6_check_api_versions(nanos6_api_versions_t const *api_versions)
{
	int rc = memcmp(&apiVersions, api_versions, sizeof(nanos6_api_versions_t));

	if (rc == 0) {
		APICheck::_apiHasBeenCheckedSuccessfully = true;
		return 1;
	} else {
		return 0;
	}
}


bool nanos6_api_has_been_checked_successfully()
{
	return APICheck::_apiHasBeenCheckedSuccessfully;
}
