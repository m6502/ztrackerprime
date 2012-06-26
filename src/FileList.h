#ifndef ZT_FILELIST_INCLUDED__
#define ZT_FILELIST_INCLUDED__

#include "zt.h"

class DriveList : public ListBox {
    public:

        int updated;
        
        DriveList();
        ~DriveList();
        virtual void draw(Drawable *S, int active);
        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange();
        virtual void enter(void);

};


class DirList : public ListBox {
    public:

        int updated;
        
        DirList();
        ~DirList();
        virtual int update();
        virtual void draw(Drawable *S, int active);
        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange();
        virtual void enter(void);

};

class FileList: public ListBox {
    public:

        int updated;
        ActFunc onEnter;
        
        FileList();
        ~FileList();
        virtual int update();
        virtual void draw(Drawable *S, int active);
        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange();
        virtual void enter(void);
        void AddFiles(char *pattern, TColor c);
};



#endif
