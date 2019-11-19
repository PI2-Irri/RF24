#############################################################################
#
# Makefile for librf24 PI2-Irri Control Hub on RPi
#
# License: GPL (General Public License)
#
# Description:
# ------------
# use make all and make install to install the examples
#

ifeq ($(wildcard ../Makefile.inc), )
	$(error Configuration not found. Run ./configure first)
endif

include ../Makefile.inc

# define all programs
PROGRAMS = central_demo

include Makefile.controlHub
