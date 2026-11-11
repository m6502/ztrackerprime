#include "zt.h"
#include "FileList.h"
#include <assert.h>

#include <filesystem>
#include <algorithm>


namespace {
void browser_error(const std::error_code &error) {
    snprintf(szStatmsg, sizeof(szStatmsg), "Cannot browse: %.160s", error.message().c_str());
    statusmsg = szStatmsg;
    status_change = 1;
    need_refresh++;
}

bool change_directory(const std::filesystem::path &path) {
    std::error_code error;
    // Verify readability before changing the process working directory.
    std::filesystem::directory_iterator probe(path, error);
    if (!error) std::filesystem::current_path(path, error);
    if (error) {
        browser_error(error);
        return false;
    }
    char current[1024];
    zt_get_current_directory(sizeof(current), current);
    snprintf(szStatmsg, sizeof(szStatmsg), "Browsing %.160s", current);
    statusmsg = szStatmsg;
    status_change = 1;
    need_refresh++;
    return true;
}
}

// ------------------------------------------------------------------------------------------------
//
//
DriveList::DriveList() {
    empty_message = "No places found";
    show_inactive_selection = false;
    use_checks = false;
    wrap_focus_at_edges = false;
    is_sorted = false;
    OnChange();
    updated = 0;
    
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
    clear();
    places.clear();
#ifdef _WIN32
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (mask & (1u << i)) {
            std::string drive(1, static_cast<char>('A' + i));
            drive += ":";
            places.push_back({drive, drive + "\\"});
        }
    }
#else
    places = file_browser::places();
#endif
    for (size_t i = places.size(); i > 0; --i) {
        auto *node = insertItem(const_cast<char *>(places[i - 1].name.c_str()));
        node->int_data = static_cast<int>(i - 1);
    }
    // Highlight the most specific place containing the current directory.
    std::error_code error;
    auto current = std::filesystem::current_path(error).generic_string();
    size_t best_length = 0;
    for (size_t i = 0; !error && i < places.size(); ++i) {
        auto path = places[i].path.generic_string();
#ifdef _WIN32
        current = file_browser::folded(current);
        path = file_browser::folded(path);
        if (path.size() > 1 && path.back() == '/') path.pop_back();
#endif
        if ((path == "/" || file_browser::within(current, path)) && path.size() > best_length) {
            setCursor(static_cast<int>(i));
            best_length = path.size();
        }
    }
    need_redraw++;

}


// ------------------------------------------------------------------------------------------------
//
//
int DriveList::update() {
    if (Keys.checkkey() == SDLK_RIGHT) {
        Keys.getkey();
        need_refresh++;
        return -1; // Browser is element 0; Places is element 1.
    }
    return ListBox::update();
}

void DriveList::OnSelect(LBNode *selected) {
    if (!selected || selected->int_data < 0 ||
        static_cast<size_t>(selected->int_data) >= places.size()) return;
    if (change_directory(places[selected->int_data].path)) {
        updated = 1;
        need_refresh++;
    }
}

FileList::FileList() 
{
    empty_message = (char *)"";
    is_sorted = false;
    use_checks = false;
    // File Load/Save users navigate by arrow + Enter; the base
    // ListBox behavior of jumping focus to a sibling list when Up/Down
    // hits an edge sent them into the wrong list and lost their place.
    // Clamp at edges instead — Tab still moves between lists.
    wrap_focus_at_edges = false;
    updated = 0;
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
        case SDLK_LEFT: ret = 1; act++; break;
        case SDLK_BACKSPACE:
            Keys.getkey();
            navigate("..");
            need_refresh++;
            return 0;
    }
    if (act) {
        Keys.getkey();
        need_refresh++;
        need_redraw++;
        return ret;
    }
    int result = ListBox::update();
    // Mouse activation runs before update(). Rebuild only after ListBox has
    // finished processing the click against the old rows.
    if (!pending_directory.empty()) {
        auto path = pending_directory;
        pending_directory.clear();
        navigate(path);
    }
    return result;
}



// ------------------------------------------------------------------------------------------------
//
//
void FileList::enter(void) {
    // Preserve selection when focus returns from Places or the save controls.
}




// ------------------------------------------------------------------------------------------------
//
//
void FileList::draw(Drawable *S, int active) 
{
  int cy;
  unsigned char *str;

  str = (unsigned char *)malloc(xsize+1+2);
  assert(str);

  LBNode *node = getNode(y_start);

  for (cy=0; cy <= ysize; cy++) {

    memset(str, ' ', xsize) ;
    str[xsize] = 0;

    TColor foreground_color = COLORS.EditText;
    TColor background_color = COLORS.EditBG;

    if (node) {

      strc((char *)str, node->caption);
      const auto &entry = entries[node->int_data];
      foreground_color = entry.directory ? COLORS.EditText :
          (file_browser::folded(std::filesystem::path(entry.name).extension().string()) == ".zt"
              ? COLORS.Data : COLORS.Highlight);
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
void FileList::OnChange()
{
    clear();
    typeahead_len = 0;
    std::error_code error;
    auto current = std::filesystem::current_path(error);
    if (!error) entries = file_browser::entries(current, error);
    else entries.clear();
    // ListBox's unsorted insertion prepends, so insert in reverse order.
    for (size_t i = entries.size(); i > 0; --i) {
        const auto &entry = entries[i - 1];
        auto caption = entry.name + (entry.directory && entry.name != ".." ? "/" : "");
        auto *node = insertItem(const_cast<char *>(caption.c_str()));
        node->int_data = static_cast<int>(i - 1);
    }
    if (error) browser_error(error);
    need_redraw++;
}

void FileList::navigate(const std::filesystem::path &path)
{
    std::error_code error;
    auto previous = std::filesystem::current_path(error);
    if (!change_directory(path)) return;
    OnChange();
    updated = 1;
    if (path == ".." && !error) {
        auto caption = previous.filename().string() + "/";
        setCursor(findItem(const_cast<char *>(caption.c_str())));
    }
}

void FileList::OnSelect(LBNode *selected)
{
    if (!selected || selected->int_data < 0 ||
        static_cast<size_t>(selected->int_data) >= entries.size()) return;
    auto entry = entries[selected->int_data];
    if (entry.directory) pending_directory = entry.name;
    else if (onEnter) onEnter(this);
    need_refresh++;
}
