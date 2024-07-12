Technical documentation
-----------------------
Product name        : arKad
Current Version     : 1.2

Supported platforms : Linux + GTK+ 2.0 + Cairo + Pango
					  Windows
Language	    	: English.

Author              : Lino
Website             : https://www.arkademu.it


*** please do not send any questions on how to use or play games. ***


Emulation status
----------------
CPS3:
	- Hitachi SH2  : CPU support (90%)
	- Timers
		* All timers are emulated in both prescalar and count-up mode.
	- EEPROM
		* Write and Read operations are emulated.
	- CDROM
		* Partialy emulaated
	- GFX
		* All layers work and Sprites support is working.
	- Interrupts
		* TIMERs interrupts are emulated,
		* DMAs
		* VBLANK


BlackTiger PCB
	- Z80 CPU support (95%)
	- Yamaha 2203 OPN (85%)
	- GFX
		* All layers work and Sprites support is working.
	- Interrupts
		* VBLANK


Tehkan World cup
	- Z80 CPU support (95%)
	- Interrupts
		* VBLANK
		* NMI


PSX/PS1
	- R3000 CPU support (95%)
	- CDROM
		* Partialy emulaated
	- Interrupts
		* VBLANK
		* TIMERs interrupts are emulated,
		* DMAs



Special features
----------------

Acknowledgements (in no particular order)
-----------------------------------------

Keys
-----------------------------------------
Left Key
Right Key
Up Key
Down Key

F2 	Reset
F3	Stop
F4	Step by Step on Debugger Window
F5 	Run
F7	Run to Cursor on Debugger Window
F8 	Steo Over
F9	Add breakpoint


Tested games
-----------------------------------------
sfiii3_japan_nocd.29f400.u2			SF3
redearth_euro.29f400.u2 + (cdrom)
bdu-01a.5e							(BlackTiger)
twc-4.bin							(Tehkan World cup)
