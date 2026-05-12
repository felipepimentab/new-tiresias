#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources_ifdef(CONFIG_NRF5340_AUDIO_CS47L63_DRIVER
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cs47l63_comm.c
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/adau1787.c
)

# Make header files available to the rest of the project
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})