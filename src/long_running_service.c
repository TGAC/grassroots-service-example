/*
** Copyright 2014-2016 The Earlham Institute
** 
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
** 
**     http://www.apache.org/licenses/LICENSE-2.0
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#include <string.h>
#include <time.h>

#include "long_running_service.h"
#include "memory_allocations.h"
#include "string_utils.h"
#include "jobs_manager.h"
#include "service_job_set_iterator.h"

#include "signed_int_parameter.h"
#include "unsigned_int_parameter.h"

#include "uuid_util.h"

/*
 * This service is an example to show how job data can be persisted between separate
 * requests. It mimics real world jobs by running a user-specified number of jobs that
 * are have a start and end times, each one an equivalent to a stopwatch.
 */

/*
 * This datatype stores the start and end times to mimic a real job.
 */
typedef struct TimeInterval
{
	/* The start time of the job. */
	time_t ti_start;

	/* The finish time of the job. */
	time_t ti_end;

	/*
	 * The duration of the job, simply ti_end - ti_start.
	 */
	time_t ti_duration;
} TimeInterval;


/*
 * This is the subclassed ServiceJob that is used to store the information
 * for the mimicked jobs that this Service runs.
 */
typedef struct TimedServiceJob
{
	/* The base ServiceJob */
	ServiceJob tsj_job;

	/*
	 * A pointer to the TimeInterval that is used to mimic the running of a real task.
	 */
	TimeInterval *tsj_interval_p;

	/* Has the TimedServiceJob been added to the JobsManager yet? */
	bool tsj_added_flag;

	/* The process */
	int32 tsj_process_id;
} TimedServiceJob;





/*
 * The ServiceData that this Service will use. Since we don't have any custom configuration
 * we could just use the base structure, ServiceData, instead but we want to show how to
 * extend it as that will be the common situation.
 */
typedef struct
{
	ServiceData lsd_base_data;
	uint32 lsd_default_number_of_jobs;

} LongRunningServiceData;


/*
 * STATIC DATATYPES
 */


/*
 * To store the persistent data for our tasks, we will use the
 * keys shown below.
 */


static const char * const LRS_SERVICE_JOB_TYPE_S = "long running service job";


/* This is the key used to specify the start time of the task. */
static const char * const LRS_START_S = "start";

/* This is the key used to specify the end time of the task. */
static const char * const LRS_END_S = "end";

/*
 * This is the key used to specify whether the task has been added
 * to the JobsManager yet.
 */
static const char * const LRS_ADDED_FLAG_S = "added_to_job_manager";


/*
 * We will have a single parameter that specifies how many tasks we want to
 * simulate.
 */
static NamedParameterType LRS_MIN_DURATION = { "Minimum duration of each job", PT_SIGNED_INT };

static NamedParameterType LRS_NUMBER_OF_JOBS = { "Number of Jobs", PT_UNSIGNED_INT };

/*
 * STATIC PROTOTYPES
 * =================
 *
 * These are the functions that the function pointers in our generated Service structure
 * will point to.
 */

static LongRunningServiceData *AllocateLongRunningServiceData (Service *service_p);

static void FreeLongRunningServiceData (LongRunningServiceData *data_p);

static const char *GetLongRunningServiceName (const Service *service_p);

static const char *GetLongRunningServiceAlias (const Service *service_p);

static const char *GetLongRunningServiceDescription (const Service *service_p);

static ParameterSet *GetLongRunningServiceParameters (Service *service_p, DataResource *resource_p, User *user_p);

static void ReleaseLongRunningServiceParameters (Service *service_p, ParameterSet *params_p);

static bool GetLongRunningServiceParameterTypesForNamedParameters (const Service *service_p, const char *param_name_s, ParameterType *pt_p);


static ServiceJobSet *RunLongRunningService (Service *service_p, ParameterSet *param_set_p, User *user_p, ProvidersStateTable *providers_p);

static ParameterSet *IsFileForLongRunningService (Service *service_p, DataResource *resource_p, Handler *handler_p);

static bool CloseLongRunningService (Service *service_p);

