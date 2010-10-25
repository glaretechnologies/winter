#ifndef GLOBALS_H_666
#define GLOBALS_H_666

#include <string>

#define printVar(v) conPrint(std::string(#v) + ": " + ::toString(v))

void conPrint(const std::string& s);
void conPrintStr(const std::string& s);
void fatalError(const std::string& s);

const bool DEBUG_VERBOSE = false;

enum INDIGO_DEBUG_LEVEL
{
	DEBUG_LEVEL_RELEASE = 1,
	DEBUG_LEVEL_DEBUG = 2,
	DEBUG_LEVEL_VERBOSE = 3
};

enum IndigoProductVersion
{
	INDIGO_FULL	= 1,
	INDIGO_RT	= 2,
};

static const IndigoProductVersion indigoProductVersion = INDIGO_FULL;

INDIGO_DEBUG_LEVEL getDebugLevel();
bool atDebugLevel(INDIGO_DEBUG_LEVEL level);

const std::string indigoVersionDescription();

#endif //GLOBALS_H_666
