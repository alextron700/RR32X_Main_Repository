
; hello world test
define Terminal = 0x83764625

STJ print_string
JMP
String_table:
TABLE[P"Hello, World!"]

print_string:
	ADD R00, 0, String_table
	ADD R01, 0, -1
	XOR R02, R02, R02
loop:
		BYTESEL R01
		LDM R02(W08), M$[(R00)(W08)]
		MULL R03, 8, R02
		SHL.C R02,R02, R03
		BYTESEL 0
		STM M$[Terminal], R02(W08)
		AND R03, R01, 0x03
		STJ dont_advance
		JNE R03, 0
			ADD R00, R00, 1
dont_advance:
		SUB R01, R01, 1
	STJ loop
	JNE R03, 0



; ABI Demo, simple
define a = 15
define b = 12
STJ Main
JMP

ArgumentTable:
TABLE[0,0,0,0]
ReturnTable:
TABLE[0,0,0,0]
Add:
	ADD R02, 0, ArgumentTable
	LDM R00 M$[R02 + 0]
	LDM R01 M$[R02 + 1]
	ADD R02, R00, R01
	STM M$[ReturnTable] R02
	RET

Main:
	LDM R00 M$[a]
	LDM R01 M$[b]
	ADD R02, 0, ArgumentTable
	STM M$[R02 + 0], R00
	STM M$[R02 + 1], R01
	CAL Add
	LDM R00 M$[ReturnTable]


; ABI Demo, more properly:

define a = 15
define b = 12

STJ Main
JMP

ArgumentTable:
TABLE[0,0,0,0]
ReturnTable:
TABLE[0,0,0,0]

CurrentBank:
TABLE[0]
Add:
	EAM.SET R01
	LDM R04, M$[R00 + 0]
	LDM R05, M$[R00 + 1]
	ADD R06, R04, R05
	EAM.SET R02
	STM M$[R03 + 0], R06
	RET


Main:
	LDM R00 M$[a]
	LDM R01 M$[b]
	ADD R02, 0, ArgumentTable
	STM M$[R02 + 0], R00
	STM M$[R02 + 1], R01
	EAM.GET R00
	STM M$[Current_Bank], R00

;load pointers to argument and return table pointers.
	EAM.SET ArgumentTable.
	ADD R00,0, ArgumentTable
	EAM.GET R01

	ADD R02, 0, ReturnTable
	EAM.SET ReturnTable	
	EAM.GET R03
; actually call Add 
	EAM.SET Add
	CAL Add
	LDM R00 M$[ReturnTable]