static json_t *GetLongRunningResultsAsJSON (Service *service_p, const uuid_t service_id);

static OperationStatus GetLongRunningServiceStatus (Service *service_p, const uuid_t service_id);

static unsigned char *SerialiseTimedServiceJob (ServiceJob *job_p, unsigned int *value_length_p);

static void StartTimedServiceJob (TimedServiceJob *job_p);


static OperationStatus GetTimedServiceJobStatus (ServiceJob *job_p);


static ServiceJobSet *GetServiceJobSet (Service *service_p, const uint32 num_jobs, const int32 min_duration);


static TimedServiceJob *AllocateTimedServiceJob (Service *service_p, const char * const job_name_s, const char * const job_description_s, const time_t duration);


static void FreeTimedServiceJob (ServiceJob *job_p);


static json_t *GetTimedServiceJobAsJSON (TimedServiceJob *job_p);


static TimedServiceJob *GetTimedServiceJobFromJSON (Service *service_p, const json_t *json_p);


static ServiceJob *BuildTimedServiceJob (Service *service_p, const json_t *service_job_json_p);


static json_t *BuildTimedServiceJobJSON (Service *service_p, ServiceJob *service_job_p, bool omit_results_flag);


static void CustomiseTimedServiceJob (Service *service_p, ServiceJob *job_p);


static bool UpdateTimedServiceJob (struct ServiceJob *job_p);


static ServiceMetadata *GetLongRunningServiceMetadata (Service *service_p);


/*
 * API FUNCTIONS
 */
ServicesArray *GetServices (User *user_p, GrassrootsServer *grassroots_p)
{
	Service *service_p = (Service *) AllocMemory (sizeof (Service));

	if (service_p)
		{
			/*
			 * Since we only have a single Service, create a ServicesArray with
			 * 1 item.
			 */
			ServicesArray *services_p = AllocateServicesArray (1);
			
			if (services_p)
				{		
					ServiceData *data_p = (ServiceData *) AllocateLongRunningServiceData (service_p);
					
					if (data_p)
						{
							/*
							 * Set up our Service structure and ServiceData.
							 */
							if (InitialiseService (service_p,
								GetLongRunningServiceName,
								GetLongRunningServiceDescription,
								GetLongRunningServiceAlias,
								NULL,
								RunLongRunningService,
								IsFileForLongRunningService,
								GetLongRunningServiceParameters,
								GetLongRunningServiceParameterTypesForNamedParameters,
								ReleaseLongRunningServiceParameters,
								CloseLongRunningService,
								CustomiseTimedServiceJob,
								true,
								SY_ASYNCHRONOUS_DETACHED,
								data_p,
								GetLongRunningServiceMetadata,
								NULL,
								grassroots_p))
								{
									* (services_p -> sa_services_pp) = service_p;


									/*
									 * We are going to store the data representing the asynchronous tasks
									 * in the JobsManager and so we need to specify the callback functions
									 * that we will use to convert our ServiceJobs to and from their JSON
									 * representations.
									 */
									service_p -> se_deserialise_job_json_fn = BuildTimedServiceJob;
									service_p -> se_serialise_job_json_fn = BuildTimedServiceJobJSON;

									return services_p;
								}
						}

					FreeServicesArray (services_p);
				}

			FreeService (service_p);
		}

	return NULL;
}


void ReleaseServices (ServicesArray *services_p)
{
	FreeServicesArray (services_p);
}


/*
 * STATIC FUNCTIONS 
 */
 

static LongRunningServiceData *AllocateLongRunningServiceData (Service * UNUSED_PARAM (service_p))
{
	LongRunningServiceData *data_p = (LongRunningServiceData *) AllocMemory (sizeof (LongRunningServiceData));

	if (data_p)
		{
			data_p -> lsd_default_number_of_jobs = 3;
			return data_p;
		}

	return NULL;
}


