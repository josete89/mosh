/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#ifndef TERMINALFB_HPP
#define TERMINALFB_HPP

#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include <vector>
#include <deque>
#include <string>
#include <list>

/* Terminal framebuffer */

namespace Terminal {
  typedef uint16_t color_type;

  class Renditions {
  public:
    typedef enum { bold, faint, italic, underlined, blink, inverse, invisible, SIZE } attribute_type;

    // all together, a 32 bit word now...
    unsigned int foreground_color : 12;
    unsigned int background_color : 12;
  private:
    unsigned int attributes : 8;

  public:
    Renditions( color_type s_background );
    void set_foreground_color( int num );
    void set_background_color( int num );
    void set_rendition( color_type num );
    std::string sgr( void ) const;

    void posterize( void );

    bool operator==( const Renditions &x ) const
    {
      return ( attributes == x.attributes )
        && ( foreground_color == x.foreground_color )
        && ( background_color == x.background_color );
    }
    void set_attribute( attribute_type attr, bool val )
    {
      attributes = val ?
	( attributes | (1 << attr) ) :
	( attributes & ~(1 << attr) );
    }
    bool get_attribute( attribute_type attr ) const { return attributes & ( 1 << attr ); }
    void clear_attributes() { attributes = 0; }
  };

  class Cell {
  public:
    typedef std::string content_type; /* can be std::string, std::vector<uint8_t>, or __gnu_cxx::__vstring */
    content_type contents;
    Renditions renditions;
    uint8_t width;
    bool fallback; /* first character is combining character */

    Cell( color_type background_color );
    Cell(); /* default constructor required by C++11 STL */

    void reset( color_type background_color );

    bool operator==( const Cell &x ) const
    {
      return ( (contents == x.contents)
	       && (fallback == x.fallback)
	       && (width == x.width)
	       && (renditions == x.renditions) );
    }

    bool operator!=( const Cell &x ) const { return !operator==( x ); }

    wint_t debug_contents( void ) const;

    bool is_blank( void ) const
    {
      return ( contents.empty()
	       || contents == " "
	       || contents == "\xC2\xA0" );
    }

    bool contents_match ( const Cell &other ) const
    {
      return ( is_blank() && other.is_blank() )
             || ( contents == other.contents );
    }

    bool compare( const Cell &other ) const;

    static void append_to_str( std::string &dest, const wchar_t c )
    {
      static mbstate_t ps = mbstate_t();
      char tmp[MB_LEN_MAX];
      size_t ignore = wcrtomb(NULL, 0, &ps);
      (void)ignore;
      size_t len = wcrtomb(tmp, c, &ps);
      dest.append( tmp, len );
    }

    void append( const wchar_t c )
    {
      static mbstate_t ps = mbstate_t();
      char tmp[MB_LEN_MAX];
      size_t ignore = wcrtomb(NULL, 0, &ps);
      (void)ignore;
      size_t len = wcrtomb(tmp, c, &ps);
      contents.insert( contents.end(), tmp, tmp+len );
    }
  };

  class Row {
  public:
    typedef std::vector<Cell> cells_type;
    cells_type cells;
    bool wrap; /* if last cell, wrap to next line */
    // gen is a generation counter.  It can be used to quickly rule
    // out the possibility of two rows being identical; this is useful
    // in scrolling.
    uint64_t gen;

    Row( size_t s_width, color_type background_color );
    Row(); /* default constructor required by C++11 STL */

    void insert_cell( int col, color_type background_color );
    void delete_cell( int col, color_type background_color );

    void reset( color_type background_color );

    bool operator==( const Row &x ) const
    {
      return ( gen == x.gen && cells == x.cells && wrap == x.wrap );
    }

    bool get_wrap( void ) const { return wrap; }
    void set_wrap( bool w ) { wrap = w; }

    uint64_t get_gen() const;
  };

  class SavedCursor {
  public:
    int cursor_col, cursor_row;
    Renditions renditions;
    /* not implemented: character set shift state */
    bool auto_wrap_mode;
    bool origin_mode;
    /* not implemented: state of selective erase */

    SavedCursor();
  };

  class DrawState {
  private:
    int width, height;

    void new_grapheme( void );
    void snap_cursor_to_border( void );

    int cursor_col, cursor_row;
    int combining_char_col, combining_char_row;

    bool default_tabs;
    std::vector<bool> tabs;

    void reinitialize_tabs( unsigned int start );

    int scrolling_region_top_row, scrolling_region_bottom_row;

    Renditions renditions;

    SavedCursor save;

  public:
    bool next_print_will_wrap;
    bool origin_mode;
    bool auto_wrap_mode;
    bool insert_mode;
    bool cursor_visible;
    bool reverse_video;
    bool bracketed_paste;

    enum MouseReportingMode {
      MOUSE_REPORTING_NONE = 0,
      MOUSE_REPORTING_X10 = 9,
      MOUSE_REPORTING_VT220 = 1000,
      MOUSE_REPORTING_VT220_HILIGHT = 1001,
      MOUSE_REPORTING_BTN_EVENT = 1002,
      MOUSE_REPORTING_ANY_EVENT = 1003
    } mouse_reporting_mode;

    bool mouse_focus_event;       // 1004
    bool mouse_alternate_scroll;  // 1007

