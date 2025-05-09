#
# Copyright (c) 2024 Analog Devices Inc.
#
# This file is part of Scopy
# (see https://www.github.com/analogdevicesinc/scopy).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

if(DEFINED __INCLUDED_SCOPY_ABOUT_CMAKE)
	return()
endif()
set(__INCLUDED_SCOPY_ABOUT_CMAKE TRUE)

# Get the GIT hash of the latest commit
include(FindGit OPTIONAL)
if(GIT_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE SCOPY_GIT_REPO
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE SCOPY_VERSION_GIT
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	execute_process(
		COMMAND ${GIT_EXECUTABLE} config --get remote.origin.url
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE SCOPY_REMOTE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE SCOPY_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	execute_process(
		COMMAND ${GIT_EXECUTABLE} log --oneline -n 150
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_LOG
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	string(REPLACE "\n" "<br>" SCOPY_GIT_LOG ${GIT_LOG})

endif()

function(configure_about ABOUT_RESOURCES_DIR)
	string(TIMESTAMP TODAY "%Y-%m-%d")
	string(TIMESTAMP NOW "%H:%M:%S")
	cmake_host_system_information(RESULT BUILD_HOST QUERY HOSTNAME)

	set(BUILD_INFO)
	if(DEFINED ENV{BUILD_HOST})
		if($ENV{USERNAME} STREQUAL "github-actions")
			set(CI_URL $ENV{GITHUB_SERVER_URL})
			set(CI_API_URL $ENV{GITHUB_API_URL})
			set(CI_ACCOUNT_NAME $ENV{GITHUB_REPOSITORY_OWNER})
			set(CI_PROJECT_NAME $ENV{GITHUB_REPOSITORY})
			set(CI_RUN_ID $ENV{GITHUB_RUN_ID})
			set(CI_RUN_NUMBER $ENV{GITHUB_RUN_NUMBER})
			set(CI_JOB_ID $ENV{GITHUB_RUN_ID})
			set(CI_JOB_NAME $ENV{GITHUB_JOB})
			set(CI_JOB_NUMBER $ENV{GITHUB_RUN_NUMBER})
			set(CI_JOB_LINK
			    $ENV{GITHUB_SERVER_URL}/$ENV{GITHUB_REPOSITORY_OWNER}/$ENV{GITHUB_REPOSITORY}/actions/runs/$ENV{GITHUB_RUN_ID}
			)
		else($ENV{USERNAME} STREQUAL "azure-pipelines")
			set(CI_URL $ENV{BUILD_REPO_URL})
			set(CI_API_URL "-")
			set(CI_ACCOUNT_NAME $ENV{ACCOUNT_NAME})
			set(CI_PROJECT_NAME $ENV{PROJECT_NAME})
			set(CI_RUN_ID $ENV{RUN_ID})
			set(CI_RUN_NUMBER $ENV{RUN_NUMBER})
			set(CI_JOB_ID $ENV{JOB_ID})
			set(CI_JOB_NAME $ENV{JOB_NAME})
			set(CI_JOB_NUMBER "-")
			set(CI_JOB_LINK "-")
		endif()

		set(BUILD_INFO ${BUILD_INFO}Built\ on\ $ENV{USERNAME}\n)
		set(BUILD_INFO ${BUILD_INFO}url:\ ${CI_URL}\n)
		set(BUILD_INFO ${BUILD_INFO}api_url:\ ${CI_API_URL}\n)
		set(BUILD_INFO ${BUILD_INFO}acc_name:\ ${CI_ACCOUNT_NAME}\n)
		set(BUILD_INFO ${BUILD_INFO}prj_name:\ ${CI_PROJECT_NAME}\n)
		set(BUILD_INFO ${BUILD_INFO}run_id:\ ${CI_RUN_ID}\n)
		set(BUILD_INFO ${BUILD_INFO}run_nr:\ ${CI_RUN_NUMBER}\n)
		set(BUILD_INFO ${BUILD_INFO}job_id:\ ${CI_JOB_ID}\n)
		set(BUILD_INFO ${BUILD_INFO}job_name:\ ${CI_JOB_NAME}\n)
		set(BUILD_INFO ${BUILD_INFO}job_nr:\ ${CI_JOB_NUMBER}\n)
		set(BUILD_INFO ${BUILD_INFO}job_link:\ ${CI_JOB_LINK}\n)

		if(EXISTS ${CMAKE_SOURCE_DIR}/build-status)
			message("build-status found in ${CMAKE_SOURCE_DIR}.. populating")
			file(READ ${CMAKE_SOURCE_DIR}/build-status SCOPY_BUILD_STATUS_INFO)
		endif()
	else()
		set(BUILD_INFO ${BUILD_INFO} "Built locally")
	endif()

	message(STATUS "AboutPage Info: " ${BUILD_INFO})
	# TODO: Pack these in a GLOB and run foreach

	set(ABOUT_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${ABOUT_RESOURCES_DIR})
	set(ABOUT_RESOURCES_TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
	configure_file(${ABOUT_RESOURCES_DIR}/buildinfo.html.cmakein ${ABOUT_RESOURCES_TARGET_DIR}/buildinfo.html)
	configure_file(${ABOUT_RESOURCES_DIR}/scopy_osp.html.cmakein ${ABOUT_RESOURCES_TARGET_DIR}/scopy_osp.html)
	configure_file(${ABOUT_RESOURCES_DIR}/credits.html.cmakein ${ABOUT_RESOURCES_TARGET_DIR}/credits.html)
	configure_file(${ABOUT_RESOURCES_DIR}/about.html.cmakein ${ABOUT_RESOURCES_TARGET_DIR}/about.html)
	configure_file(${ABOUT_RESOURCES_DIR}/scopy_home.html.cmakein ${ABOUT_RESOURCES_TARGET_DIR}/scopy_home.html)

	set(ABOUT_HTML_SOURCES
	    ${ABOUT_RESOURCES_TARGET_DIR}/buildinfo.html
	    ${ABOUT_RESOURCES_TARGET_DIR}/scopy_osp.html
	    ${ABOUT_RESOURCES_TARGET_DIR}/credits.html
	    ${ABOUT_RESOURCES_TARGET_DIR}/about.html
	    ${ABOUT_RESOURCES_TARGET_DIR}/scopy_home.html
	    ${ABOUT_RESOURCES_TARGET_DIR}/license.html
	)

	set(ABOUT_HTML_QRC_SOURCES)
	foreach(file ${ABOUT_HTML_SOURCES})
		get_filename_component(file_name ${file} NAME)
		set(ABOUT_HTML_QRC_SOURCES "${ABOUT_HTML_QRC_SOURCES}\n<file>${file_name}</file>")
	endforeach()

	configure_file(${ABOUT_RESOURCES_DIR}/aboutpage.qrc.cmakein ${ABOUT_RESOURCES_TARGET_DIR}/aboutpage.qrc @ONLY)
	message(STATUS "built about page in - " ${ABOUT_RESOURCES_TARGET_DIR}/aboutpage.qrc)

endfunction()