static void FreeLongRunningServiceData (LongRunningServiceData *data_p)
{
	FreeMemory (data_p);
}

 
static bool CloseLongRunningService (Service *service_p)
{
	bool close_flag = true;
	LongRunningServiceData *data_p = (LongRunningServiceData *) (service_p -> se_data_p);
	ServiceJobSetIterator iterator;
	ServiceJob *job_p = NULL;
	bool loop_flag = true;

	/*
	 * Check whether any jobs are still running.
	 */
	if (service_p -> se_jobs_p)
		{
			InitServiceJobSetIterator (&iterator, service_p -> se_jobs_p);

			job_p = GetNextServiceJobFromServiceJobSetIterator (&iterator);
			loop_flag = (job_p != NULL);

			while (loop_flag)
				{
					OperationStatus status = GetTimedServiceJobStatus (job_p);

					if (status == OS_PENDING || status == OS_STARTED)
						{
							close_flag = false;
							loop_flag = false;
						}
					else
						{
							job_p = GetNextServiceJobFromServiceJobSetIterator (&iterator);

							if (!job_p)
								{
									loop_flag = false;
								}
						}
				}
		}

	if (close_flag)
		{
			FreeLongRunningServiceData (data_p);
		}

	return close_flag;
}
 
 
static const char *GetLongRunningServiceName (const Service * UNUSED_PARAM (service_p))
{
	return "Long Running service";
}


static const char *GetLongRunningServiceDescription (const Service * UNUSED_PARAM (service_p))
{
	return "A service to test long-running asynchronous services";
}


static const char *GetLongRunningServiceAlias (const Service * UNUSED_PARAM (service_p))
{
	return "example" SERVICE_GROUP_ALIAS_SEPARATOR "run";
}

static ParameterSet *GetLongRunningServiceParameters (Service *service_p, DataResource * UNUSED_PARAM (resource_p), User * UNUSED_PARAM (user_p))
{
	ParameterSet *param_set_p = AllocateParameterSet ("LongRunning service parameters", "The parameters used for the LongRunning service");
	
	if (param_set_p)
		{
			/*
			 * We will have a single parameter specifying the number of jobs to run.
			 */
			Parameter *param_p = NULL;
			LongRunningServiceData *data_p = (LongRunningServiceData *) (service_p -> se_data_p);

			if ((param_p = EasyCreateAndAddUnsignedIntParameterToParameterSet (service_p -> se_data_p, param_set_p, NULL, LRS_NUMBER_OF_JOBS.npt_name_s, "Number of jobs", "Number of jobs to run",  & (data_p -> lsd_default_number_of_jobs), PL_ALL)) != NULL)
				{
					param_p -> pa_required_flag = true;

					if ((param_p = EasyCreateAndAddSignedIntParameterToParameterSet (service_p -> se_data_p, param_set_p, NULL, LRS_MIN_DURATION.npt_type, LRS_MIN_DURATION.npt_name_s, "Minimum time", "Minimum duration of each job",  NULL, PL_ALL)) != NULL)
						{
							return param_set_p;
						}
				}

			FreeParameterSet (param_set_p);
		}		/* if (param_set_p) */
		
	return NULL;
}


static bool GetLongRunningServiceParameterTypesForNamedParameters (const Service *service_p, const char *param_name_s, ParameterType *pt_p)
{
	bool success_flag = false;

	if (strcmp (param_name_s, LRS_NUMBER_OF_JOBS.npt_name_s) == 0)
		{
			*pt_p = LRS_NUMBER_OF_JOBS.npt_type;
			success_flag = true;
		}
	else if (strcmp (param_name_s, LRS_MIN_DURATION.npt_name_s) == 0)
		{
			*pt_p = LRS_MIN_DURATION.npt_type;
			success_flag = true;
		}

	return success_flag;
}

static void ReleaseLongRunningServiceParameters (Service * UNUSED_PARAM (service_p), ParameterSet *params_p)
{
	FreeParameterSet (params_p);
}


