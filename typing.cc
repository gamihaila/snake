#include <curses.h> // required
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

WINDOW *wnd;
int nrows, ncols;

// the keyboard, upper case letters plus punctuation, QWERTY
const char *keys[] = {
  "`1234567890-=",
  "QWERTYUIOP[]\\",
  "ASDFGHJKL;'",
  "ZXCVBNM,./"
};
const int NKEYROWS = 4;
const int KEYPITCH = 4; // one key is " X ", plus a space between keys

// screen layout, adjusted by layout() once the size is known:
// the tall values suit a desktop terminal; on a short screen
// (a phone with a large font) everything packs tighter
int keytop = 3;   // screen row of the first keyboard row
int keystep = 2;  // rows between keyboard rows
int bartop = 11;  // screen row of the space bar
const int BARLEFT = 24;
const int BARLEN = 30;
int septop = 14;  // screen row of the separator line
int texttop = 17; // screen row of the practice line

void layout() {
  if (nrows < 20) {
    keytop = 2;
    keystep = 1;
    bartop = 7;
    septop = 9;
    texttop = 11;
  }
}

// the shift keys flank the bottom letter row; each is a wide
// key labeled SHIFT, drawn like the space bar
const int LSHIFTC = 0;
const int RSHIFTC = 48;
const int SHIFTLEN = 7;

// symbols that share a key: unshifted, and shifted above it
const char *unshifted = "`1234567890-=[]\\;',./";
const char *shifted   = "~!@#$%^&*()_+{}|:\"<>?";

// does typing ch require a shift key?
bool needs_shift(char ch) {
  return isupper((unsigned char)ch) || strchr(shifted, ch) != NULL;
}

// the symbol a key shows while shift is held
char shift_glyph(char ch) {
  const char *p = strchr(unshifted, ch);
  return p ? shifted[p - unshifted] : ch;
}

// which hand types the key in row i at position j: 0 left, 1 right
int key_hand(int i, int j) {
  const int split[] = {5, 4, 4, 4}; // last left-hand key of each row
  return j <= split[i] ? 0 : 1;
}

vector<string> lines;
int line_idx = 0;
int pos = 0;            // cursor position in the current line
long correct = 0;
long wrong = 0;
int case_errors = 0;   // consecutive letters wrong only in case
bool caps_shown = false;
time_t start = 0;       // time of the first keystroke

// the currently inverted key, so it can be drawn plain again
int last_r = -1, last_c = -1, last_len = 0;
char last_ch = ' ';
int last_shift = -1;    // the currently inverted shift key, if any
bool shown_shift = false; // is the keyboard showing shifted symbols?

void draw_key(int r, int c, char ch, int len, bool invert) {
  if (invert) attron(A_REVERSE);
  for (int i = 0; i < len; i++)
    mvaddch(r, c + i, i == len / 2 ? ch : ' ');
  if (invert) attroff(A_REVERSE);
}

// One curses quirk matters here: for a long run of identical
// cells (a row of dashes, a stretch of spaces) curses sends a
// REP escape - "repeat the last character N times" - instead of
// the characters themselves. hterm, the terminal inside iSH,
// ignores REP, so such runs lose all but their first cells: a
// full-width separator shows up as two dashes. The loops that
// draw runs therefore refresh after every cell, which keeps
// each run from ever forming. It costs a few extra refreshes
// and changes nothing on other terminals.
void draw_bar(bool invert) {
  const char *label = "SPACE";
  if (invert) attron(A_REVERSE);
  for (int j = 0; j < BARLEN; j++) {
    int at = j - (BARLEN - 5) / 2;
    mvaddch(bartop, BARLEFT + j, at >= 0 && at < 5 ? label[at] : ' ');
    refresh();
  }
  if (invert) attroff(A_REVERSE);
}

void draw_shift(int side, bool invert) {
  const char *label = "SHIFT";
  int top = keytop + keystep * 3;
  int c = side ? RSHIFTC : LSHIFTC;
  if (invert) attron(A_REVERSE);
  for (int j = 0; j < SHIFTLEN; j++) {
    int at = j - (SHIFTLEN - 5) / 2;
    mvaddch(top, c + j, at >= 0 && at < 5 ? label[at] : ' ');
  }
  if (invert) attroff(A_REVERSE);
}

