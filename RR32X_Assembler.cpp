// RR32X_Assembler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
/*
* 
SAMPLE.asm
							define Terminal = 0x83764625

0x1A400000 00000006          STJ print_string ; branch to print_string subroutine
0x1E000000                   JMP
                  StringTable:
0x48656C6C 6F205772 6C642100 TABLE[P"Hello World!"]

print_string:
0x0200C0C0 00000000 00000003  ADD R00, 0, String_Table ; *string
0x0201C0C0 00000000 FFFFFFFF  ADD R01, 0, -1 ; byte_portion
0x0A020202                    XOR R02, R02, R02
                         loop:
0x3F000100                    BYTESEL R01
0x18820080                    LDM R02(W08) M$[(R00)(W08)]
0x1403C001 00000008           MULL R03, 8, R01
0x0E020203                    SHL.C R02,R02,R03
0x3F00C000 00000000           BYTESEL 0 ;set to least significant byte
0x19400082 83764625           STM M$[Terminal] R02(W08)
0x0603020C 00000003           AND R03, R01, 0x03 ; byte_portion & 0x03
0x1A400000 0000001E           STJ dont_advance ; don't advance if byte_portion & 0x03 != 0
0x1E0503C0 00000000           JNE R03, 0 ; check if byte_portion & 0x03 != 0
0x020000C0 00000001           ADD R00, R00, 1 ; *string++
                 dont_advance:
0x040101C0 00000001           SUB R01, R01, 1 ; byte_portion--
0x1A40000 0000'000D           STJ loop ; go back to the top of the loop
0x1E070000			          JMP
*/
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <bit>
enum tokenType{
	OPCODE,
	REGISTER,
	IMMEDIATE_HEX,
	IMMEDIATE_DEC,
	COMMENT,
	WIDTH_SPECIFIER,
	JUMP_INSTRUCTION,
	LABEL_DEFINE,
	LABEL_REFERENCE,
	MEMORY_ADDRESS,
	DATA_TABLE,
	CLOSE_SQUARE_BRACKET,
	OPEN_SQUARE_BRACKET,
	LABELED_CONSTANT_DEFINE,
	STRING_START,
	TINY_STRING,
	PACKED_TINY_STRING,
	STRING_END,
	STRING_MID_QUOTE,
	PACKED_STRING_START,
	IMMEDIATE_FLOAT,
	PLUS,
	EQUALS,
	MINUS,
	COLON,
	PC,
	OPEN_ROUND_BRACKET,
	CLOSE_ROUND_BRACKET,
	BOUNCEBACK_MODE,
};
const std::map<tokenType, std::string> token_names = {
	{OPCODE,"Opcode "},
	{REGISTER,"Register "},
	{IMMEDIATE_HEX,"Immediate (Hex) "},
	{IMMEDIATE_DEC,"Immediate (Dec) "},
	{COMMENT,"Comment "},
	{WIDTH_SPECIFIER,"Width Specifier "},
	{JUMP_INSTRUCTION,"Jump Syntax "},
	{LABEL_DEFINE,"Label Definition "},
	{LABEL_REFERENCE,"Label Reference "},
	{MEMORY_ADDRESS,"Memory Address "},
	{DATA_TABLE,"Data Table "},
	{CLOSE_SQUARE_BRACKET,"Closing Square Bracket ']' "},
	{OPEN_SQUARE_BRACKET,"Open Square Bracket '[' "},
	{LABELED_CONSTANT_DEFINE,"'define' Directive "},
	{STRING_START,"Start of String "},
	{TINY_STRING,"Small String "},
	{PACKED_TINY_STRING,"Small Packed String "},
	{STRING_END,"End of String "},
	{STRING_MID_QUOTE, "Double Quote '\"' "},
	{IMMEDIATE_FLOAT,"Immediate (Float) "},
	{PLUS,"Plus '+' "},
	{EQUALS,"Equals '=' "},
	{MINUS,"Minus '-' "},
	{COLON,"Colon ':' "},
	{PC,"PC "},
	{OPEN_ROUND_BRACKET,"Open Round Bracket '(' "},
	{CLOSE_ROUND_BRACKET,"Close Round Bracket ')' "},
	{BOUNCEBACK_MODE,"Bounceback token"}
};
// Operand groups for instruction encoding
// Uppers: Present, Lowers: Not Present
enum OperandGroup{
dxy,
Dxy,
dXy,
DXy,
dxY,
DxY,
dXY,
DXY
};
enum stringResult {
	VALID_STRING,
	INVALID_STRING,
	STRING_NOT_FOUND
};
struct Token {
	tokenType type = OPCODE;
	std::string value = "NULL";
};
struct Instruction {
	std::string opcode;
	uint8_t encoding;
	OperandGroup operandGroup;
	bool specialDestination = false; // Indicates if the instruction has a special destination requirement
	bool specialX = false; // Indicates if the instruction has a special X requirement
	bool specialY = false; // Indicates if the instruction has a special Y requirement
};
// Define the instruction set with opcodes, encodings, and operand groups
const std::vector<Instruction> optable = {
	{"EAM.SET",0x00,dXy},
	{"IVR.SET",0x01,DXy},
	{"ADD",0x02,DXY},
	{"ADD.C",0x03,DXY},
	{"SUB",0x04,DXY},
	{"SUB.C",0x05,DXY},
	{"AND",0x06,DXY},
	{"NAND",0x07,DXY},
	{"OR",0x08,DXY},
	{"NOR",0x09,DXY},
	{"XOR",0x0A,DXY},
	{"XNOR",0x0B,DXY},
	{"NOT",0x0C,DXY},
	{"NEG",0x0D,DXY},
	{"SHL.C",0x0E,DXY},
	{"SHR.C",0x0F,DXY},
	{"SHL.A",0x10,DXY },
	{"SHR.A",0x11,DXY },
	{"ROL",0x12,DXY },
	{"ROR",0x13,DXY },
	{"MULL",0x14,DXY },
	{"MULH",0x15,Dxy },
	{"DIV",0x16,DXY },
	{"REM",0x17,Dxy },
	{"LDM",0x18,DXy,false,true,false},
	{"STM",0x19,dXY,false,false,true },
	{"STJ",0x1A,dXy,false,true,false },
	{"NIL",0x1E,dxy }, // this is for the OPCODE that covers all the jump variants. The actual jump instructions are in the validJumpInstructions table below.
	{"JLT",0x1E,dXY }, // this exists so that the future encoder can use the one lookup table to find the jump instruction encoding, and then use the subcode to determine which jump variant it is. (handled separately) 
	{"JEQ",0x1E,dXY },
	{"JLE",0x1E,dXY },
	{"JGT",0x1E,dXY },
	{"JNE",0x1E,dXY },
	{"JGE",0x1E,dXY },
	{"JMP",0x1E,dxy },
	{"JCC",0x1E,dxy },
	{"JLT.U",0x1E,dXY },
	{"JEQ.U",0x1E,dXY },
	{"JLE.U",0x1E,dXY },
	{"JGT.U",0x1E,dXY },
	{"JNE.U",0x1E,dXY },
	{"JGE.U",0x1E,dXY },
	{"JCS",0x1E,dxy },
	{"JVF",0x1E,dXy },
	{"JLT.F",0x1E,dXY },
	{"JEQ.F",0x1E,dXy },
	{"JLE.F",0x1E,dXY },
	{"JGT.F",0x1E,dXY },
	{"JNE.F",0x1E,dXY },
	{"JGE.F",0x1E,dXY },
	{"JFN",0x1E,dXy },
	{"UNUSED" ,0x1E,dxy },
	{"JLT.FA",0x1E,dXY },
	{"JEQ.FA",0x1E,dXy },
	{"JLE.FA",0x1E,dXY },
	{"JGT.FA",0x1E,dXY },
	{"JNE.FA",0x1E,dXY },
	{"JGE.FA",0x1E ,dXY},
	{"UNUSED",0x1E,dxy},
	{"CAL",0x1F,dXy,false,true,false},
	{"RET",0x23,dxy },
	{"IRET",0x24,dxy },
	{"FPU.I2F",0x25,DXy },
	{"FPU.ADD",0x26,DXY },
	{"FPU.SUB",0x27,DXY },
	{"FPU.MUL",0x28,DXY },
	{"FPU.DIV",0x29,DXY },
	{"FPU.F2I",0x2A,DXy },
	{"DMA.SRCBANK",0x2B,dXy },
	{"DMA.DSTBANK",0x2C,dXy },
	{"DMA.SRCSTRIDE",0x2D,dXy },
	{"DMA.DSTSTRIDE",0x2E,dXy },
	{"DMA",0x2F,DXY },
	{"DMA.GETCOUNT",0x30,Dxy },
	{"VOP.BANK",0x31,dXy },
	{"VOP.STRIDE",0x32,dXy },
	{"VOP.COUNT",0x33,dXy },
	{"VOP.SET",0x34,dxy },
	{"LDVA",0x35,dXy,false,true,false},
	{"VOP.GO",0x36,dXy,false,true,false },
	{"STVA",0x37,dXy,false,true,false },
	{"VOP.GETGO",0x38,Dxy },
	{"EAM.GET",0x39,Dxy },
	{"IVR.GET",0x3A,DXy },
	{"IVR.GETBANK",0x3B,DXy },
	{"BRK",0x3C,DXY,true,false,false },
	{"CMS",0x3D,DXY,true,true,true },
	{"HLT",0x3E,dxy },
	{"BYTESEL",0x3F,dXy}
};
// all the jump variants and how their conditions are encoded. The syntax may be odd, so here's a breakdown of the syntax:
// JLT = Jump if Less Than
// JEQ = Jump if Equal
// JLE = Jump if Less than or Equal
// JGT = Jump if Greater Than
// JNE = Jump if Not Equal
// JGE = Jump if Greater than or Equal
// JMP = Jump Unconditionally
// JCC = Jump if Carry Clear
// JCS = Jump if Carry Set
// JVF = Jump if floating point valid (Not NaN)
// JFN = Jump if floating point finite (Not Infinity)
// The .U variants are for unsigned comparisons, .F is for comparisons with floating point numbers, and .FA is for comparisons with floating point numbers using absolute values. 
const std::vector<Instruction> validJumpInstructions = {
	{"NIL",0x00,dxy }, // this is how individual jump instructions are encoded as a sub-code of the jump instruction.  
	{"JLT",0x01,dXY },
	{"JEQ",0x02,dXY },
	{"JLE",0x03,dXY },
	{"JGT",0x04,dXY },
	{"JNE",0x05,dXY },
	{"JGE",0x06,dXY },
	{"JMP",0x07,dxy },
	{"JCC",0x08,dxy },
	{"JLT.U",0x09,dXY },
	{"JEQ.U",0x0A,dXY },
	{"JLE.U",0x0B,dXY },
	{"JGT.U",0x0C,dXY },
	{"JNE.U",0x0D,dXY },
	{"JGE.U",0x0E,dXY },
	{"JCS",0x0F,dxy },
	{"JVF",0x10,dXy },
	{"JLT.F",0x11,dXY },
	{"JEQ.F",0x12,dXy },
	{"JLE.F",0x13,dXY },
	{"JGT.F",0x14,dXY },
	{"JNE.F",0x15,dXY },
	{"JGE.F",0x16,dXY },
	{"JFN",0x17,dXy },
	{"UNUSED" ,0x18,dxy },
	{"JLT.FA",0x19,dXY },
	{"JEQ.FA",0x1A,dXy },
	{"JLE.FA",0x1B,dXY },
	{"JGT.FA",0x1C,dXY },
	{"JNE.FA",0x1D,dXY },
	{"JGE.FA",0x1E ,dXY},
	{"UNUSED",0x1F,dxy}
};
// Function to find an instruction in the optable by opcode
const Instruction* findInstruction(const std::string& opcode) {
	for (const auto& instruction : optable) {
		if (opcode == instruction.opcode) {
			return &instruction;
		}
	}
	return nullptr; // Return nullptr if not found
}
// find a jump condition from the table of valid conditions
bool isJumpInstruction(const std::string& opcode) {
	for (const auto& instruction : validJumpInstructions) {
		if (opcode == instruction.opcode) {
			return true;
		}
	}
	return false;
}
// This here is for later
// Function to compare two strings case-insensitively
bool compareStrings(const std::string& str1, const std::string& str2) {
	if (str1.length() != str2.length()) {
		return false;
	}
	for (size_t i = 0; i < str1.length(); ++i) {
		if (tolower(str1[i]) != tolower(str2[i])) {
			return false;
		}
	} 
	return true;
}
// Function to check if a string is a valid width specifier
bool isWidthSpecifier(const std::string& str) {
	if (str.length() < 5) {
		return false;
	}
	const std::vector<std::string> validSpecifiers = { "(W08)","(L16)","(H16)","(W32)"};

	for (const auto& specifier : validSpecifiers) {
		if (compareStrings(str, specifier)) {
			return true;
		}
	}
	return false;
}
// Function to check if a string is a valid immediate hex value (e.g., 0x1A2B3C)
uint32_t lastHex = 0;
bool isImmediateHex(const std::string& str) {
	std::string filtered;
	for (auto s : str)
	{
		if (s != '\'')
		{
			filtered += s;
		}
	}
	if (filtered.length() < 3 || filtered[0] != '0' || filtered[1] != 'x') {
		return false;
	}
	unsigned int hexValue;
	std::string hexStr = filtered.substr(2, std::min(static_cast<int>(filtered.length() - 2), 10));
	for (size_t i = 2; i < filtered.length(); ++i) {
		if (!isxdigit(filtered[i])) {
			return false;
		}
		sscanf(hexStr.c_str(), "%x", &hexValue);
		sscanf(hexStr.c_str(), "%X", &hexValue);
		lastHex = hexValue;
		if (hexValue < 0 || hexValue > 0xFFFFFFFF) {
			return false;
		}
	}
	return true;
}
// Function to check if a string is a valid immediate decimal value (e.g., -12345)
int32_t decSigned = 0;
bool isImmediateDec(const std::string& str) {
	std::string filtered;
	for (auto s : str)
	{
		if (s != '\'' )
		{
			filtered += s;
		}
	}
	if (filtered.empty()) {
		return false;
	}
	
	if (filtered.length() > 50) {
		return false;
	}
	bool allowMinus = true;
	int decValueSigned;
	unsigned int decValueUnsigned;
	std::string decStrSigned = filtered.substr(0,filtered.length());
	std::string decStrUnsigned = filtered.substr(0, std::min(static_cast<int>(filtered.length()), 11));
	for (char c : filtered) {
		if (!isdigit(c)) {
			if (c == '-' && allowMinus) {
				allowMinus = false;
				continue;
			}
			return false;
		}
		int test = sscanf(decStrSigned.c_str(), "%d", &decValueSigned);
		decSigned = decValueSigned;
		if (decValueSigned < -2147483648 || decValueSigned > 2147483647) {
			return false;
		}
	}
	return true;
}
float FValue = 0.0f;
//validate floating point immediates
bool isImmediateFloat(const std::string& str) {
	if (str.empty()) {
		return false;
	}
	bool hasDecimalPoint = false;
	bool allowMinus = true;
	float fValue;
	for (char c : str) {
		if (!isdigit(c)) {
			if (c == '-' && allowMinus) {
				allowMinus = false;
				continue;
			}
			else if (c == '-' && !allowMinus)
			{
				return false;
			}
			if (c == '.' && !hasDecimalPoint) {
				hasDecimalPoint = true;
				continue;
			}
			else if (c == '.' && hasDecimalPoint)
			{
				return false;
			}
			
		}
		if(sscanf(str.c_str(), "%f", &fValue) != 1)
		{
			return false;
		}
	}
	return true;
}
// Check if the string is a valid register (e.g., R00, R01, ..., R3F)
bool isRegister(const std::string& str) {
	if (str.length() < 3|| str[0] != 'R') {
		return false;
	}
	int regNum;
	std::string regNumStr = str.substr(1,2);
	for (size_t i = 1; i < str.length(); ++i) {
		if (!isxdigit(str[i])) {
			return false;
		}
		sscanf(regNumStr.c_str(), "%x", &regNum);
		if (regNum < 0 || regNum > 63) {
			return false;
		}
		 
	}
	return true;
}
// get ready for an excruciatingly stupid hack. 
bool avertedBug = false;
// Function to determine if a space should be inserted after a specific index in the string
bool shouldInsertSpace(std::string& str, int index) {
	if (str.empty()) {
		return false;
	}
	if (index <= 0 || index >= str.length()) {
		return false;
	}
	std::string testStr = str.substr(index, 2);
	std::vector<char> xchartests = { '+','-','(',']' };
	for (auto x : xchartests)
	{
		if (isxdigit(testStr[0]) && testStr[1] == x)
		{
			return true;
		}
	}
	std::vector<char> charxtests = { '+','['};
	for (auto c : charxtests)
	{
		if (testStr[0] == c && isxdigit(testStr[1]))
		{
			return true;
		}
	}
	if (testStr[0] == '[')
	{
		return true;
	}
	/*
	This is a list of the most important modes we must support, expressed in their syntax:
	M$[BB(R01):([R02 + R03])(L16)]
	M$[PC+R01]
	M$[R01+=R02]
	M$[PC-R03]
	M$[[PC]+R02]
	M$[[PC-91]+23]
	*/
	std::vector<std::string> testStrings = {"):",":(","([","])","+R","-R","=R","+=","-=","]+","]-",")(","++","--","+-","-+",")]","(R","\"]"};
	for(int s = 0; s < testStrings.size();s++)
	{
		
		if (testStr == testStrings[s])
		{
			if (testStrings[s] == "):" || testStrings[s] == ")(" || testStrings[s] == ")]")
			{
				str.insert(index, " ");
				str.insert(index + 2, " ");
				avertedBug = true;
				return false;
			}
			return true;
		}
		
	}
	return false;
}
// Function to check if a string is a valid opcode
bool isOpcode(const std::string& str) {
	for (const auto& instruction : optable) {
		if (compareStrings(str, instruction.opcode)) {
			return true;
		}
	}
	//std::cout << "Error: Invalid opcode: '" << str << "'!" << std::endl;
	return false;
}
uint32_t turnIntoBitpattern(const Token& Value)
{
	std::string filtered;
	for (auto c : Value.value)
	{
		if (c != '\'')
		{
			filtered += Value.value;
		}
	}
	if (Value.type == IMMEDIATE_HEX)
	{
		isImmediateHex(filtered);
		return lastHex;
	}
	if (Value.type == IMMEDIATE_DEC)
	{
		isImmediateDec(filtered);
		return static_cast<uint32_t>(decSigned);
	}
	if (Value.type == IMMEDIATE_FLOAT)
	{
		isImmediateFloat(filtered);
		return std::bit_cast<uint32_t>(FValue);
	}
	return 0;
}
// turn one line into an array of tokens
std::vector<Token> tokenizeLine(const std::string& line, uint64_t lineNo = 0) {
	avertedBug = false;
	std::vector<Token> tokens;
	if (line.empty()) {
		return tokens;
	}
	// Simple tokenization logic - replace with actual tokenization as needed
	std::string L = line;
	for (int i = 0; i < L.length(); ++i) {

		if (isspace(L[i])) {
			continue;
		}
		std::string tokenValue;
		int length_dbg = tokens.size();
		while (i < L.length() && !(isspace(L[i]) || L[i] == ',')) {
			tokenValue += L[i];
			if (shouldInsertSpace(L, i)) {
				// Skip the next character (the '(')
				++i;
				L.insert(i, " ");
				break;

			}else if(avertedBug)
			{
				tokenValue.erase(tokenValue.size() - 1);
				avertedBug = false;
				//i++;
				break;
			}

			++i;

		}
 		Token token;
		if (isOpcode(tokenValue)) {
			token.type = OPCODE;

		}
		else if (isRegister(tokenValue)) {
			token.type = REGISTER;
		}
		else if (isImmediateHex(tokenValue)) {
			token.type = IMMEDIATE_HEX;
		}
		else if (isImmediateDec(tokenValue)) {
			token.type = IMMEDIATE_DEC;
		}
		else if (tokenValue[0] == ';') {
			token.type = COMMENT;
			tokenValue = line.substr(i); // Capture the rest of the line as comment
			i = line.length(); // Move to end of line
		}
		else if (isWidthSpecifier(tokenValue)) {
			token.type = WIDTH_SPECIFIER;
		}
		else if (isJumpInstruction(tokenValue)) {
			token.type = JUMP_INSTRUCTION;
		}
		else if (tokenValue.back() == ':' && tokenValue.front() != ':') {
			token.type = LABEL_DEFINE;
		}
		else if (tokenValue.substr(0, 3) == "M$[") {
			token.type = MEMORY_ADDRESS;
		}
		else if (compareStrings(tokenValue.substr(0, 6), "TABLE[")) {
			token.type = DATA_TABLE;
		}
		else if (tokenValue[0] == '[') {
			token.type = OPEN_SQUARE_BRACKET;
		}
		else if (tokenValue[0] == ']') {
			token.type = CLOSE_SQUARE_BRACKET;
		}
		else if (tokenValue[0] == '(')
		{
			token.type = OPEN_ROUND_BRACKET;
		}
		else if (tokenValue[0] == ')')
		{
			token.type = CLOSE_ROUND_BRACKET;
		}
		else if (tokenValue[0] == '+')
		{
			token.type = PLUS;
		}
		else if (tokenValue[0] == '-')
		{
			token.type = MINUS;
		}
		else if (tokenValue[0] == '=')
		{
			token.type = EQUALS;
		}
		else if (tokenValue[0] == ':')
		{
			token.type = COLON;
		}
		else if (compareStrings(tokenValue.substr(0, 2), "BB"))
		{
			token.type = BOUNCEBACK_MODE;
		}
		else if (compareStrings(tokenValue.substr(0, 2), "PC"))
		{
			token.type = PC;
		}
		else if (compareStrings(tokenValue.substr(0, 6), "define")) {
			token.type = LABELED_CONSTANT_DEFINE;
		}
		else if (isImmediateFloat(tokenValue)) {
			token.type = IMMEDIATE_FLOAT;
		}
		else if (tokenValue.find("\\\"") != std::string::npos) {
			token.type = STRING_MID_QUOTE;
		}
		else if (tokenValue.front() == '"' && tokenValue.back() == '"') {
			token.type = TINY_STRING;
		}
		else if (tokenValue.front() == '"') {
			token.type = STRING_START;
		}
		else if (compareStrings(tokenValue.substr(0, 2), "P\"") && tokenValue.back() == '"') {
			token.type = PACKED_TINY_STRING;
		}
		else if(compareStrings(tokenValue.substr(0,2), "P\"")) {
			token.type = PACKED_STRING_START;
		}
		else if (tokenValue.back() == '"') {
			token.type = STRING_END;
		}
		else if (!tokenValue.empty()) {
			token.type = LABEL_REFERENCE;
		}
		else {
			std::cerr << "Error: Unrecognized token: '" << tokenValue << "'!" << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			continue; // Skip unrecognized tokens
		}
		token.value = tokenValue;
		tokens.push_back(token);
	}
	std::vector<Token> filteredTokens;
	for (auto& t : tokens)
	{
		if (t.value.size() > 0 && t.value != "NULL")
		{
			filteredTokens.push_back(t);
		}
	}
	return filteredTokens;
}
// check that the destination slot is valid
bool validateDest(const std::vector<Token>& tokens, const Instruction* instruction, int startingPoint, bool allowSpecial, uint64_t lineNo = 0)
{
	if (tokens.size() > startingPoint)
	{
		
		// token map of a maxed instruction [label][opcode][SP[Destination]][Width][X][Width][Y][Width][Comment]
		if (tokens.at(startingPoint).type == REGISTER)
		{
			if (tokens.size() <= startingPoint + 1)
			{
				return false;
			}
			if (tokens.at(startingPoint + 1).type == WIDTH_SPECIFIER)
			{
				if (tokens.size() < startingPoint + 2)
				{
					return false;
				}
				if (tokens[startingPoint + 2].type != COMMENT && tokens[startingPoint + 2].type != REGISTER && (allowSpecial && tokens[startingPoint + 2].type == JUMP_INSTRUCTION))
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects a destination register, but found "<< token_names.at(tokens[startingPoint + 2].type) << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
			}
			else {
				if (tokens.size() < startingPoint + 1)
				{
					return false;
				}
				if (tokens.at(startingPoint + 1).type != COMMENT && tokens.at(startingPoint + 1).type != REGISTER && (allowSpecial && tokens.at(startingPoint + 1).type == JUMP_INSTRUCTION))
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects a destination register, but found "<< token_names.at(tokens.at(startingPoint + 1).type)<< std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
			}
		}
		else {
			if (instruction->operandGroup != dXy && instruction->operandGroup != dxY && instruction->operandGroup != dXY)
			{
				std::cerr << "Error: Opcode'" << instruction->opcode << "' requires a destination register!" << std::endl;;
				std::cerr << "On line " << lineNo + 1 << std::endl;
			}
			return false;
		}
	}
	else
	{
		std::cerr << "Error: Opcode'" << instruction->opcode << "' requires a destination register !" << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return false;
	}
	return true;
}
int validateMemoryExpression(const Instruction* instruction, const std::vector<Token>& tokens, int startingPoint, uint64_t lineNo = 0)
{
	int scanPoint = startingPoint + 1;
	int idxCount = 0;
	if (instruction->encoding == 0x18 || instruction->encoding == 0x19)
	{
		const std::vector<tokenType> BBTokens = { BOUNCEBACK_MODE,OPEN_ROUND_BRACKET,REGISTER,CLOSE_ROUND_BRACKET,COLON };
		for (int i = 0; i < BBTokens.size(); i++)
		{
			if (tokens[scanPoint + i].type == BBTokens[i])
			{
				idxCount++;
			}
		}
		scanPoint += idxCount;
		if (idxCount != 0 && idxCount != BBTokens.size())
		{
			std::cerr << "Error: Incomplete Bounceback token sequence found." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return -1;
		}
		if (tokens[scanPoint].type == OPEN_ROUND_BRACKET)
		{
			scanPoint++;
		}
		std::vector<std::vector<tokenType>> MemoryModesWidths = {
			{REGISTER,CLOSE_ROUND_BRACKET,WIDTH_SPECIFIER},
			{REGISTER, PLUS, REGISTER,CLOSE_ROUND_BRACKET,WIDTH_SPECIFIER},
			{IMMEDIATE_HEX,CLOSE_ROUND_BRACKET,WIDTH_SPECIFIER},
			{LABEL_REFERENCE,CLOSE_ROUND_BRACKET,WIDTH_SPECIFIER},
			{PC, PLUS, REGISTER,CLOSE_ROUND_BRACKET,WIDTH_SPECIFIER},
			{OPEN_SQUARE_BRACKET, REGISTER, PLUS, REGISTER, CLOSE_SQUARE_BRACKET,CLOSE_ROUND_BRACKET,WIDTH_SPECIFIER},
			{REGISTER,CLOSE_ROUND_BRACKET},
			{REGISTER, PLUS, REGISTER,CLOSE_ROUND_BRACKET},
			{IMMEDIATE_HEX,CLOSE_ROUND_BRACKET},
			{LABEL_REFERENCE,CLOSE_ROUND_BRACKET},
			{PC, PLUS, REGISTER,CLOSE_ROUND_BRACKET},
			{OPEN_SQUARE_BRACKET, REGISTER, PLUS, REGISTER, CLOSE_SQUARE_BRACKET,CLOSE_ROUND_BRACKET},
			{REGISTER},
			{REGISTER, PLUS, REGISTER},
			{IMMEDIATE_HEX},
			{LABEL_REFERENCE},
			{PC, PLUS, REGISTER},
			{OPEN_SQUARE_BRACKET, REGISTER, PLUS, REGISTER, CLOSE_SQUARE_BRACKET}
		};
		int matchCounter = 0;
		for (int i = 0; i < MemoryModesWidths.size(); i++)
		{
			for (int j = 0; j < MemoryModesWidths[i].size(); j++)
			{
				if (tokens[scanPoint + j].type == MemoryModesWidths[i][j])
				{
					matchCounter++;
					if (matchCounter == MemoryModesWidths[i].size())
					{
						if (MemoryModesWidths[i][j] == WIDTH_SPECIFIER)
						{
							if (MemoryModesWidths[i][0] != PC && tokens[scanPoint + j].value == "(H16)")
							{
								std::cerr << "Error: This address mode does not support the (H16) access specifier." << std::endl;
								std::cerr << "On line " << lineNo + 1 << std::endl;
								return -1;
							}
						}
						scanPoint += matchCounter;
						if (tokens[scanPoint].type == CLOSE_SQUARE_BRACKET)
						{
							return scanPoint;
						}
						scanPoint-= matchCounter;
						break;
					}
				}
				else {
					matchCounter = 0;
					continue;
				}
			}
		}

		std::vector<std::vector<tokenType>> ExtraModes = {
		{REGISTER, PLUS, IMMEDIATE_HEX,CLOSE_ROUND_BRACKET},
		{REGISTER, PLUS, IMMEDIATE_DEC,CLOSE_ROUND_BRACKET},
		{REGISTER, PLUS, LABEL_REFERENCE,CLOSE_ROUND_BRACKET},
		{OPEN_SQUARE_BRACKET,REGISTER,CLOSE_SQUARE_BRACKET,PLUS,REGISTER,CLOSE_ROUND_BRACKET},
		{REGISTER,PLUS,EQUALS,REGISTER,CLOSE_ROUND_BRACKET},
		{REGISTER, PLUS, IMMEDIATE_HEX},
		{REGISTER, PLUS, IMMEDIATE_DEC},
		{REGISTER, PLUS, LABEL_REFERENCE},
		{OPEN_SQUARE_BRACKET,REGISTER,CLOSE_SQUARE_BRACKET,PLUS,REGISTER},
		{REGISTER,PLUS,EQUALS,REGISTER},
		};
		int matchCounter2 = 0;
		for (int i = 0; i < ExtraModes.size(); i++)
		{
			for (int j = 0; j < ExtraModes[i].size(); j++)
			{
				if (tokens[scanPoint + j].type == ExtraModes[i][j])
				{
					matchCounter2++;
					if (matchCounter2 == ExtraModes[i].size())
					{
						scanPoint += matchCounter2;
						if (tokens[scanPoint].type == CLOSE_SQUARE_BRACKET)
						{
							return scanPoint;
						}
						else {
							std::cerr << "Error: Incomplete memory expression. Memory Expressions need a closing square bracket." << std::endl;
							std::cerr << "On line " << lineNo + 1 << std::endl;
							return -1;
						}
					}

				}
				else if (tokens[scanPoint + j].type == WIDTH_SPECIFIER)
				{
					std::cerr << "Error: This address mode does not support width specifiers." << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return -1;
				}
				else {
					matchCounter2 = 0;
					continue;
				}
			}
		}
		std::cerr << "Error: Address mode is not supported." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return -1;
	}
	if (instruction->encoding == 0x1A || instruction->encoding == 0x1F)
	{
		const std::vector<std::vector<tokenType>> BranchFormats = {
			{REGISTER},
			{IMMEDIATE_HEX},
			{LABEL_REFERENCE},
			{PC, PLUS, IMMEDIATE_HEX},
			{PC, PLUS, IMMEDIATE_DEC},
			{PC, MINUS, IMMEDIATE_HEX},
			{PC, MINUS, IMMEDIATE_DEC},
			{REGISTER,PLUS,REGISTER},
			{IMMEDIATE_HEX, PLUS, REGISTER},
			{LABEL_REFERENCE, PLUS, REGISTER},
			{OPEN_SQUARE_BRACKET, REGISTER, PLUS, REGISTER, CLOSE_SQUARE_BRACKET},
			{OPEN_SQUARE_BRACKET, IMMEDIATE_HEX, PLUS, REGISTER, CLOSE_SQUARE_BRACKET},
			{OPEN_SQUARE_BRACKET, LABEL_REFERENCE, PLUS, REGISTER, CLOSE_SQUARE_BRACKET},
			{OPEN_SQUARE_BRACKET, PC, PLUS, REGISTER, CLOSE_SQUARE_BRACKET},
			{OPEN_SQUARE_BRACKET, PC, PLUS, IMMEDIATE_HEX, PLUS, REGISTER,CLOSE_SQUARE_BRACKET},
			{OPEN_SQUARE_BRACKET, PC, PLUS, IMMEDIATE_DEC,PLUS, REGISTER,CLOSE_SQUARE_BRACKET },
			{OPEN_SQUARE_BRACKET,REGISTER,CLOSE_SQUARE_BRACKET,PLUS,REGISTER},
			{OPEN_SQUARE_BRACKET,IMMEDIATE_HEX,CLOSE_SQUARE_BRACKET,PLUS,REGISTER},
			{OPEN_SQUARE_BRACKET,LABEL_REFERENCE,CLOSE_SQUARE_BRACKET,PLUS,REGISTER },
			{OPEN_SQUARE_BRACKET, PC, CLOSE_SQUARE_BRACKET, PLUS, REGISTER},
			{OPEN_SQUARE_BRACKET, PC, PLUS, IMMEDIATE_HEX,CLOSE_SQUARE_BRACKET, PLUS, REGISTER},
			{OPEN_SQUARE_BRACKET, PC, PLUS, IMMEDIATE_DEC,CLOSE_SQUARE_BRACKET, PLUS, REGISTER},
		};
		bool sawPC = false;;
		try {
			for (int i = 0; i < BranchFormats.size(); i++)
			{
				int matchCounter = 0;
				for (int j = 0; j < BranchFormats[i].size(); j++)
				{
					if (tokens.at(scanPoint + j).type == BranchFormats[i][j])
					{
						matchCounter++;
						if (BranchFormats[i][j] == PC)
						{
							sawPC = true;
						}
						if (sawPC && (BranchFormats[i][j] == IMMEDIATE_HEX || BranchFormats[i][j]== IMMEDIATE_DEC))
						{
							int output = turnIntoBitpattern(tokens.at(scanPoint + j));
							if (output > 32767 || output < -32768)
							{
								std::cerr << "Error: Short pointers can only fit a signed int16 range! (+32,767 to -32,768)" << std::endl;
								std::cerr << "On line " << lineNo + 1 << std::endl;
								return -1;
							}
						}
						if (matchCounter == BranchFormats[i].size())
						{
							scanPoint += matchCounter;
							return scanPoint;
						}
					}
					else {
						matchCounter = 0;
						continue;
					}
				}
			}
		}
		catch (const std::out_of_range& e)
		{
			return 0;
		}
		std::cerr << "Error: Branch / Call mode is not supported." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return -1;
	}
	if (instruction->encoding == 0x35 || instruction->encoding == 0x36 || instruction->encoding == 0x37 || instruction->encoding == 0x3D)
	{
		const std::vector<std::vector<tokenType>> SimpleFormats = {
			{REGISTER},
			{IMMEDIATE_HEX},
			{LABEL_REFERENCE},
			{OPEN_ROUND_BRACKET,REGISTER,CLOSE_ROUND_BRACKET,WIDTH_SPECIFIER},
			{OPEN_ROUND_BRACKET,REGISTER,CLOSE_ROUND_BRACKET },
		};
		for (int i = 0; i < SimpleFormats.size(); i++)
		{
			int matchCounter = 0;
			for (int j = 0; j < SimpleFormats[i].size(); j++)
			{
				if (tokens[scanPoint + j].type == SimpleFormats[i][j])
				{
					matchCounter++;
					if (matchCounter == SimpleFormats[i].size())
					{
						if (SimpleFormats[i][j] == WIDTH_SPECIFIER)
						{
							if (tokens[scanPoint + j].value == "(H16)")
							{
								std::cerr << "Error: This address mode does not support the (H16) access specifier.\n Should you need it, use an LDM or STM instruction. (Those have the PC-relative address mode which supports (H16))." << std::endl;
								std::cerr << "On line " << lineNo + 1 << std::endl;
								return -1;
							}
						}
						scanPoint += matchCounter;
						return scanPoint;
					}
				}
				else {
					matchCounter = 0;
					continue;
				}
			}
		}
		std::cerr << "Error: Simple Memory Operands (Those outside STM,LDM,CAL,STJ), do not support this address mode." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return -1;
	}

	return 0;
}

