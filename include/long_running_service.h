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

/**
 * @file
 * @brief
 */
/**@file long_running_service.h
*/ 

#ifndef LONG_RUNNING_SERVICE_H
#define LONG_RUNNING_SERVICE_H

#include "service.h"
#include "library.h"

/*
** Now we use the generic helper definitions above to define LIB_API and LIB_LOCAL.
** LIB_API is used for the public API symbols. It either DLL imports or DLL exports
** (or does nothing for static build)
** LIB_LOCAL is used for non-api symbols.
*/

	/** \example long_running_service.c
	 * This is an example Service showing some commonly needed techniques.
	 *
	 * This service is an example to show how job data can be persisted between separate
	 * requests. It mimics real world jobs by running a user-specified number of jobs that
	 * are have a start and end times, each one an equivalent to a stopwatch.
	 */


/**
 * @file
 * @defgroup example_service The Example Services Module
 * @brief
 */
/**
 * @privatesection
 * @{
 */


#ifdef SHARED_LIBRARY /* defined if LIB is compiled as a DLL */
  #ifdef LONG_RUNNING_LIBRARY_EXPORTS /* defined if we are building the LIB DLL (instead of using it) */
    #define LONG_RUNNING_SERVICE_API LIB_HELPER_SYMBOL_EXPORT
  #else
    #define LONG_RUNNING_SERVICE_API LIB_HELPER_SYMBOL_IMPORT
  #endif /* #ifdef LONG_RUNNING_LIBRARY_EXPORTS */
  #define LONG_RUNNING_SERVICE_LOCAL LIB_HELPER_SYMBOL_LOCAL
#else /* SHARED_LIBRARY is not defined: this means LIB is a static lib. */
  #define LONG_RUNNING_SERVICE_API
  #define LONG_RUNNING_SERVICE_LOCAL
#endif /* #ifdef SHARED_LIBRARY */


/**
 * @}
 */


#ifdef __cplusplus
extern "C"
{
#endif


/**
 * Get the ServicesArray containing the example Service.
 *
 * @param user_p The User for the user trying to access the services.
 * This can be <code>NULL</code>.
 * @return The ServicesArray containing all of the example Service or
 * <code>NULL</code> upon error.
 * @ingroup example_service
 */
LONG_RUNNING_SERVICE_API ServicesArray *GetServices (User *user_p, GrassrootsServer *grassroots_p);


/**
 * Free the ServicesArray containing the example Service.
 *
 * @param services_p The ServicesArray to free.
 * @ingroup example_service
 */
LONG_RUNNING_SERVICE_API void ReleaseServices (ServicesArray *services_p);

#ifdef __cplusplus
}
#endif



#endif		/* #ifndef LONG_RUNNING_SERVICE_H */
