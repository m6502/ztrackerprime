zTracker'
=========

zTracker was a Win32 only MIDI tracker / sequencer modeled after Impulse Tracker / Scream Tracker. 

zTracker' is, as it's name implies, a derivate of zTracker.

This fork is released as a homage to the original work of Chris Micali. I have been using zTracker
nearly on a daily basis for the past 12 years. As the original project has remained untouched since
2002, at some point I started tinkering with the source code, at first to fix bugs and later to
change how the program behaves.

I don't know if there are any zTracker users remaining out there, but as I'm preparing to try a big
overhaul of this software that could potentially end as a whole new program, this is released with
the hope that someone can find it useful.

*Please be advised that I can't guarantee this version works better for you than the original*

Everyone does have his own tastes, routines and circumstances. zTracker' behaves nearly flawlessly
for me and I like it better than the original. But in your case could be a different history. It is
a good idea to check the following list to understand what this fork is about.

There's also the possibility that I misunderstood how something worked and "fixing" it I broke it,
or that I have changed the way something works that could have been activated by just pressing a 
key. Who knows?


So, what's new / different from the original zTracker?

Program behavior changes:

- Song play view shows more information per note.
- Starts in "Tracker Mode" by default.
- Instrument editor shows a small dot '·' on the left side of any instrument used in the last play.
- Playing the current row only plays non silenced channels.
- Cursor steps when playing the current note / row with '4' and '8' keys.
- Default pattern size is 64 instead of 128.
- Program doesn't allow the user to set a resolution lower than 1024x700 and will increase the X, Y 
  or both of them if necessary.
- Play view draws as many channels as it can fit within the horizontal resolution, instead of a 
  fixed number of them.
- Load and Save screens now use all the available space to display their list entries, and have an
  improved X size.
- File requester didn't show .mid files; now it does.


Fixed bugs:

- Using F6 to play current pattern now works even if it's not included in the song order list.
- Scroll didn't move when playing individual notes and rows.
- Current pattern being edited didn't display correctly the row numbers if it had a different count
  than the default.
- Last line remained highlighted when play entered the current pattern then exited to another.
- You could use the numeric keypad '+' to advance the song position, but couldn't use the '-' to
  rewind it.


Removed things:

- Initial information screen has been removed.
- VUMeters can't be enabled. It's been broken forever, and I don't use it - Will reenable when it's
  fixed.
- 8 bit .png files are not supported anymore.
- Comments when exporting .mid files have been removed (I needed them to be as small as possible).
- The weird "Are you crazy" requester has been removed, substituted by just stopping the current
  song and loading the new one.
- Column size can't be changed.


Broken things:

- zt.conf configuration file needs to be edited by hand.
- Full screen mode doesn't work with the weird resolutions you can set on the windowed mode :-)
- The only known crash happens sometimes when loading a song from disk. Luckily for obvious reasons
  it is impossible to lose any useful work, because you were already going to load a new file.


Other changes:

- The source code has been updated where needed to compile with modern C++.
- External libraries (zlib, libpng, SDL 1.2) are again up to date.
- zlib and libpng are now statically linked instead of using external .dll files.
- The (good old) Visual C++ 6.0 project and solution have been replaced by Visual Studio 2008 
  versions.
- Parts of the code have been cleaned, re-indented, commented and altered to suit my tastes. I
  sincerely hope this is not seen as a rude move at all. I needed to do it to understand what I was 
  reading.
- Directory tree has been changed.
- Version number scheme and program name have been changed, too.


Well, this is a nearly complete list of changes from the original program. It is not perfect though, 
because it's been a lot of years since I started playing with the source code. I can't be sure if 
I really documented ALL the changes I performed. In fact, I know that, at least, I forgot to write 
one of them, so there can be more!

I'm open to your feedback. I love this software and I will try to fix all the bugs I could have
introduced with my changes, plus the bugs from the 2002 version I didn't catch. Please, don't 
hesitate to contact me about this matter!