// check that operand Y is valid
bool validateY(const std::vector<Token>& tokens, const Instruction* instruction, int startingPoint, bool allowMemory,bool needsY, uint64_t lineNo = 0)
{
	if (tokens.size() > startingPoint)
	{
		if(tokens[startingPoint].type == REGISTER)
		{
		
			if (tokens[startingPoint].type == WIDTH_SPECIFIER)
			{
				if (tokens.size() <= startingPoint + 1)
				{
					return false;
				}
				if (tokens[startingPoint + 1].type != COMMENT && tokens[startingPoint].type != REGISTER && (allowMemory && tokens[startingPoint].type == MEMORY_ADDRESS))
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects a Y operand, But found " << token_names.at(tokens[startingPoint + 2].type) << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
				if(tokens[startingPoint + 1].value == "(H16)")
				{
					std::cerr << "Error: It is illegal to have (H16) as a width specifier for source operands." << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
			}
			else {
			
				if ( tokens[startingPoint].type != REGISTER && (allowMemory && tokens[startingPoint].type == MEMORY_ADDRESS))
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects a Y operand, but found " << token_names.at(tokens[startingPoint + 1].type) << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
			}

		}
		else if (tokens[startingPoint].type == MEMORY_ADDRESS)
		{
			if (allowMemory)
			{
				int result = validateMemoryExpression(instruction, tokens, startingPoint);
				if (result == -1)
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects a valid memory expression, but found: " << token_names.at(tokens.at(startingPoint).type) << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
				else {
					startingPoint += result;
					return true;
				}
			}
			else {
				std::cerr << "Error: The Opcode '" << instruction->opcode << "' does not support memory expressions." << std::endl;
				std::cerr << "On line " << lineNo + 1 << std::endl;
				return false;
			}
		}else if(tokens[startingPoint].type == IMMEDIATE_HEX || tokens[startingPoint].type == IMMEDIATE_DEC || tokens[startingPoint].type == LABEL_REFERENCE || tokens[startingPoint].type == IMMEDIATE_FLOAT)
		{
			if (tokens.size() <= startingPoint + 1)
			{
				//std::cerr <<"Error: Please put a comment after an immediate value or label reference as the rightmost operand. \n Perhaps use this to document which register represents what value for your program." << std::endl;
				//std::cerr << " Please do not ask why you have to do this. " << std::endl;
				//std::cerr << "On line " << lineNo + 1 << std::endl;
				return needsY;
			}
			if (tokens[startingPoint + 1].type == WIDTH_SPECIFIER)
			{
				std::cerr << "Error: Immediate values and labels cannot have a width specifier." << std::endl;
				std::cerr << "On line " << lineNo + 1 << std::endl;
				return false;
			}
		}else if (tokens[startingPoint].type == PC)
		{
			return false;
		}
		return true;
	}
	return false;
}
// check that operand X is valid
bool validateX(const std::vector<Token>& tokens, const Instruction* instruction, int startingPoint, bool allowMemory, uint64_t lineNo = 0)
{
	if (tokens.size() > startingPoint)
	{
		if (tokens.at(startingPoint).type == REGISTER)
		{
			if (tokens.size() <= startingPoint + 1)
			{
				return false;
			}
			if (tokens.at(startingPoint + 1).type == WIDTH_SPECIFIER)
			{
				if (tokens.size() <= startingPoint + 1)
				{
					return false;
				}
				if (tokens.at(startingPoint + 1).type != COMMENT && tokens.at(startingPoint).type != REGISTER)
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects an X operand, but found " << token_names.at(tokens.at(startingPoint + 2).type) <<"."<< std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
				if (tokens.at(startingPoint + 1).value == "(H16)")
				{
					std::cerr << "Error: It is illegal to have (H16) as a width specifier for source operands." << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
					return false;
				}
			}
			else {
				
				if (tokens.at(startingPoint).type != REGISTER)
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects an X operand, but found " << token_names.at(tokens.at(startingPoint + 1).type) << "." << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
			}
		}
		else if (tokens.at(startingPoint).type == IMMEDIATE_HEX || tokens.at(startingPoint).type == IMMEDIATE_DEC || tokens.at(startingPoint).type == LABEL_REFERENCE || tokens.at(startingPoint).type == IMMEDIATE_FLOAT)
		{
			if (tokens.size() <= startingPoint + 1)
			{
				return true;
			}
			if (tokens.at(startingPoint + 1).type == WIDTH_SPECIFIER)
			{
				std::cerr << "Error: Immediate values and label references cannot have a width specifier." << std::endl;
				std::cerr << "On line " << lineNo + 1 << std::endl;
				return false;
			}
		}
		else if (tokens.at(startingPoint).type == MEMORY_ADDRESS)
		{
			if (allowMemory)
			{
				int result = validateMemoryExpression(instruction, tokens, startingPoint);
				if (result == -1)
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects a valid memory expression, but found: " << token_names.at(tokens.at(startingPoint).type) << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
					return false;
				}
				else {
					startingPoint += result;
					return true;
				}
			}
			else {
				std::cerr << "Error: The Opcode '" << instruction->opcode << "' does not support a memory operand as an X operand." << std::endl;
				std::cerr << "On line " << lineNo + 1 << std::endl;
				return false;
			}
		}
		else if (tokens.at(startingPoint).type == PC)
		{
			if ((instruction->encoding == 0x1A || instruction->encoding == 0x1F))
			{
				if (tokens.size() > startingPoint + 2)
				{
					if (tokens.at(startingPoint + 1).type == PLUS || tokens.at(startingPoint + 1).type == MINUS)
					{
						if (tokens.at(startingPoint + 2).type != IMMEDIATE_HEX && tokens.at(startingPoint + 2).type != IMMEDIATE_DEC)
						{
							std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects an immediate value after the PC offset, but found " << token_names.at(tokens.at(startingPoint + 2).type) << std::endl;
							std::cerr << "On line " << lineNo + 1 << std::endl;
							return false;
						}
					}
				}
			}
			else {
				std::cerr << "Error: The Opcode '" << instruction->opcode << "' does not support the PC register as an operand. PC Operands are limited to specific opcodes. These are: STJ and CAL." << std::endl;
				std::cerr << "On line " << lineNo + 1 << std::endl;
				return false;
			}

		}
		return true;
	}
	return false;
}

