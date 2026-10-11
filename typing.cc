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
const int KEYTOP = 3;   // screen row of the first keyboard row
const int KEYPITCH = 4; // one key is " X ", plus a space between keys

// the space bar
const int BARTOP = 11;
const int BARLEFT = 24;
const int BARLEN = 30;

const int TEXTTOP = 17; // screen row of the practice line

vector<string> lines;
int line_idx = 0;
int pos = 0;            // cursor position in the current line
long correct = 0;
long wrong = 0;
time_t start = 0;       // time of the first keystroke

// the currently inverted key, so it can be drawn plain again
int last_r = -1, last_c = -1, last_len = 0;
char last_ch = ' ';

void draw_key(int r, int c, char ch, int len, bool invert) {
  if (invert) attron(A_REVERSE);
  for (int i = 0; i < len; i++)
    mvaddch(r, c + i, i == len / 2 ? ch : ' ');
  if (invert) attroff(A_REVERSE);
}

void draw_bar(bool invert) {
  const char *label = "SPACE";
  if (invert) attron(A_REVERSE);
  for (int j = 0; j < BARLEN; j++) {
    int at = j - (BARLEN - 5) / 2;
    mvaddch(BARTOP, BARLEFT + j, at >= 0 && at < 5 ? label[at] : ' ');
  }
  if (invert) attroff(A_REVERSE);
}

// find the screen position of the key for character ch
bool find_key(char ch, int *r, int *c, int *len, char *shown) {
  if (ch == ' ') {
    *r = BARTOP; *c = BARLEFT; *len = BARLEN; *shown = ' ';
    return true;
  }
  // shifted punctuation lives on the unshifted key
  const char *shifted   = "~!@#$%^&*()_+{}|:\"<>?";
  const char *unshifted = "`1234567890-=[]\\;',./";
  const char *p = strchr(shifted, ch);
  if (p) ch = unshifted[p - shifted];
  ch = toupper(ch);
  for (int i = 0; i < NKEYROWS; i++) {
    const char *q = strchr(keys[i], ch);
    if (q) {
      *r = KEYTOP + 2 * i;
      *c = 2 + 2 * i + KEYPITCH * (int)(q - keys[i]);
      *len = 3;
      *shown = ch;
      return true;
    }
  }
  return false;
}

void draw_keyboard() {
  for (int i = 0; i < NKEYROWS; i++)
    for (int j = 0; keys[i][j]; j++)
      mvaddch(KEYTOP + 2 * i, 2 + 2 * i + KEYPITCH * j + 1, keys[i][j]);
  draw_bar(false);
}

void unhighlight() {
  if (last_r < 0) return;
  if (last_len >= BARLEN) draw_bar(false);
  else draw_key(last_r, last_c, last_ch, last_len, false);
  last_r = -1;
}

// the inverted key is the one for the NEXT letter to be typed:
// the keyboard shows you where to go, not where you have been
void highlight_next() {
  unhighlight();
  if (line_idx >= (int)lines.size()) return;
  int r, c, len;
  char shown;
  if (find_key(lines[line_idx][pos], &r, &c, &len, &shown)) {
    if (len >= BARLEN) draw_bar(true);
    else draw_key(r, c, shown, len, true);
    last_r = r; last_c = c; last_len = len; last_ch = shown;
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

void draw_stats() {
  mvprintw(0, 0, "Accuracy: %.1f%%", accuracy());
  mvprintw(0, 24, "Speed: %d wpm", wpm());
  long secs = elapsed();
  char buf[32];
  sprintf(buf, "Time: %02ld:%02ld", secs / 60, secs % 60);
  mvprintw(0, ncols - (int)strlen(buf), "%s", buf);
}

void draw_line() {
  string line = lines[line_idx];
  for (int j = 0; j < (int)line.size(); j++) {
    if (j == pos) attron(A_UNDERLINE);
    mvaddch(TEXTTOP, 2 + j, line[j]);
    if (j == pos) attroff(A_UNDERLINE);
  }
  move(TEXTTOP, 2 + pos);
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
  clear();

  load(argv[1]);
  if (lines.empty()) {
    endwin();
    cout << "typing: no practice text in " << argv[1] << endl;
    return 1;
  }

  draw_keyboard();
  for (int c = 0; c < ncols; c++)
    mvaddch(14, c, '-');
  mvprintw(nrows - 1, 0, "ESC: quit");
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
      move(TEXTTOP, 2 + pos);
      refresh();
      continue;
    }
    if (ch == 27) break; // ESC
    if (!start) start = time(NULL);
    if (ch == lines[line_idx][pos]) {
      correct++;
      pos++;
      if (pos >= (int)lines[line_idx].size()) {
        // line finished: clear it and bring up the next one
        for (int j = 0; j < (int)lines[line_idx].size(); j++)
          mvaddch(TEXTTOP, 2 + j, ' ');
        line_idx++;
        pos = 0;
        if (line_idx >= (int)lines.size()) done = true;
      }
    } else {
      wrong++;
      beep();
    }
    if (!done) highlight_next();
    draw_stats();
    if (!done) draw_line();
    refresh();
  }

  // final score
  wtimeout(wnd, -1);
  clear();
  long secs = elapsed();
  mvprintw(nrows / 2 - 2, 4, "Finished: %s", argv[1]);
  mvprintw(nrows / 2, 4, "Time: %02ld:%02ld", secs / 60, secs % 60);
  mvprintw(nrows / 2 + 1, 4, "Accuracy: %.1f%%", accuracy());
  mvprintw(nrows / 2 + 2, 4, "Speed: %d wpm", wpm());
  mvprintw(nrows / 2 + 4, 4, "Press any key.");
  refresh();
  getch();
  endwin(); // curses call to restore the original window and leave
  return 0;
}