// find the screen position of the key for character ch
bool find_key(char ch, int *r, int *c, int *len, char *shown,
              int *side) {
  *side = -1;
  if (ch == ' ') {
    *r = bartop; *c = BARLEFT; *len = BARLEN; *shown = ' ';
    return true;
  }
  // shifted punctuation lives on the unshifted key
  bool shift = needs_shift(ch);
  const char *p = strchr(shifted, ch);
  if (p) ch = unshifted[p - shifted];
  ch = toupper(ch);
  for (int i = 0; i < NKEYROWS; i++) {
    const char *q = strchr(keys[i], ch);
    if (q) {
      *r = keytop + keystep * i;
      *c = 2 + 2 * i + KEYPITCH * (int)(q - keys[i]);
      *len = 3;
      *shown = ch;
      // a shifted character is typed with the opposite hand's shift
      if (shift) *side = key_hand(i, (int)(q - keys[i])) ? 0 : 1;
      return true;
    }
  }
  return false;
}

void draw_keyboard(bool shifted_mode) {
  for (int i = 0; i < NKEYROWS; i++)
    for (int j = 0; keys[i][j]; j++) {
      char ch = keys[i][j];
      mvaddch(keytop + keystep * i, 2 + 2 * i + KEYPITCH * j + 1,
              shifted_mode ? shift_glyph(ch) : ch);
    }
  draw_bar(false);
  draw_shift(0, false);
  draw_shift(1, false);
}

void unhighlight() {
  if (last_r < 0) return;
  if (last_len >= BARLEN) draw_bar(false);
  else draw_key(last_r, last_c,
                shown_shift ? shift_glyph(last_ch) : last_ch,
                last_len, false);
  if (last_shift >= 0) draw_shift(last_shift, false);
  last_r = -1;
  last_shift = -1;
}

// the inverted key is the one for the NEXT letter to be typed:
// the keyboard shows you where to go, not where you have been
void highlight_next() {
  unhighlight();
  if (line_idx >= (int)lines.size()) return;
  char want = lines[line_idx][pos];
  // while the next character needs shift, the keyboard shows the
  // shifted symbols, as if the key were already held down
  bool shift = needs_shift(want);
  if (shift != shown_shift) {
    shown_shift = shift;
    draw_keyboard(shown_shift);
  }
  int r, c, len, side;
  char shown;
  if (find_key(want, &r, &c, &len, &shown, &side)) {
    if (len >= BARLEN) draw_bar(true);
    else draw_key(r, c, shown_shift ? shift_glyph(shown) : shown,
                  len, true);
    last_r = r; last_c = c; last_len = len; last_ch = shown;
    if (side >= 0) {
      draw_shift(side, true);
      last_shift = side;
    }
  }
}

long elapsed() {
  return start ? time(NULL) - start : 0;
}

int wpm() {
  long secs = elapsed();
  if (!secs) return 0;
  return (int)((correct / 5.0) / (secs / 60.0));
}

double accuracy() {
  long total = correct + wrong;
  return total ? 100.0 * correct / total : 100.0;
}

// Caps lock cannot be asked about - a terminal program only
// sees the characters it receives. But it can be inferred: a
// typist who keeps producing the right letter in the wrong
// case almost certainly has caps lock on. After CAPS_AFTER
// such errors in a row, say so on the empty line under the
// stats, until an exactly right keystroke clears the suspicion.
const int CAPS_AFTER = 3;

void draw_caps() {
  bool show = case_errors >= CAPS_AFTER;
  if (show == caps_shown) return;
  caps_shown = show;
  if (show) {
    attron(A_REVERSE);
    mvprintw(1, 0, " CAPS LOCK ");
    attroff(A_REVERSE);
  } else {
    mvprintw(1, 0, "           ");
  }
}

