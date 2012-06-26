#ifdef _MSC_VER
#    pragma warning(disable: 4786) // identifier was truncated to '255' characters
#endif
/***************************************************************************
                          fxml.h  -  description
                             -------------------
    begin                : Sat Oct 30 1999
    copyright            : (C) 1999 by Team FXML
    email                : fxml@exite.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the BSD License.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef _USE_EXPAT
#include "fxmlexpat.h"
#else
#include "fxmlelement.h"
#endif

#pragma warning(disable:4786)
#define PARSE_MODE_NONE 0
#define PARSE_MODE_ELEMENT_PROVISIONAL 1
#define PARSE_MODE_ELEMENT_DATA 2
#define PARSE_MODE_END 3
#define PARSE_MODE_ELEMENT_NAME 4
#define PARSE_MODE_ATTRIBUTE_PROVISIONAL 5
#define PARSE_MODE_ATTRIBUTE_NAME 6
#define PARSE_MODE_ATTRIBUTE_VALUE 7
#define PARSE_MODE_ATTRIBUTE_VALUE_PROVISIONAL 8
#define PARSE_MODE_END_PROVISIONAL 9
#define PARSE_MODE_SPECIAL 10
#define PARSE_MODE_CDATA 11
#define PARSE_MODE_CDATA_PROVISIONAL 12
#define PARSE_MODE_CDATA_END_PROVISIONAL 13
#define PARSE_MODE_CDATA_END 14
#define USE_PARSER_FXML 0
#define USE_PARSER_EXPAT 1

class FXMLParser {
public:
	//Parses XML File.  Returns pointer to FXElement if successful, or NULL
	//if unsuccessful
	FXMLElement* ParseFile(char* cFileName);
	FXMLElement* ParseString(std::string sInput);

    int nParser;
	std::string sError;
	int nError;

	FXMLParser()
	{
		nParser = USE_PARSER_FXML;
		nError = 0;
		sError = "";
	}
private:
	FXMLElement* ParseElement(std::string sCurInput, long* nLoc, long* nLineCount)	;
	int ReadFile(const char *cFilename, std::string &Str);
};
