#include "zt.h"
#include "FileList.h"


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
DriveList::~DriveList() {
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
    
    GetCurrentDirectory(512,(LPSTR)&cur[0]);
    
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
    chdir(&selected->caption[2]);
    OnChange();
    updated++;
    ListBox::OnSelect(selected);
}



// ------------------------------------------------------------------------------------------------
//
//
void DriveList::OnSelectChange() {
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
DirList::~DirList() {
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
void DirList::OnChange() {
    clear();
    long res,hnd;
    _finddata_t f;
    hnd = res = _findfirst("*.*",&f);
    while(res != -1) {
        if (f.attrib & _A_SUBDIR) {
            LBNode *p;
//            if (f.name[0] != '.' && f.name[1]!=0){
                p = insertItem(f.name);
//            }
        }
        res = _findnext(hnd, &f);
    }
    _findclose(hnd);
    need_redraw++;
}




// ------------------------------------------------------------------------------------------------
//
//
void DirList::OnSelect(LBNode *selected) {
    chdir(selected->caption);
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
void DirList::OnSelectChange() {
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
  TColor f,b;
  unsigned char *str;

  str = (unsigned char *)malloc(xsize+1+2);
  LBNode *node = getNode(y_start);

  for (cy=0; cy <= ysize; cy++) {

    memset(str,0,xsize+1);
    memset(str,' ',xsize);

    if (node) {
      strc((char *)str, node->caption);
      f = node->int_data;
      node = node->next;
    } 

    if (cy == cur_sel) {

      if (active) b = COLORS.Highlight;
      else b = COLORS.EditText;

      f = COLORS.EditBG;
    } 
    else {

      b = COLORS.EditBG;
    }

    if (num_elements == 0 && cy==0) {

      f = COLORS.EditText;
      b = COLORS.EditBG;
      if (empty_message) strc((char*)str, empty_message);
    }

    printBGu(col(x),row(cy+y),str,f,b,S);
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
    long res,hnd;
    _finddata_t f;
    hnd = res = _findfirst(pattern,&f);
    while(res != -1) {
        LBNode *p;
        p = insertItem(f.name);
        p->int_data = c;
        res = _findnext(hnd, &f);
    }
    _findclose(hnd);
}




// ------------------------------------------------------------------------------------------------
//
//
void FileList::OnChange() 
{
    clear();
    AddFiles("*.zt", COLORS.Data);
    AddFiles("*.it", COLORS.Highlight);
    
    // <Manu> Quiero que muestre tambien los .mid, asi que añado esta linea
    AddFiles("*.mid", COLORS.Highlight);
    
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




// ------------------------------------------------------------------------------------------------
//
//
void FileList::OnSelectChange() {
}



