#ifndef ZT_FILELIST_INCLUDED__
#define ZT_FILELIST_INCLUDED__

#include "UserInterface.h"
#include "file_browser.h"

class DriveList : public ListBox {
    std::vector<file_browser::Place> places;
    public:

        int updated;
        
        DriveList();
        ~DriveList() = default ;
        virtual int update();
        virtual void draw(Drawable *S, int active);
        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange() {}
        virtual void enter(void);

};


class FileList: public ListBox {
    std::vector<file_browser::Entry> entries;
    void navigate(const std::filesystem::path &path);
    std::filesystem::path pending_directory;
    public:

        int updated;
        ActFunc onEnter;
        
        FileList();
        ~FileList();
        virtual int update();
        virtual void draw(Drawable *S, int active);
        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange() {}
        virtual void enter(void);

};



#endif