static json_t *GetLongRunningResultsAsJSON (Service *service_p, const uuid_t job_id)
{
	GrassrootsServer *grassroots_p = GetGrassrootsServerFromService (service_p);
	JobsManager *jobs_mananger_p = GetJobsManager (grassroots_p);
	TimedServiceJob *job_p = (TimedServiceJob *) GetServiceJobFromJobsManager (jobs_mananger_p, job_id);
	json_t *results_array_p = NULL;


	if (job_p)
		{
			json_error_t error;
			json_t *result_p = json_pack_ex (&error, 0, "{s:i,s:i}", "start", job_p -> tsj_interval_p -> ti_start, "end", job_p -> tsj_interval_p -> ti_end);

			if (result_p)
				{
					json_t *resource_json_p = GetDataResourceAsJSONByParts (PROTOCOL_INLINE_S, NULL, "Long Runner", result_p);

					if (resource_json_p)
						{
							json_decref (result_p);

							results_array_p = json_array ();

							if (results_array_p)
								{
									if (json_array_append_new (results_array_p, result_p) != 0)
										{
											json_decref (result_p);
											json_decref (results_array_p);
											results_array_p = NULL;
										}
								}

						}
				}

		}		/* if (job_p) */
	else
		{
			char job_id_s [UUID_STRING_BUFFER_SIZE];

			ConvertUUIDToString (job_id, job_id_s);
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get job data for \"%s\"", job_id_s);
		}

	return results_array_p;
}


/*
 * This is where we create our TimedServiceJob structures prior to running the Service.
 */
static ServiceJobSet *GetServiceJobSet (Service *service_p, const uint32 num_jobs, const int32 min_duration)
{
	/*
	 * If we were just runnig a single generic ServiceJob, we could use the
	 * AllocateSimpleServiceJobSet() function. However we need multiple custom
	 * ServiceJobs, so we need to build these.
	 */
	ServiceJobSet *jobs_p = AllocateServiceJobSet (service_p);

	if (jobs_p)
		{
			uint32 i = 0;
			bool loop_flag = (i < num_jobs);
			bool success_flag = true;

			while (loop_flag && success_flag)
				{
					TimedServiceJob *job_p = NULL;
					char job_name_s [256];
					char job_description_s [256];

					/*
					 * Get a duration for our task that is between 1 and 60 seconds.
					 */
					const int duration = min_duration + (rand () % 60);

					sprintf (job_name_s, "job " INT32_FMT, i);
					sprintf (job_description_s, "duration " SIZET_FMT, (size_t) duration);

					job_p = AllocateTimedServiceJob (service_p, job_name_s, job_description_s, duration);

					if (job_p)
						{
							if (AddServiceJobToService (service_p, (ServiceJob *) job_p))
								{
									++ i;
									loop_flag = (i < num_jobs);
								}		/* if (AddServiceJobToService (service_p, (ServiceJob *) job_p)) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add TimedServiceJob to ServiceJobSet");
									FreeTimedServiceJob ((ServiceJob *) job_p);
									success_flag = false;
								}

						}		/* if (job_p) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate TimedServiceJob");
							success_flag = false;
						}

				}		/* while (loop_flag && success_flag) */

			if (!success_flag)
				{
					FreeServiceJobSet (jobs_p);
					jobs_p = NULL;
				}

		}		/* if (jobs_p) */

	return jobs_p;
}


static ServiceJobSet *RunLongRunningService (Service *service_p, ParameterSet *param_set_p, User * UNUSED_PARAM (user_p), ProvidersStateTable * UNUSED_PARAM (providers_p))
{
	const uint32 *num_tasks_p = NULL;

	if (GetCurrentUnsignedIntParameterValueFromParameterSet (param_set_p, LRS_NUMBER_OF_JOBS.npt_name_s, &num_tasks_p))
		{
			if (num_tasks_p != NULL)
				{
					if (*num_tasks_p > 0)
						{
							const int32 *min_duration_p = NULL;

							GetCurrentSignedIntParameterValueFromParameterSet (param_set_p, LRS_MIN_DURATION.npt_name_s, &min_duration_p);

							service_p -> se_jobs_p = GetServiceJobSet (service_p, *num_tasks_p, min_duration_p ? *min_duration_p : 1);

							if (service_p -> se_jobs_p)
								{
									ServiceJobSetIterator iterator;
									GrassrootsServer *grassroots_p = GetGrassrootsServerFromService (service_p);
									JobsManager *jobs_manager_p = GetJobsManager (grassroots_p);
									TimedServiceJob *job_p = NULL;
									int32 i = 0;
									bool loop_flag;

									InitServiceJobSetIterator (&iterator, service_p -> se_jobs_p);
									job_p = (TimedServiceJob *) GetNextServiceJobFromServiceJobSetIterator (&iterator);

									loop_flag = (job_p != NULL);

									while (loop_flag)
										{
											StartTimedServiceJob (job_p);

											SetServiceJobStatus (& (job_p -> tsj_job), GetTimedServiceJobStatus ((ServiceJob *) job_p));

											job_p -> tsj_added_flag = true;


											if (!AddServiceJobToJobsManager (jobs_manager_p, job_p -> tsj_job.sj_id, (ServiceJob *) job_p))
												{
													char job_id_s [UUID_STRING_BUFFER_SIZE];

													ConvertUUIDToString (job_p -> tsj_job.sj_id, job_id_s);
													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add job \"%s\" to JobsManager", job_id_s);

													job_p -> tsj_added_flag = false;
												}

											job_p = (TimedServiceJob *) GetNextServiceJobFromServiceJobSetIterator (&iterator);

											if (job_p)
												{
													++ i;
												}
											else
												{
													loop_flag = false;
												}

										}		/* while (loop_flag) */

								}		/* if (service_p -> se_jobs_p) */

						}
				}


		}		/* if (GetParameterValueFromParameterSet (param_set_p, TAG_LONG_RUNNING_NUM_JOBS, &param_value, true)) */


	return service_p -> se_jobs_p;
}


static ParameterSet *IsFileForLongRunningService (Service * UNUSED_PARAM (service_p), DataResource * UNUSED_PARAM (resource_p), Handler * UNUSED_PARAM (handler_p))
{
	return NULL;
}


static OperationStatus GetLongRunningServiceStatus (Service *service_p, const uuid_t job_id)
{
	OperationStatus status = OS_ERROR;
	GrassrootsServer *grassroots_p = GetGrassrootsServerFromService (service_p);
	JobsManager *jobs_manager_p = GetJobsManager (grassroots_p);
	ServiceJob *job_p = GetServiceJobFromJobsManager (jobs_manager_p, job_id);

	if (job_p)
		{
			status = GetTimedServiceJobStatus (job_p);
		}		/* if (job_p) */
	else
		{
			char job_id_s [UUID_STRING_BUFFER_SIZE];

			ConvertUUIDToString (job_id, job_id_s);
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get job data for \"%s\"", job_id_s);
		}

	return status;
}


