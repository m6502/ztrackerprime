/*************************************************************************
 *
 * FILE  list.cpp
 * $Id: list.cpp,v 1.3 2001/07/27 12:59:39 cmicali Exp $
 *
 * DESCRIPTION 
 *   Generic linked list class.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2000-2001, Christopher Micali <micali@concentric.net>
 * Copyright (c) 2001, Daniel Kahlin <tlr@users.sourceforge.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their
 *    contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS´´ AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******/

#include "stdafx.h"
#include "zt.h"

node::node() {
    str = NULL;
    strdata = NULL;
    this->next = NULL;
}

node::~node() {
    if (str)
        delete[] str;
    if (strdata)
        delete[] strdata;
}

void list::clearstr(char *s1,int size)
{
    for(int i=0;i<size;i++)
        s1[i]=0;
}
list::list()
{
    head = new node;
    tail = new node;
    head->next = tail;
    tail->next = NULL;
}
list::~list()
{
    node *temp,*temp2;
    if (head->next != tail) {
        temp = head->next;
        while (temp != tail)
        {
            temp2 = temp->next;
            delete temp;
            temp = temp2;       
        }
    }
    delete head;
    delete tail;
}
int list::isempty(void)
{
    return (head->next == tail);
}
int list::size(void)
{
    int i=0;
    node *temp=head->next;
    while (temp != tail)
    {
        i++;
        temp = temp->next;
    }
    return i;
}
void *list::findnode(char *str)
{
    node *temp;
    temp = head->next;
    if (temp == tail)
        return NULL;
    if (!strcmp(temp->str,str))
        return temp;
    temp = temp->next;
    while (temp != tail)
    {
        if (!strcmp(temp->str,str))
            return temp;
        temp = temp->next;
    }
    return NULL;
}
void *list::findprev(node *loc)
{
    node *temp;
    temp = head;
    if (temp->next == loc)
        return temp;
    temp = temp->next;
    while (temp != tail)
    {
        if (temp->next == loc)
            return temp;
        temp = temp->next;
    }
    return NULL;
}   
int list::insert(char *str,char *strdata,listitemtype data)
{
    node *next, *prev;
    if (findnode(str) != NULL)
        return -1;
    prev = (node *)findprev(tail);
    next = new node;
    if (next == NULL)
        return -1;
    prev->next = next;
    next->next = tail;
    next->str = new char[strlen(str)+1];
    if (next->str == NULL)
        return -1;
    //clearstr(next->str,strlen(str)+1);
    strcpy(next->str,str);
    next->data = data;
    next->strdata = new char[strlen(strdata)+1];//strdup(strdata);
    strcpy(next->strdata, strdata);
    return 0;
}
int list::remove(char *str)
{
    node *temp,*prev;
    temp = (node *)findnode(str);
    if (temp == NULL)
        return -1;
    prev = (node *)findprev(temp);
    if (prev == NULL)
        return -1;
    prev->next = temp->next;
//  delete[] temp->str;
//  delete[] temp->strdata;
    delete temp;
    return 0;
}
listitemtype list::getdata(char *str)
{
    node *temp;
    temp = (node *)findnode(str);
    if (temp==NULL)
        return -1;
    return temp->data;
}
char* list::getstrdata(char *str)
{
    node *temp;
    temp = (node *)findnode(str);
    if (temp==NULL)
        return NULL;
    return temp->strdata;
}
int list::setdata(char *str, char *strdata,listitemtype data)
{
    node *temp;
    temp = (node *)findnode(str);
    if (temp==NULL)
        return -1;
    temp->data = data;
    //temp->strdata = strdup(strdata);
    temp->strdata = new char[strlen(strdata)];
    strcpy(temp->strdata, strdata);
    return 0;
}
void list::reset(void) {
    cur = head->next;
}

char *list::getnextkey(void) {
    char *a;
    if (cur == tail)
        return NULL;
    a = cur->str;
    cur = cur->next;
    return a;
}
