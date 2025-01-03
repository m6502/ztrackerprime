#ifndef _LIST_H_
#define _LIST_H_
/*
struct node {
    char *str;
    char *strdata;
    listitemtype data;
    node *next; 
} *head,*tail,*cur;
*/

typedef int listitemtype;

class node {
    public:
        node();
        ~node();

        char *str;
        char *strdata;
        listitemtype data;
        node *next;
};

class list {
private:

  node *head,*tail,*cur;
  
  void *findprev(node *loc);
  void clearstr(char *s1,int size);
public:
  list();
  ~list();
  void reset();
  char *getnextkey();
  int size(void);
  int insert(const char *str,const char *strdata,listitemtype data=0);
  int remove(const char *str);
  int isempty(void);
  int setdata(const char *str, const char *strdata, listitemtype data);
  void *findnode(const char *str);
  listitemtype getdata(const char *str);
  char *getstrdata(const char *str);
};
#endif


