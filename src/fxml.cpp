#ifdef _MSC_VER
#    pragma warning(disable: 4786) // identifier was truncated to '255' characters
#endif
/***************************************************************************
                          fxml.cpp  -  description
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

#include <string>
#include <fstream>
#include <iostream>
#include "fxml.h"


///////////////////////////////////////////////////////
//
//	Name: ParseFile
//	Params: cFileName - name of XML file to parse
//	Returns: FXMLElement of parsed results
//	Description: Calls ParseElement to parse XML File
///////////////////////////////////////////////////////

FXMLElement* FXMLParser::ParseFile(char* cFileName)
{
	std::string sInput; //input to read
	
	ReadFile(cFileName, sInput); //read file into string
	
	return ParseString(sInput);
}


///////////////////////////////////////////////////////
//
//	Name: ParseString
//	Params: sInput - String to parse
//	Returns: FXMLElement of parsed results
//	Description: Calls ParseElement to parse XML File
///////////////////////////////////////////////////////

FXMLElement* FXMLParser::ParseString(std::string sInput)
{
	long nLoc = 0;	//location of file to start
	long nLineCount = 1; //line count to start
	
	if (sInput.empty())
	{
	    nError = -1;
	    sError = "File not found or empty";	    
	    return NULL;
	}

#ifdef _USE_EXPAT
	if (nParser == USE_PARSER_EXPAT)
	{
	    fXMLExpat fxeParser;
	    //parse the input using the expat wrapper
	    FXMLElement *fxReturn = fxeParser.ParseString(sInput);
	    //set the error values
	    nError = fxeParser.nError;
	    sError = fxeParser.sError;	    
	    //return
	    return fxReturn;
	}
	else if (nParser == USE_PARSER_FXML)
	{
#endif
		FXMLElement* fxRet = ParseElement(sInput, &nLoc, &nLineCount);
		if ((fxRet == NULL) || (sError.length() > 0))
		{
			//an error occurred, set nError to -1
			nError = -1;
			if (sError.length() == 0) sError = "FXML reports parse error.";		
		}
		return fxRet;

#ifdef _USE_EXPAT
	}
	else
		return NULL;
#endif
}

///////////////////////////////////////////////////////
//
//	Name: ParseElement
//	Params: sCurInput - XML string to parse
//		fxInput - FXMLElement to modify
//		nLoc -position to start at
//		nLineCount - line being processed (for error handling)
//	Returns: Parses string of XML, returns FXMLElement
//	Description: Parses XML string
///////////////////////////////////////////////////////


FXMLElement* FXMLParser::ParseElement(std::string sCurInput, long* nLoc, long* nLineCount)
{
	int nMode = PARSE_MODE_NONE; //set the initial mode
	std::string sWord("");	//set the current word read
	char ch;	//current character read
	std::string sAttributeName; //attribute name to save
	int nSpecialMode; //if parsing comments, used to save old mode
	FXMLElement *fxInput = NULL;
	FXMLElement *fxChild = NULL;
	std::string sElementName; //element name, to be used for error checking

    //parse the line a char at a time
    for (long i = (*nLoc); i < sCurInput.length(); i++)
    {
        ch = sCurInput.at(i);
		if (ch == '<')
		{
			//parse the start of new elements
			if (nMode == PARSE_MODE_NONE)
			{
				//the beginning of an element
				nMode = PARSE_MODE_ELEMENT_PROVISIONAL;
				//create a new input element
				if (fxInput == NULL)
				{
					fxInput = new FXMLElement;
					fxInput->fxParent = NULL;
				}
			}
			else if ((nMode == PARSE_MODE_ELEMENT_DATA) || (nMode == PARSE_MODE_CDATA_END))
			{
			    nMode = PARSE_MODE_END_PROVISIONAL;
			    
			    //see if this is the last element or not
			    if (sCurInput.at(i + 1) == '/')
			    {
					//it is a last element, set mode
					fxInput->Value(sWord);
					nMode = PARSE_MODE_END;	
					sWord.erase();			
			    }
			    else
			    {
					//if not last element, perform recursive call
					if (fxInput->mChildren == NULL)
						fxInput->mChildren = new std::list<FXMLElement*>;
					fxChild = ParseElement(sCurInput, &i, nLineCount);				
					if (fxChild != NULL)
					{
						fxChild->fxParent = fxInput;
						//V 0.92 Elements now in order thanks to
						//Max Belugin - belugin@mail.ru  
						fxInput->mChildren->push_back(fxChild);
					}

					//set mode, initialize sWord with existing data
					nMode = PARSE_MODE_ELEMENT_DATA;
					
					//V0.93 removed - to be fixed later
					//sWord = fxInput->Value();
			    }
			    
			}
			else if (nMode == PARSE_MODE_CDATA)
			{
			    sWord.append(1, ch);
			}
		}
		else if ((ch == ' ') || (ch == '\n') || (ch == '\r') || (ch == '\t'))
		{
			//if in element data, add it.  Else end state

			//also, do not add newlines or spaces to data
			if (nMode == PARSE_MODE_ELEMENT_DATA)
			{
				if (sWord.length() > 0) sWord.append(1,ch);
			}
			else if ((nMode == PARSE_MODE_CDATA) || (nMode == PARSE_MODE_ATTRIBUTE_VALUE))
				//V 0.92 Attribute values with spaces now valid thanks to Max Belugin - belugin@mail.ru  
				sWord.append(1, ch);
			else if (nMode == PARSE_MODE_ELEMENT_NAME)
			{
				//set the element name
				sElementName = sWord;
				fxInput->Name(sWord);
				sWord.erase();
				//now set to look for attributes
				nMode = PARSE_MODE_ATTRIBUTE_PROVISIONAL;
			}

			//increment line count if \n
			if (ch == '\n') (*nLineCount)++;
		}
		else if ((ch == '?') || (ch == '!'))
		{
			//its a comment or declaration.  For this version
			//of the parser, ignore them
			if (nMode == PARSE_MODE_ELEMENT_PROVISIONAL)
			{
			    //save the old mode
			    nSpecialMode = nMode;
			    nMode = PARSE_MODE_SPECIAL;
			}			    
		}
		else if (ch == '[')
		{
		    //look out for cdata tags
		    if (nMode == PARSE_MODE_SPECIAL)
		    {
			if (sCurInput.substr(i+1, 5).compare("CDATA") == 0)
			    nMode = PARSE_MODE_CDATA_PROVISIONAL;
		    }	
		    else if (nMode == PARSE_MODE_CDATA_PROVISIONAL)
		    {
				sWord.erase();
				nMode = PARSE_MODE_CDATA;
		    }
		    else
			sWord.append(1, ch);    
		}
		else if (ch == ']')
		{
		    //look for end of CDATA
		    if (nMode == PARSE_MODE_CDATA)
				nMode = PARSE_MODE_CDATA_END_PROVISIONAL;
		    else if (nMode == PARSE_MODE_CDATA_END_PROVISIONAL)
				nMode = PARSE_MODE_CDATA_END;
		    else
				sWord.append(1, ch);
		    
		}
		else if (ch == '>')
		{
			//end of element name
			if (nMode == PARSE_MODE_ELEMENT_NAME)
			{
				//set the element name
				sElementName = sWord;
				fxInput->Name(sWord);
				sWord.erase();
				//now set more to look for element data
				nMode = PARSE_MODE_ELEMENT_DATA;
			}
			else if (nMode == PARSE_MODE_ATTRIBUTE_PROVISIONAL)
			{
			    //attribute parsing over, prepare to parse element data
			    sWord.erase();
			    nMode = PARSE_MODE_ELEMENT_DATA;
			}
			else if (nMode == PARSE_MODE_END)
			{
			    //end of the element
				//only compare IF sWord.length > 0
				if (sWord.length() > 0)
				{
					if (sWord.compare(sElementName) != 0)
					{
						//parse error!!!
						//V0.93 handle parse errors nicely!
						char* cError = (char*)malloc(6);
						 sprintf(cError, "%d", (*nLineCount));
						sError = (std::string)"FXML reports parsing error.  Unmatched tag: Expecting " +
							sElementName + (std::string)" but encountered " + sWord + (std::string)" at line: " + cError;
						free(cError);
						delete fxInput;
						fxInput = NULL;
					}
				}
				//V0.93
				//do NOT skip ahead one if xml file stuffed together
				//handles <xml>blah</xml><more>data</more> correctly
				//parse element data
			    sWord.erase();
				if (sCurInput.length() > (i + 1))
				{
					if ((sCurInput.at(i + 1) =='\n') ||
						(sCurInput.at(i + 1) =='\r'))
					{
						(*nLoc) = i + 1;
					}
					else
						(*nLoc) = i;
				}
				else
					(*nLoc) = i;

			    return fxInput;
			}
			else if (nMode == PARSE_MODE_SPECIAL)
			{
			    //end of a special element, reset the mode
			    nMode = nSpecialMode;
			}
			else if (nMode == PARSE_MODE_CDATA)
			{
			    sWord.append(1, ch);
			}
		}
		else if (ch == '/')
		{
		    //handle elements such as <blah ..... />
		    if (nMode == PARSE_MODE_ATTRIBUTE_PROVISIONAL)
		    {
				nMode = PARSE_MODE_END;
		    }
		    else if ((nMode != PARSE_MODE_SPECIAL) && (nMode != PARSE_MODE_END))
				sWord.append(1, ch);
		    			
		}
		else if (ch == '=')
		{
			//parsing of attributes
			if (nMode == PARSE_MODE_ATTRIBUTE_NAME)
			{
				//set the attribute name	
				sAttributeName = sWord;
				sWord.erase();
				//set the mode
				nMode = PARSE_MODE_ATTRIBUTE_VALUE_PROVISIONAL;
			}
			else if (nMode != PARSE_MODE_SPECIAL)
				sWord.append(1, ch);

		}
		else if ((ch == '"') || (ch == '\''))
		{
			//parsing of attributes
			if (nMode == PARSE_MODE_ATTRIBUTE_VALUE_PROVISIONAL)
			{
				nMode = PARSE_MODE_ATTRIBUTE_VALUE;
			}
			else if (nMode == PARSE_MODE_ATTRIBUTE_VALUE)
			{
				//save the pair 
				fxInput->SetAttributePair(sAttributeName, sWord);
				//erase
				sWord.erase();	
				sAttributeName.erase();
				
				//set new mode
				nMode = PARSE_MODE_ATTRIBUTE_PROVISIONAL;
			}
			else if (nMode != PARSE_MODE_SPECIAL)
				sWord.append(1, ch);
	
			
		}
		else
		{
			//parsing of straight text/words
			if (nMode == PARSE_MODE_ELEMENT_PROVISIONAL)
				nMode = PARSE_MODE_ELEMENT_NAME;
			else if (nMode == PARSE_MODE_ATTRIBUTE_PROVISIONAL)
				nMode = PARSE_MODE_ATTRIBUTE_NAME;

			//ignore comments and special declarations
			if ((nMode != PARSE_MODE_SPECIAL) && (nMode != PARSE_MODE_CDATA_END))
				sWord.append(1, ch);
		}


	} 
	
	return NULL;
}

///////////////////////////////////////////////////////
//
//	Name: ReadFile
//	Params: cFilename - name of file to parse
//		Str - string to place file into
//	Returns: 0
//	Description: Transforms disk file into string
///////////////////////////////////////////////////////

int FXMLParser::ReadFile(const char *cFilename, std::string &Str)
{
  std::string Buffer(""); //line of file to read

  
  Str.assign("");

  if (strlen(cFilename) > 256) return -1;

  //std::ifstream File(cFilename, std::ios::in | std::ios::nocreate); // Pre-VS2008
  std::ifstream File(cFilename, std::ios::in); //<VS2008>

  //open the file
  if(File)
  {
    //read file into string
    while(!File.eof()) 
    {
		std::getline(File, Buffer);    
		Buffer.append("\n\0"); 
		Str.append(Buffer);
    }
  }
  
  return 0;
}

