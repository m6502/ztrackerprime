#ifdef _MSC_VER
#    pragma warning(disable: 4786) // identifier was truncated to '255' characters
#endif
/***************************************************************************
                          fxmlelement.h  -  description
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

#include <stdio.h>
#include <list>
#include <string>
#include <map>
#pragma warning(disable:4786)
class FXMLElement {

public:
	FXMLElement();
	~FXMLElement();

	//wrappers for name and value
	std::string Name();
	void Name(std::string sNewName);
	std::string Value();
	void Value(std::string sNewValue); 
	std::string Dump();

    std::string getValueFromPath(std::string path);
    FXMLElement* findChild(FXMLElement *root, std::string name);


    //attributes
	void SetAttributePair(std::string sAttributeName, std::string sAttributeValue);
        std::map<std::string, std::string> mAttributes; //attribute map
	std::list<FXMLElement*> *mChildren;	//children
	FXMLElement *fxParent; //pointer to the parent element, NULL for root

private:
        std::string sName;
        std::string sValue;


};

