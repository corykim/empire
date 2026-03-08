################################################################################
################################################################################
#
# TRS-80 Empire game makefile.
#
################################################################################
################################################################################

#
# Top-level make targets.
#

all: empire


#
# Build rules.
#

empire: attack.cpp cpu.cpp economy.cpp empire.cpp grain.cpp investments.cpp population.cpp ui.cpp
	g++ -g -std=c++17 -lncurses -lm -o empire $^

