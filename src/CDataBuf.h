#ifndef CDATABUF_H
#define CDATABUF_H

#define DATABUF_CHUNK_SIZE 1024

#define CDB_SEEK_FROMSTART 1
#define CDB_SEEK_FROMEND   2
#define CDB_SEEK_FROMCUR   3

class CDataBuf {
    
    private:

        char *buffer;
        unsigned int buffsize;
        unsigned int allocsize;
        unsigned int read_cursor;

    public:

        CDataBuf();
        ~CDataBuf();

        int isempty(void);

        void write(const char *buf, int size);
        void pushc(const char c);
        void pushc(const int c);
        void pushuc(const unsigned char c);
        void pushsi(const short int si);
        void pushusi(const unsigned short int usi);
        void pushi(const int i);
        void pushui(const unsigned int ui);
        void pushstr(const char *str);
        void popc(void);
        // Cursor-position edits used by the F10 Song Message editor.
        // insert_at clamps pos to [0, getsize()]; erase_at is a no-op
        // outside [0, getsize()-1] so callers do not have to bounds
        // check. Both keep the buffer null-terminator-free; the
        // caller is responsible for adding one if it needs C-string
        // semantics (CommentEditor maintains its own mirror).
        void insert_at(unsigned int pos, char c);
        void erase_at(unsigned int pos);
        void flush(void);
        char *getbuffer(void);
        int getsize(void);
        void setbufsize(int size);
        int eob(void);

        char getch(void);
        unsigned char getuch(void);
        short int getsi(void);
        unsigned short int getusi(void);
        int geti(void);
        unsigned int getui(void);

        void reset_read(void);
        void seek(int size, int mode);
};

#endif

