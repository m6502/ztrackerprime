#include "zt.h"
#include "FileList.h"

#include <filesystem>


// ------------------------------------------------------------------------------------------------
//
//
DriveList::DriveList() {
    empty_message = "No drives found";
    is_sorted = true;
    OnChange();
    updated = 1;
    
}



// ------------------------------------------------------------------------------------------------
//
//
void DriveList::enter(void) {
    OnChange();
}



// ------------------------------------------------------------------------------------------------
//
//
void DriveList::draw(Drawable *S, int active) {
    color_itemnosel = &COLORS.Data;
    ListBox::draw(S,active);
    updated = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
void DriveList::OnChange() 
{
  int i ;

    clear();        
    int r;
//    unsigned char *s;
    char str[4];
    unsigned char cap[16],cur[512],save[16];
    strcpy(str,"A:\\");
    r = GetLogicalDrives();
    int cd=0;
    LBNode *p;
    save[0] = 0;
    if(!already_changed_default_directory)
    {
        if(zt_config_globals.default_directory[0] != '\0')
            SetCurrentDirectory((LPCTSTR)zt_config_globals.default_directory);
        already_changed_default_directory = 1;
    }
    
    GetCurrentDirectory(512,(LPSTR)cur);
    
    for(i=0;i<26;i++) {
        if (r&1) {
            strcpy((char *)&cap[0], "  A:");
            cap[2]+=i;
            str[0] = 'A' + i;
            switch(GetDriveType(str)) {
                case DRIVE_CDROM:
                case DRIVE_REMOVABLE:
                    cap[0] = (unsigned char)225;
                    break;
                default:
                    cap[0] = (unsigned char)224;
                    break;
            }
            p = insertItem((char *)&cap[0]);
            p->int_data = i;
            if (tolower(cur[0])==tolower(str[0]))
                strcpy((char *)save,(const char *)cap);
        }
        r>>=1;
    }
    i = findItem((char *)save);
    if (i>=0)
        setCursor(i);
       
}


// ------------------------------------------------------------------------------------------------
//
//
void DriveList::OnSelect(LBNode *selected) {
    std::filesystem::current_path(&selected->caption[2]);
    OnChange();
    updated++;
    ListBox::OnSelect(selected);
}



// ------------------------------------------------------------------------------------------------
//
//
DirList::DirList() 
{
    empty_message = "";
    is_sorted = true;
    use_checks = false;
    updated = 1;
    OnChange();
}



// ------------------------------------------------------------------------------------------------
//
//
void DirList::enter(void) {
    OnChange();
}



// ------------------------------------------------------------------------------------------------
//
//
int DirList::update() {
    int key = Keys.checkkey();
    int act=0;
    int ret=0;
    switch(key) {
        case DIK_LEFT: ret = -1; act++; break;
    }
    if (act) {
        Keys.getkey();
        need_refresh++;
        need_redraw++;
        return ret;
    } else
    return ListBox::update();
}



// ------------------------------------------------------------------------------------------------
//
//
void DirList::draw(Drawable *S, int active) {
    ListBox::draw(S,active);
    updated = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
bool is_root_directory(const std::filesystem::path &dir_path)
{
    if (std::filesystem::exists(dir_path) && std::filesystem::is_directory(dir_path)) {

        bool result = (dir_path == dir_path.root_path()) ;
        return result ;
    }

    return false;
}




// ------------------------------------------------------------------------------------------------
//
//
void DirList::OnChange()
{
    clear();

    std::filesystem::path current_dir = std::filesystem::current_path();

    if(!is_root_directory(current_dir))
    {
        LBNode* p = insertItem((char *)"..");
    }

    for (const auto& entry : std::filesystem::directory_iterator(".")) {

        if (entry.is_directory()) {

            const std::string dirName = entry.path().filename().string();
            LBNode* p = insertItem((char *)dirName.c_str());
        }
    }

/*
    long res,hnd;
    _finddata_t f;
    hnd = res = _findfirst("*.*",&f);
    while(res != -1) {
        if (f.attrib & _A_SUBDIR) {
            LBNode *p  = insertItem(f.name);
        }
        res = _findnext(hnd, &f);
    }
    _findclose(hnd);
*/
    need_redraw++;
}




// ------------------------------------------------------------------------------------------------
//
//
void DirList::OnSelect(LBNode *selected) {
    std::filesystem::current_path(selected->caption);
    OnChange();
    updated++;
    char d[1024];
    GetCurrentDirectory(1024,d);
    sprintf(szStatmsg,"Browsing %.70s",d);
    statusmsg = szStatmsg;
    status_change = 1;    
    ListBox::OnSelect(selected);
}




// ------------------------------------------------------------------------------------------------
//
//
FileList::FileList() 
{
    empty_message = "";
    is_sorted = true;
    use_checks = false;
    updated = 1;
    OnChange();
    onEnter = NULL;
}



// ------------------------------------------------------------------------------------------------
//
//
FileList::~FileList() {
}



// ------------------------------------------------------------------------------------------------
//
//
int FileList::update() 
{
    int key = Keys.checkkey();
    int act=0;
    int ret=0;
    switch(key) {
        case DIK_RIGHT: ret = 1; act++; break;
    }
    if (act) {
        Keys.getkey();
        need_refresh++;
        need_redraw++;
        return ret;
    } else
    return ListBox::update();
}



// ------------------------------------------------------------------------------------------------
//
//
void FileList::enter(void) {
    OnChange();
}




// ------------------------------------------------------------------------------------------------
//
//
void FileList::draw(Drawable *S, int active) 
{
  int cy;
  unsigned char *str;

  str = (unsigned char *)malloc(xsize+1+2);
  _ASSERT(str) ;

  LBNode *node = getNode(y_start);

  for (cy=0; cy <= ysize; cy++) {

    memset(str, ' ', xsize) ;
    str[xsize] = NULL ;

    TColor foreground_color = COLORS.EditText;
    TColor background_color = COLORS.EditBG;

    if (node) {

      strc((char *)str, node->caption);
      foreground_color = node->int_data;
      node = node->next;
    } 

    if (cy == cur_sel) {

      if (active) background_color = COLORS.Highlight;
      else        background_color = COLORS.EditText;

      foreground_color = COLORS.EditBG;
    } 

    if (num_elements == 0 && cy==0) {

      if (empty_message) strc((char*)str, empty_message);
    }

    printBGu(col(x), row(cy+y), str, foreground_color, background_color, S) ;
  }

  frm.type=0;

  frm.x=x;
  frm.y=y;

  frm.xsize=xsize;
  frm.ysize=ysize+1;

  frm.draw(S,0);

  screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+ysize+1));

  free(str);
  updated = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void FileList::AddFiles(char *pattern, TColor c) 
{
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        // Match the pattern (simple example using filename)

        if (entry.is_regular_file() && entry.path().extension() == pattern) {
            LBNode* p = insertItem((char *)entry.path().filename().string().c_str());
            p->int_data = c; // Set the integer data
        }
    }
}




// ------------------------------------------------------------------------------------------------
//
//
void FileList::OnChange() 
{
    clear();
    AddFiles(".zt", COLORS.Data);
    AddFiles(".it", COLORS.Highlight);
    
    // <Manu> Quiero que muestre tambien los .mid, asi que añado esta linea
    AddFiles(".mid", COLORS.Highlight);
    
    need_redraw++;
}




// ------------------------------------------------------------------------------------------------
//
//
void FileList::OnSelect(LBNode *selected) {
    if (onEnter)
        onEnter(this);
    updated++;
    ListBox::OnSelect(selected);
    SDL_Delay(1);
}

