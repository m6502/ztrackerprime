#ifdef _MSC_VER
#    pragma warning(disable: 4786) // identifier was truncated to '255' characters
#endif
/***************************************************************************
                          fxmlelement.cpp  -  description
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

#include <iostream>
#include "fxmlelement.h"

///////////////////////////////////////////////////////
//
//	Name: Value
//	Params: set - new value, get - nothing
//	Returns: set - nothing, get - value
//	Description: wrappers for value member
///////////////////////////////////////////////////////

std::string FXMLElement::Value()
{
    return sValue;
}

void FXMLElement::Value(std::string sNewValue)
{
#ifdef _DEBUG
    cout << "Setting Value to: " << sNewValue.c_str() << endl;
#endif
    sValue = sNewValue;
}
///////////////////////////////////////////////////////
//
//	Name: Name
//	Params: set - new Name, get - nothing
//	Returns: set - nothing, get - Name
//	Description: wrappers for Name member
///////////////////////////////////////////////////////

std::string FXMLElement::Name()
{
    return sName;
}

void FXMLElement::Name(std::string sNewName)
{
#ifdef _DEBUG
    cout << "Setting Name to: " << sNewName.c_str() << endl;
#endif
    sName = sNewName;
}

void FXMLElement::SetAttributePair(std::string sAttributeName, std::string sAttributeValue)
{
#ifdef _DEBUG
    cout << "Found attibute pair: " << sAttributeName.c_str() << " --- " << sAttributeValue.c_str() << endl;
#endif
    
    mAttributes.insert(std::make_pair(sAttributeName, sAttributeValue));
}

///////////////////////////////////////////////////////
//
//	Name: Dump
//	Params: none
//	Returns: XML string of fxmlelement variables
//	Description: dumps element contents to a string.  Does not place XML headers
///////////////////////////////////////////////////////

std::string FXMLElement::Dump()
{
	std::string sRet; //string to return
	std::map<std::string, std::string>::iterator iAttribLoc;	//map iterator
	std::list<FXMLElement*>::iterator iChildLoc;	//iterator for children

	sRet = (std::string)"<" + sName + (std::string)" ";

	if (!mAttributes.empty())
	{
		for (iAttribLoc = mAttributes.begin(); iAttribLoc != mAttributes.end(); ++iAttribLoc)
		{
			sRet += (*iAttribLoc).first + (std::string)"=\"" + (*iAttribLoc).second + (std::string)"\" ";
		}
	}

	//close name/attrib section
	sRet += (std::string)">";
	//add value
	sRet += sValue;

	//now process children
	if (mChildren != NULL)
	{
		if (!mChildren->empty())
		{
    	    for (iChildLoc = mChildren->begin(); iChildLoc != mChildren->end(); ++iChildLoc)
            {
                if (*iChildLoc)
                {
					//recursive call here
					sRet += (*iChildLoc)->Dump();
               }
			}
		}
	}
	//close element
	sRet += (std::string)"</" + sName + (std::string)">\n";
	return sRet;
}

FXMLElement::FXMLElement()
{
	//contstructor. Set stuff to NULL
   mChildren = NULL;
   fxParent = NULL;
}

FXMLElement::~FXMLElement()
{
    //destructor, lets erase everything
    fxParent = NULL;
    mAttributes.clear();
	
    if (mChildren)
    {
        // clear the old stack
        std::list<FXMLElement*>::iterator iter;

        if (mChildren != NULL)
        {

            //now clear and delete the list!
			for (iter = mChildren->begin(); iter != mChildren->end(); ++iter)
			{
				if (*iter)
				{
					delete (*iter);
					(*iter) = NULL;
				}
			}

            mChildren->clear();
            delete mChildren;
            mChildren = NULL;
        }
    }

}


/**  FXMLElement* FXMLElement::findChild(FXMLElement *root, string name)
 *
 *   Added by cmicali
 *     Purpose: find a child below the current node with the passed name
 *              and return pointer to the child node
 */
FXMLElement* FXMLElement::findChild(FXMLElement *root, std::string name) {
    std::list<FXMLElement*>::iterator iter;
    for(iter = root->mChildren->begin(); iter != root->mChildren->end(); ++iter) {
        if (*iter) {
            if ((*iter)->Name() == name)
                return (*iter);
        }
    }
    return NULL;
}

/**  string FXMLElement::getValueFromPath(string path);
 *
 *   Added by cmicali
 *     Purpose: get the value of the node referenced by the dot-path notation
 *              example: 
 *
 *              <config>
 *                <general>
 *                   <size>50</size>
 *                <general>
 *              </config>
 * 
 *              FXMLElement *n = fxml.ParseFile("abovefile.xml");
 *              // n is now a pointer to root node "config"
 *              std::string s = n->getValueFromPath("general.size");
 *              // String s now is "50"
 */
std::string FXMLElement::getValueFromPath(std::string path) {
    int f = path.find(".",0);
    int o = 0;
    std::list<std::string> l;
    path = path + ".";
    while (f != std::string::npos) {
        std::string s = path.substr(o,f-o);
        if (s != "")
            l.push_back(s);
        o = f+1;
        f = path.find(".", o);;
    }
    std::list<std::string>::const_iterator iter;
    iter=l.begin();
    if (iter != l.end()) {
        std::string s = (*iter);
        FXMLElement *n = findChild(this, s);
        iter++;
        if (!n) return "";
        for (; iter != l.end(); iter++) {
            s = (*iter);
            n = findChild(n, s);
            if (!n) return "";
        }
        return n->Value();
    }
    return "";    
}