static void StartTimedServiceJob (TimedServiceJob *job_p)
{
	TimeInterval *ti_p = job_p -> tsj_interval_p;

	time (& (ti_p -> ti_start));
	ti_p -> ti_end = (ti_p -> ti_start) + (ti_p -> ti_duration);

	SetServiceJobStatus (& (job_p -> tsj_job), OS_STARTED);
}



static OperationStatus GetTimedServiceJobStatus (ServiceJob *job_p)
{
	TimedServiceJob *timed_job_p = (TimedServiceJob *) job_p;
	OperationStatus status = OS_IDLE;
	TimeInterval * const ti_p = timed_job_p -> tsj_interval_p;

	if (ti_p -> ti_start != ti_p -> ti_end)
		{
			time_t t = time (NULL);

			if (t >= ti_p -> ti_start)
				{
					if (t <= ti_p -> ti_end)
						{
							status = OS_STARTED;
						}
					else
						{
							status = OS_SUCCEEDED;
						}
				}
			else
				{
					status = OS_ERROR;
				}
		}

	SetServiceJobStatus (job_p, status);

	return status;
}



static bool UpdateTimedServiceJob (struct ServiceJob *job_p)
{
	return true;
}


static TimedServiceJob *AllocateTimedServiceJob (Service *service_p, const char * const job_name_s, const char * const job_description_s, const time_t duration)
{
	TimedServiceJob *job_p = NULL;
	TimeInterval *interval_p = (TimeInterval *) AllocMemory (sizeof (TimeInterval));

	if (interval_p)
		{
			job_p = (TimedServiceJob *) AllocMemory (sizeof (TimedServiceJob));

			if (job_p)
				{
					interval_p -> ti_start = 0;
					interval_p -> ti_end = 0;
					interval_p -> ti_duration = duration;

					job_p -> tsj_interval_p = interval_p;
					job_p -> tsj_added_flag = false;

					InitServiceJob (& (job_p -> tsj_job), service_p, job_name_s, job_description_s, UpdateTimedServiceJob, NULL, FreeTimedServiceJob, NULL, LRS_SERVICE_JOB_TYPE_S);

				}		/* if (job_p) */
			else
				{
					FreeMemory (interval_p);
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate TimedServiceJob");
				}

		}		/* if (interval_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate TimeInterval");
		}

	return job_p;
}


static void FreeTimedServiceJob (ServiceJob *job_p)
{
	TimedServiceJob *timed_job_p = (TimedServiceJob *) job_p;

	if (timed_job_p -> tsj_interval_p)
		{
			FreeMemory (timed_job_p -> tsj_interval_p);
		}

	FreeBaseServiceJob (job_p);
}



static json_t *GetTimedServiceJobAsJSON (TimedServiceJob *job_p)
{
	/*
	 * Get the JSON for the ServiceJob base class.
	 */
	json_t *json_p = GetServiceJobAsJSON (& (job_p -> tsj_job), false);

	if (json_p)
		{
			/*
			 * Now we add our extra data which is the start and end time of the TimeInterval
			 * for the given TimedServiceJob.
			 */
			if (json_object_set_new (json_p, LRS_START_S, json_integer (job_p -> tsj_interval_p -> ti_start)) == 0)
				{
					if (json_object_set_new (json_p, LRS_END_S, json_integer (job_p -> tsj_interval_p -> ti_end)) == 0)
						{
							return json_p;
						}		/* if (json_object_set_new (json_p, LRS_END_S, json_integer (job_p -> tsj_interval_p -> ti_end)) == 0) */
					else
						{
							PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, json_p, "Failed to add %s " SIZET_FMT " to json", LRS_END_S, job_p -> tsj_interval_p -> ti_end);
						}

				}		/* if (json_object_set_new (json_p, LRS_START_S, json_integer (job_p -> tsj_interval_p -> ti_start)) == 0) */
			else
				{
					PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, json_p, "Failed to add %s " SIZET_FMT " to json", LRS_END_S, job_p -> tsj_interval_p -> ti_end);
				}

			json_decref (json_p);
		}		/* if (json_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create JSON for TimedServiceJob");
		}

	return NULL;
}





static unsigned char *SerialiseTimedServiceJob (ServiceJob *base_job_p, unsigned int *value_length_p)
{
	TimedServiceJob *job_p = (TimedServiceJob *) base_job_p;
	unsigned char *value_p = NULL;
	json_t *job_json_p = GetTimedServiceJobAsJSON (job_p);

	if (job_json_p)
		{
			char *job_s = json_dumps (job_json_p, JSON_INDENT (2));

			if (job_s)
				{
					/*
					 * include the terminating \0 to make sure
					 * the value as a valid c-style string
					 */
					*value_length_p = strlen (job_s) + 1;
					value_p = (unsigned char *) job_s;
				}		/* if (job_s) */
			else
				{
					char uuid_s [UUID_STRING_BUFFER_SIZE];

					ConvertUUIDToString (job_p -> tsj_job.sj_id, uuid_s);
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "json_dumps failed for \"%s\"", uuid_s);
				}

			json_decref (job_json_p);
		}		/* if (job_json_p) */
	else
		{
			char uuid_s [UUID_STRING_BUFFER_SIZE];

			ConvertUUIDToString (job_p -> tsj_job.sj_id, uuid_s);
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "GetTimedServiceJobAsJSON failed for \"%s\"", uuid_s);
		}

	return value_p;
}


