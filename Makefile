# Examples:
#
# make checkout			# Checkout core modules
# make public-checkout		# Checkout core modules from public svn (login not required)
# make checkout-soar2d		# Checkout Soar2D (not required, here for example)
# make public-checkout-soar2d	# Checkout Soar2D from public svn (not required, here for example)
# make update			# svn update everywhere
# make status			# svn status everywhere (there is no "make commit")
# make				# build all checked-out modules (there is no "make install")
# make clean			# clean all checked-out modules

all:
	cd Core && scons

noscu:
	cd Core && scons --no-scu

static:
	cd Core && scons --static

up: update
update:
	find . -maxdepth 1 -type d -not -name ".*" -exec echo {} \; -exec svn update {} \;

status:
	find . -maxdepth 1 -type d -not -name ".*" -exec echo {} \; -exec svn status {} \;

clean:
	cd Core && scons -c
	cd Core && scons --static -c

public-checkout:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Core
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Java
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Python

checkout:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Core
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Java
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Python

public-checkout-core:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Core

checkout-core:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Core

public-checkout-filterc:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/FilterC

checkout-filterc:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/FilterC

public-checkout-javamissionaries:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/JavaMissionaries

checkout-javamissionaries:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/JavaMissionaries

public-checkout-javatoh:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/JavaTOH

checkout-javatoh:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/JavaTOH

public-checkout-loggerjava:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/LoggerJava

checkout-loggerjava:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/LoggerJava

public-checkout-quicklink:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/QuickLink

checkout-quicklink:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/QuickLink

public-checkout-soar2d:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Soar2D

checkout-soar2d:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Soar2D

public-checkout-soar2soar:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Soar2Soar

checkout-soar2soar:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Soar2Soar

public-checkout-soartextio:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/SoarTextIO

checkout-soartextio:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/SoarTextIO

public-checkout-sproom:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Sproom

checkout-sproom:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Sproom

public-checkout-tcl:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/Tcl

checkout-tcl:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/Tcl

public-checkout-tohsml:
	svn checkout http://soar.googlecode.com/svn/trunk/SoarSuite/TOHSML

checkout-tohsml:
	svn checkout https://soar.googlecode.com/svn/trunk/SoarSuite/TOHSML