    enum MouseEncodingMode {
      MOUSE_ENCODING_DEFAULT = 0,
      MOUSE_ENCODING_UTF8 = 1005,
      MOUSE_ENCODING_SGR = 1006,
      MOUSE_ENCODING_URXVT = 1015
    } mouse_encoding_mode;

    bool application_mode_cursor_keys;

    /* bold, etc. */

    void move_row( int N, bool relative = false );
    void move_col( int N, bool relative = false, bool implicit = false );

    int get_cursor_col( void ) const { return cursor_col; }
    int get_cursor_row( void ) const { return cursor_row; }
    int get_combining_char_col( void ) const { return combining_char_col; }
    int get_combining_char_row( void ) const { return combining_char_row; }
    int get_width( void ) const { return width; }
    int get_height( void ) const { return height; }

    void set_tab( void );
    void clear_tab( int col );
    void clear_default_tabs( void ) { default_tabs = false; }
    /* Default tabs can't be restored without resetting the draw state. */
    int get_next_tab( void ) const;

    void set_scrolling_region( int top, int bottom );

    int get_scrolling_region_top_row( void ) const { return scrolling_region_top_row; }
    int get_scrolling_region_bottom_row( void ) const { return scrolling_region_bottom_row; }

    int limit_top( void ) const;
    int limit_bottom( void ) const;

    void set_foreground_color( int x ) { renditions.set_foreground_color( x ); }
    void set_background_color( int x ) { renditions.set_background_color( x ); }
    void add_rendition( color_type x ) { renditions.set_rendition( x ); }
    Renditions get_renditions( void ) const { return renditions; }
    int get_background_rendition( void ) const { return renditions.background_color; }

    void save_cursor( void );
    void restore_cursor( void );
    void clear_saved_cursor( void ) { save = SavedCursor(); }

    void resize( int s_width, int s_height );

    DrawState( int s_width, int s_height );

    bool operator==( const DrawState &x ) const
    {
      /* only compare fields that affect display */
      return ( width == x.width ) && ( height == x.height ) && ( cursor_col == x.cursor_col )
	&& ( cursor_row == x.cursor_row ) && ( cursor_visible == x.cursor_visible ) &&
	( reverse_video == x.reverse_video ) && ( renditions == x.renditions ) &&
  ( bracketed_paste == x.bracketed_paste ) && ( mouse_reporting_mode == x.mouse_reporting_mode ) &&
  ( mouse_focus_event == x.mouse_focus_event ) && ( mouse_alternate_scroll == x.mouse_alternate_scroll) &&
  ( mouse_encoding_mode == x.mouse_encoding_mode );
    }
  };

  class Framebuffer {
  public:
    typedef std::vector<wchar_t> title_type;
    typedef std::vector<const Row *> rows_p_type;

  private:
    typedef std::vector<Row> rows_type;
    rows_type rows;
    title_type icon_name;
    title_type window_title;
    unsigned int bell_count;
    bool title_initialized; /* true if the window title has been set via an OSC */

    Row newrow( void ) { return Row( ds.get_width(), ds.get_background_rendition() ); }

  public:
    Framebuffer( int s_width, int s_height );
    DrawState ds;

    void scroll( int N );
    void move_rows_autoscroll( int rows );

    rows_p_type get_p_rows() const
    {
      rows_p_type retval;
      for ( size_t i = 0; i < rows.size(); i++ ) {
	retval.push_back( &rows.at(i) );
      }
      return retval;
    }

    const Row *get_row( int row ) const
    {
      if ( row == -1 ) row = ds.get_cursor_row();

      return &rows.at( row );
    }

    inline const Cell *get_cell( int row = -1, int col = -1 ) const
    {
      if ( row == -1 ) row = ds.get_cursor_row();
      if ( col == -1 ) col = ds.get_cursor_col();

      return &rows.at( row ).cells.at( col );
    }

    Row *get_mutable_row( int row )
    {
      return const_cast<Row *>(get_row( row ));
    }

    inline Cell *get_mutable_cell( int row = -1, int col = -1 )
    {
      return const_cast<Cell *>(get_cell( row, col ));
    }

    Cell *get_combining_cell( void );

    void apply_renditions_to_current_cell( void );

    void insert_line( int before_row, int count );
    void delete_line( int row, int count );

    void insert_cell( int row, int col );
    void delete_cell( int row, int col );

    void reset( void );
    void soft_reset( void );

    void set_title_initialized( void ) { title_initialized = true; }
    bool is_title_initialized( void ) const { return title_initialized; }
    void set_icon_name( const title_type &s ) { icon_name = s; }
    void set_window_title( const title_type &s ) { window_title = s; }
    const title_type & get_icon_name( void ) const { return icon_name; }
    const title_type & get_window_title( void ) const { return window_title; }

    void prefix_window_title( const title_type &s );

    void resize( int s_width, int s_height );

    void reset_cell( Cell *c ) { c->reset( ds.get_background_rendition() ); }
    void reset_row( Row *r ) { r->reset( ds.get_background_rendition() ); }

    void posterize( void );

    void ring_bell( void ) { bell_count++; }
    unsigned int get_bell_count( void ) const { return bell_count; }

    bool operator==( const Framebuffer &x ) const
    {
      return ( rows == x.rows ) && ( window_title == x.window_title ) && ( bell_count == x.bell_count ) && ( ds == x.ds );
    }
  };
}

#endif