static TimedServiceJob *GetTimedServiceJobFromJSON (Service *service_p, const json_t *json_p)
{
	/* allocate the memory for the TimedServiceJob */
	TimedServiceJob *job_p = (TimedServiceJob *) AllocMemory (sizeof (TimedServiceJob));

	if (job_p)
		{
			/* allocate the memory for the TimeInterval */
			job_p -> tsj_interval_p = (TimeInterval *) AllocMemory (sizeof (TimeInterval));

			if (job_p -> tsj_interval_p)
				{
					GrassrootsServer *grassroots_p = GetGrassrootsServerFromService (service_p);

					job_p -> tsj_job.sj_service_p = service_p;

					/* initialise the base ServiceJob from the JSON fragment */
					if (InitServiceJobFromJSON (& (job_p -> tsj_job), json_p, service_p, grassroots_p))
						{
							/*
							 * We now need to get the start and end times for the TimeInterval
							 * from the JSON.
							 */
							if (GetJSONLong (json_p, LRS_START_S, & (job_p -> tsj_interval_p -> ti_start)))
								{
									if (GetJSONLong (json_p, LRS_END_S, & (job_p -> tsj_interval_p -> ti_end)))
										{
											bool b;
											OperationStatus old_status = GetServiceJobStatus (& (job_p -> tsj_job));

											if (GetJSONBoolean (json_p, LRS_ADDED_FLAG_S, &b))
												{
													job_p -> tsj_added_flag = b;
												}
											else
												{
													job_p -> tsj_added_flag = false;
												}

											/* Update the job status */
											if (old_status == OS_STARTED)
												{
													OperationStatus new_status = GetTimedServiceJobStatus (& (job_p -> tsj_job));

													if (new_status != old_status)
														{
															JobsManager *jobs_manager_p = GetJobsManager (grassroots_p);
															RemoveServiceJobFromJobsManager (jobs_manager_p, job_p -> tsj_job.sj_id, false);
														}
												}

											return job_p;
										}		/* if (GetJSONLong (json_p,  LRS_END_S, & (job_p -> tsj_interval_p -> ti_start))) */
									else
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get %s from JSON", LRS_END_S);
										}

								}		/* if (GetJSONLong (json_p,  LRS_START_S, & (job_p -> tsj_interval_p -> ti_start))) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get %s from JSON", LRS_START_S);
								}
						}		/* if (InitServiceJobFromJSON (& (job_p -> tsj_job), json_p)) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to init ServiceJob from JSON");
							PrintJSONToLog (STM_LEVEL_SEVERE, __FILE__, __LINE__, json_p, "Init ServiceJob failure: ");
						}

				}		/* if (job_p -> tsj_interval_p) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate TimeInterval");
				}

			FreeTimedServiceJob ((ServiceJob *) job_p);
		}		/* if (job_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate TimedServiceJob");
		}

	return NULL;
}



static ServiceJob *BuildTimedServiceJob (Service *service_p, const json_t *service_job_json_p)
{
	return ((ServiceJob* ) GetTimedServiceJobFromJSON (service_p, service_job_json_p));
}


static json_t *BuildTimedServiceJobJSON (Service *service_p, ServiceJob *service_job_p, bool omit_results_flag)
{
	return GetTimedServiceJobAsJSON ((TimedServiceJob *) service_job_p);
}



static void CustomiseTimedServiceJob (Service *service_p, ServiceJob *job_p)
{
	job_p -> sj_update_fn = NULL;
	job_p -> sj_free_fn = NULL;
}



static ServiceMetadata *GetLongRunningServiceMetadata (Service *service_p)
{
	const char *term_url_s = CONTEXT_PREFIX_EDAM_ONTOLOGY_S "operation_0304";
	SchemaTerm *category_p = AllocateSchemaTerm (term_url_s, "Query and retrieval", "Search or query a data resource and retrieve entries and / or annotation.");

	if (category_p)
		{
			ServiceMetadata *metadata_p = AllocateServiceMetadata (category_p, NULL);

			if (metadata_p)
				{
					return metadata_p;
				}		/* if (metadata_p) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate service metadata");
				}

			FreeSchemaTerm (category_p);
		}		/* if (category_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate category term %s for service metadata", term_url_s);
		}

	return NULL;
}

