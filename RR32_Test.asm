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