// check that an opcode has a valid destination, X, Y combination.
bool validateSetup(const std::vector<Token>& tokens, const Instruction* instruction, int startingPoint, bool Dest, bool X, bool Y, bool specialDest = false, bool specialX = false, bool specialY = false, uint64_t lineNo = 0)
{
	if (Dest != validateDest(tokens, instruction, startingPoint, specialDest, lineNo))
	{
		return false;

	}
	else if (tokens.size() > startingPoint + 1)
	{
		startingPoint++;
	}
    if (startingPoint + 1 < tokens.size() && tokens[startingPoint + 1].type == WIDTH_SPECIFIER)
	{
		startingPoint++;
	}
	if (X != validateX(tokens, instruction, startingPoint, specialX,lineNo))
	{
		return false;
	}
	else if (tokens.size() > startingPoint + 1)
	{
		startingPoint++;
	}
	if (startingPoint + 1 < tokens.size() && tokens.at(startingPoint + 1).type == WIDTH_SPECIFIER)
	{
		startingPoint++;
	}
	if (Y != validateY(tokens, instruction, startingPoint, specialY, Y, lineNo))
	{
		return false;
	}
	return true;
 }
stringResult validateString(const std::vector<Token>& tokens, uint64_t lineNo = 0) {
	if (tokens.empty()) {
		std::cerr << "Error: No tokens to validate." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	int balance = 0;
	bool foundStart = false;
	for (const auto& token : tokens) {
		if (token.type == STRING_START || token.type == PACKED_STRING_START) {
			balance++;
			foundStart = true;
		}
		if (((token.type == TINY_STRING || token.type == PACKED_TINY_STRING) && balance > 0) || balance > 1) {
			std::cerr << "Error: Strings may not contain Strings. If you intended to have quotes in your String, mark them like you would in C++." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return INVALID_STRING;
		}
		else if (token.type == STRING_END) {
			balance--;
		}
		if (balance < 0) {
			std::cerr << "Error: Mismatched string tokens." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return INVALID_STRING;
		}
	}
	if (balance != 0) {
		std::cerr << "Error: Mismatched string tokens." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	if (!foundStart)
	{
		return STRING_NOT_FOUND;
	}
	return VALID_STRING;
}
stringResult validateDataTable(const std::vector<Token>& tokens, uint64_t lineNo = 0) {
	if (tokens.empty()) {
	///	std::cerr << "Error: No tokens to validate." << std::endl;
	//	std::cerr << "On line " << lineNo + 1 << std::endl;
		return STRING_NOT_FOUND;
	}
	bool hasConstantTableStart = false;
	if (tokens[0].type == LABELED_CONSTANT_DEFINE)
	{
		return STRING_NOT_FOUND;
	}
	if (tokens[0].type != DATA_TABLE && tokens[0].type != LABEL_DEFINE)
	{
		std::cerr << "Error: Data tables must start with a data table start token or a label definition." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	for (const auto& token : tokens) {
		if (token.type == DATA_TABLE && !hasConstantTableStart)
		{
			hasConstantTableStart = true;
		}
		else if (token.type == DATA_TABLE && hasConstantTableStart)
		{
			std::cerr << "Error: More than one data table per line, tables within tables, and tables with multiple start tokens are not allowed." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return INVALID_STRING;
		}
	}
	if (!hasConstantTableStart) {
		return STRING_NOT_FOUND;
	}
	bool encounteredStart = false;
	for (const auto& token : tokens)
	{
		if (encounteredStart)
		{
			if (token.type == LABELED_CONSTANT_DEFINE || token.type == MEMORY_ADDRESS || token.type == LABEL_DEFINE || token.type == COMMENT || token.type == JUMP_INSTRUCTION || token.type == OPCODE)
			{
				std::cerr << "Error: a " << token_names.at(token.type) << "is not permitted in a Data Table.";
				std::cerr << "On line " << lineNo + 1 << std::endl;
				return INVALID_STRING;
			}
			if (token.type == WIDTH_SPECIFIER || token.type == REGISTER)
			{
				std::cerr << "Error: a " << token_names.at(token.type) << "is not permitted in a Data Table.";
				std::cerr << "On line " << lineNo + 1 << std::endl;
				return INVALID_STRING;
			}
		}
		else if (token.type == DATA_TABLE && !encounteredStart)
		{
			encounteredStart = true;
		}
	}
	bool hasSquareBracketClose = false;
	bool hasSeenComment = false;
	for (int i = 0; i < tokens.size(); ++i) {
		if (tokens[i].type == CLOSE_SQUARE_BRACKET && hasSquareBracketClose) {
			std::cerr << "Error: Data Tables must have exactly one closing square bracket on the same line." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return INVALID_STRING;
		}
		if (i < tokens.size() && hasSquareBracketClose && !hasSeenComment)
		{
			std::cerr << "Error: Only comments are allowed after a data table end token on the same line." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return INVALID_STRING;
		}
		if (tokens[i].type == CLOSE_SQUARE_BRACKET && !hasSquareBracketClose) {
			hasSquareBracketClose = true;
		}
		
		if (tokens[i].type == COMMENT) {
			hasSeenComment = true;
		}
		
	}
	if (!hasSquareBracketClose) {
		std::cerr << "Error: Data tables must have a closing square bracket." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	return VALID_STRING;
}

std::map<std::string, uint32_t> defineTable;
stringResult validateLabeledConstantDefine(const std::vector<Token>& tokens, uint64_t lineNo = 0)
{
	if (tokens[0].type != LABELED_CONSTANT_DEFINE)
	{
//		std::cerr << "Error: 'define' directives must have a 'define' directive token at the start. Labels may not precede a 'define' directive. (On the same line)" << std::endl;
//		std::cerr << "On line " << lineNo + 1 << std::endl;
		return STRING_NOT_FOUND;
	}
	if (tokens.size() < 4)
	{
		std::cerr << "Error: 'define' directives must have a 'define' directive token, a name,an equals sign, a value, and optionally a comment, in that order." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	if (tokens[1].type != LABEL_REFERENCE)
	{
		std::cerr << "Error: 'define' directives are not declared the same way as a label. A label is declared with label:. a 'define' directive is declared with: define name = value." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	if (tokens[2].type != EQUALS)
	{
		std::cerr << "Error: The name given to a constant must be followed with an =." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	if (tokens[3].type != IMMEDIATE_DEC && tokens[3].type != IMMEDIATE_HEX && tokens[3].type != IMMEDIATE_FLOAT)
	{
		std::cerr << "Error: A 'define' directive's value must be exactly one of: Immediate (Decimal), Immediate (Hex), Immediate (Float)" << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	if (tokens.size() > 4)
	{
		if (tokens[3].type != COMMENT)
		{
			std::cerr << "Error: A 'define' directive's value may only be followed with a comment (on the same line)."<<std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return INVALID_STRING;
		}
	}
	if (defineTable.find(tokens[1].value) != defineTable.end())
	{
		std::cerr << "Error: 'define' directives may not have duplicate names. Duplicate values, however, are allowed." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return INVALID_STRING;
	}
	else {
		defineTable[tokens[1].value]= turnIntoBitpattern(tokens[3]);
	}
	return VALID_STRING;
}

bool validateTokens(const std::vector<Token>& tokens, uint64_t lineNo = 0) {
	if (tokens.empty()) {
		std::cerr << "Warning: No tokens to validate on this line." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return true;
	}
	// Validate the first token as an opcode
	if (tokens[0].type == COMMENT) {
		return true;
	}
	std::set<tokenType> validFirstTokens = { OPCODE, JUMP_INSTRUCTION, LABEL_DEFINE, STRING_START,PACKED_STRING_START,TINY_STRING, PACKED_TINY_STRING, STRING_MID_QUOTE, STRING_END, DATA_TABLE, LABELED_CONSTANT_DEFINE };
	if (!validFirstTokens.contains(tokens[0].type)) {
		std::cerr << "Error: First token must be one of: Opcode, Label Defintion, String, Constant Table,'define' directive."<< std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return false;
	}
	// Validate the rest of the tokens based on the opcode's expected operand group
	if (tokens[0].type == LABEL_DEFINE && tokens.size() < 2)
	{
		return true;
	}
	const Instruction* instruction = (tokens[0].type == LABEL_DEFINE) ? findInstruction(tokens[1].value) : (findInstruction(tokens[0].value));
	const int startingPoint = (tokens[0].type == LABEL_DEFINE) ? 2 : 1;
	if (tokens.size() > 1 &&tokens[1].type == LABEL_DEFINE) {
		std::cerr << "Error: Only one label definition per line is allowed." << std::endl;
		std::cerr << "On line " << lineNo + 1 << std::endl;
		return false;
	}
	if (instruction == nullptr) {
		bool isString = validateString(tokens,lineNo) == VALID_STRING;
		bool isTable = validateDataTable(tokens,lineNo) == VALID_STRING;
		bool isLCD = validateLabeledConstantDefine(tokens,lineNo) == VALID_STRING;
 		if (!isString && !isTable && !isLCD)
		{
			std::cerr << "Error: Invalid line: '" << ((tokens[0].type == LABEL_DEFINE) ? tokens[1].value : tokens[0].value) << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		return true;
	}
	switch (instruction->operandGroup) {
	case dxy:
		if (tokens.size() > startingPoint)
		{
			if (tokens[startingPoint].type != COMMENT)
			{
				if (tokens.size() - startingPoint == 1)
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects no operands, yet 1 was provided." << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
				}
				else if (tokens.size() - startingPoint > 1)
				{
					std::cerr << "Error: The Opcode '" << instruction->opcode << "' expects no operands, yet " << tokens.size() - startingPoint << " were provided." << std::endl;
					std::cerr << "On line " << lineNo + 1 << std::endl;
				}
				return false;
			}
		}
		break;
	case Dxy:
		if (!validateSetup(tokens, instruction, startingPoint, true, false, false,instruction->specialDestination, instruction->specialX, instruction->specialY,lineNo))
		{
			std::cerr << "Error: Invalid operands for instruction '" << instruction->opcode << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		break;
	case dXy:
		if (!validateSetup(tokens, instruction, startingPoint, false, true, false,instruction->specialDestination, instruction->specialX, instruction->specialY,lineNo))
		{
			std::cerr << "Error: Invalid operands for instruction '" << instruction->opcode << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		break;
	case DXy:
		if (!validateSetup(tokens, instruction, startingPoint, true, true, false,instruction->specialDestination, instruction->specialX, instruction->specialY,lineNo))
		{
			std::cerr << "Error: Invalid operands for instruction '" << instruction->opcode << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		break;
	case dxY:
		if (!validateSetup(tokens, instruction, startingPoint, false, false, true,instruction->specialDestination, instruction->specialX, instruction->specialY,lineNo))
		{
			std::cerr << "Error: Invalid operands for instruction '" << instruction->opcode << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		break;
	case DxY:
		if (!validateSetup(tokens, instruction, startingPoint, true, false, true,instruction->specialDestination, instruction->specialX, instruction->specialY,lineNo))
		{

			std::cerr << "Error: Invalid operands for instruction '" << instruction->opcode << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		break;
	case dXY:
		if (!validateSetup(tokens, instruction, startingPoint, false, true, true,instruction->specialDestination, instruction->specialX, instruction->specialY,lineNo))
		{
			std::cerr << "Error: Invalid operands for instruction '" << instruction->opcode << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		break;
	case DXY:
		if (!validateSetup(tokens, instruction, startingPoint, true, true, true,instruction->specialDestination, instruction->specialX, instruction->specialY,lineNo))
		{
			std::cerr << "Error: Invalid operands for instruction '" << instruction->opcode << "'." << std::endl;
			std::cerr << "On line " << lineNo + 1 << std::endl;
			return false;
		}
		break;
	}
	
		// Further validation logic can be added here based on the instruction's operand group
		return true;
}
std::map<std::string, uint64_t> symbolTable;
bool computeSymbolTable(std::vector<std::vector<Token>>& sourceTokens)
{
	uint64_t addressCounter = 0;
	std::vector<std::string> labelReferences;
	std::vector<int> lineNumbers;
	bool hadError = false;
	for (int i = 0; i < sourceTokens.size(); i++)
	{
		for (int j = 0; j < sourceTokens.size(); j++)
		{
			tokenType Type = sourceTokens[i][j].type;
			std::string Value = sourceTokens[i][j].value;
			bool isInPString = false;
			bool isInString = false;
			bool isInTable = false;
			bool allowImmediates = true;
			if (Type == OPCODE || Type == JUMP_INSTRUCTION || Type == IMMEDIATE_FLOAT)
			{
				addressCounter++;
				allowImmediates = true;
			}
			if (allowImmediates)
			{
				if (Type == IMMEDIATE_HEX || Type == IMMEDIATE_DEC)
				{
					addressCounter++;
				}
				if (Type == LABEL_REFERENCE)
				{
					addressCounter++;
				}
			}

			if (Type == PC)
			{
				allowImmediates = false;
			}
			if (!isInPString && !isInString)
			{
				if (Type == PACKED_TINY_STRING)
				{
					addressCounter += (Value.length() - 3) / 4;
				}
				if (Type == TINY_STRING)
				{
					addressCounter += (Value.length() - 2) * 4;
				}
			}
			if (isInPString)
			{
				addressCounter += Value.length() / 4;
			}
			if (isInString)
			{
				addressCounter += Value.length() * 4;
			}
			if (Type == PACKED_STRING_START)
			{
				addressCounter += (Value.length() - 2) / 4;
				isInPString = true;
			}
			if (Type == STRING_START)
			{
				addressCounter += (Value.length() - 1) * 4;
			}
			if (Type == STRING_END)
			{
				if (isInPString)
				{
					isInPString = false;
					addressCounter = (Value.length() - 1) / 4;
				}
				if (isInString)
				{
					isInString = false;
					addressCounter = (Value.length() - 1) * 4;
				}
			}
			if (Type == DATA_TABLE)
			{
				isInTable = true;
			}
			if (isInTable && !(isInString || isInPString) && Type != IMMEDIATE_FLOAT)
			{
				addressCounter++;
			}
			if (isInTable && Type == CLOSE_SQUARE_BRACKET)
			{
				isInTable = false;
			}
			if (Type == LABEL_DEFINE)
			{
				if (symbolTable.find(Value) != symbolTable.end())
				{
					std::cerr << "Error: Duplicate label definitions are not allowed." << std::endl;
					std::cerr << "On line: " << i + 1 << std::endl;
					hadError = true;
				}
				else {
					symbolTable[Value] = addressCounter;
				}
			}
			if (Type == LABEL_REFERENCE)
			{
				if (sourceTokens[i][0].type != LABELED_CONSTANT_DEFINE)
				{
					labelReferences.push_back(Value);
					lineNumbers.push_back(i);
				}
				if (defineTable.find(Value) != defineTable.end())
				{
					std::cerr << "Error: Labels and 'define' directives must not have the same names." << std::endl;
					std::cerr << "On line: " << i + 1 << std::endl;
					hadError = true;
				}
			}
		}
	}
	for(int l = 0; l < labelReferences.size(); l++)
	{
		if (symbolTable.find(labelReferences[l]) == symbolTable.end())
		{
			std::cerr<<"Error: Found a label reference with no matching definition. Label references must have a matching definition elsewhere in the code." << std::endl;
			std::cerr << "On line: " << lineNumbers[l] + 1 << std::endl;
			hadError = true;
		}
	}

	return !hadError;
}
void processFile(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "Error: Could not open file " << filename << std::endl;
		return;
	}
	std::string line;
	int lineNo = 0;
	bool justStop = false;
	std::vector<std::vector<Token>> all_lines;
	while (std::getline(file, line)) {
		// Process each line of the file
		std::vector<Token> T = tokenizeLine(line);
		bool isValid = validateTokens(T, lineNo++);
		if (!justStop)
		{
			if (isValid)
			{
				all_lines.push_back(T);
			}
			else {
				justStop = true;
			}
		}
	}
	if (!justStop)
	{
		computeSymbolTable(all_lines);
	}
	file.close();
}

int main()
{
	std::cout << "Welcome to this RR32X Assembler! \n" << std::endl;
	std::cout << "Hint: If you see a On Line X, for your convenience, those numbers start at 1." << std::endl;
	std::cout << "Hint: use 'help' to see what this tool can do." << std::endl;
	/* For initial testing, A test line will be used to test how lines shall be processed. This will be replaced with a file input later on.
	* NOTE TO FUTURE SELF: REPLACE THE TEST LINE WITH FILE INPUT WHEN READY TO IMPLEMENT FILE INPUT FUNCTIONALITY.*/
	while (true)
	{
		std::string command;
		std::cout << "Enter a command: ";
		std::getline(std::cin, command);
		if (command == "validate_single")
		{
			std::string input;
			std::cout << "Enter the line to validate: ";
			std::getline(std::cin, input);
			std::string testLine = input;
			std::vector<Token> tokens = tokenizeLine(testLine);
			bool isValid = validateTokens(tokens, 0);
			if (isValid)
			{
				std::cout << "Line is valid!" << std::endl;
			}
			else {
				std::cout << "Line is not valid! see Above." << std::endl;
			}
			std::cout << std::endl;
		}else if (command == "assemble_file")
		{
			std::string filename;
			std::cout << "Enter the filepath to the file you wish to assemble: ";
			std::cin >> filename;
			processFile(filename);
			std::cout << std::endl;
		}else if (command == "help")
		{
			std::cout << "All known commands and what they do:" << std::endl;
			std::cout << "validate_single  check if one line of assembly is valid" << std::endl;
			std::cout << "exit_program  exits this program" << std::endl;
			std::cout << "assemble_file   enter a filename to assemble a whole file" << std::endl;
			std::cout << "help  shows this list" << std::endl;
			std::cout << std::endl;
		}
		else if (command == "exit_program")
		{
			std::cout << "Exiting program. CYA!" << std::endl;
			std::cout << std::endl;
			break;
		}
		else {
			std::cout << "Unknown command. Type 'help' for a list of commands." << std::endl;
			std::cout << std::endl;
		}

	}
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