void draw_stats() {
  move(0, 0);
  clrtoeol(); // a shorter value must not leave the old tail behind
  mvprintw(0, 0, "Accuracy: %.1f%%", accuracy());
  mvprintw(0, 24, "Speed: %d wpm", wpm());
  long secs = elapsed();
  char buf[32];
  sprintf(buf, "Time: %02ld:%02ld", (long)(secs / 60), (long)(secs % 60));
  mvprintw(0, ncols - (int)strlen(buf), "%s", buf);
}

void draw_line() {
  string line = lines[line_idx];
  for (int j = 0; j < (int)line.size(); j++) {
    // the next letter is inverted, like its key; the hardware
    // cursor is parked in a corner so it cannot hide the letter
    if (j == pos) attron(A_REVERSE);
    mvaddch(texttop, 2 + j, line[j]);
    if (j == pos) attroff(A_REVERSE);
  }
  move(nrows - 1, ncols - 1);
}

void load(const string& filename) {
  ifstream file;
  file.open(filename);
  string line;
  while (getline(file, line)) {
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);
    if (line.empty()) continue;
    // split lines that do not fit on the screen
    int width = ncols - 4;
    while ((int)line.size() > width) {
      lines.push_back(line.substr(0, width));
      line = line.substr(width);
    }
    lines.push_back(line);
  }
  file.close();
}

int main(int argc, char **argv) {
  if (argc < 2) {
    cout << "usage: typing <practice file>" << endl;
    return 1;
  }

  wnd = initscr();
  cbreak();
  noecho();
  getmaxyx(wnd, nrows, ncols);
  layout();
  clear();

  load(argv[1]);
  if (lines.empty()) {
    endwin();
    cout << "typing: no practice text in " << argv[1] << endl;
    return 1;
  }

  draw_keyboard(false);
  for (int c = 0; c < ncols; c++) {
    mvaddch(septop, c, '-');
    refresh(); // per cell, so no REP run forms (see draw_bar)
  }
  draw_stats();
  highlight_next();
  draw_line();
  refresh();

  wtimeout(wnd, 100); // wake up regularly to update the clock
  bool done = false;
  while (!done) {
    int ch = wgetch(wnd);
    if (ch == ERR) { // no key yet, just keep the clock honest
      draw_stats();
      move(nrows - 1, ncols - 1); // keep the cursor parked
      refresh();
      continue;
    }
    if (ch == 27) break; // ESC
    if (!start) start = time(NULL);
    if (ch == lines[line_idx][pos]) {
      correct++;
      case_errors = 0;
      pos++;
      if (pos >= (int)lines[line_idx].size()) {
        // line finished: clear it and bring up the next one
        for (int j = 0; j < (int)lines[line_idx].size(); j++) {
          mvaddch(texttop, 2 + j, ' ');
          refresh(); // per cell, so no REP run forms
        }
        line_idx++;
        pos = 0;
        if (line_idx >= (int)lines.size()) done = true;
      }
    } else {
      wrong++;
      // a letter that is wrong only in case points at caps lock
      char target = lines[line_idx][pos];
      if (isalpha((unsigned char)ch) &&
          isalpha((unsigned char)target) &&
          tolower((unsigned char)ch) ==
              tolower((unsigned char)target))
        case_errors++;
      else
        case_errors = 0;
      beep();
    }
    if (!done) highlight_next();
    draw_stats();
    draw_caps();
    if (!done) draw_line();
    refresh();
  }

  // final score
  wtimeout(wnd, -1);
  clear();
  long secs = elapsed();
  mvprintw(nrows / 2 - 2, 4, "Finished: %s", argv[1]);
  mvprintw(nrows / 2, 4, "Time: %02ld:%02ld", (long)(secs / 60), (long)(secs % 60));
  mvprintw(nrows / 2 + 1, 4, "Accuracy: %.1f%%", accuracy());
  mvprintw(nrows / 2 + 2, 4, "Speed: %d wpm", wpm());
  mvprintw(nrows / 2 + 4, 4, "Press any key.");
  refresh();
  getch();
  endwin(); // curses call to restore the original window and leave
  return 0;
}
