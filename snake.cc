#include <curses.h> // required
#include <time.h>
#include <iostream>
#include <fstream>
#include <queue>

using namespace std;
//typedef pair<int, int> Point;
WINDOW *wnd;
char d = 'm';
chtype existing[300];
bool grow = false;
int sleepms = 100;
int idx = 0;
bool replay = false;

string history = "";

struct Point {
  Point(int r = 0, int c = 0) {
    first = r;
    c = second;
  }
  int first, second;
}; 


void food();

void del(Point point) {
  move(point.first, point.second);
  addch(' ');
  move(point.first, point.second);
}

char read(Point point) {
  mvinchstr(point.first, point.second, existing);
  return existing[0];
}

void save(const string& history) {
  string filename = "snake.hist";
  FILE *f = fopen("snake.hist", "w");
  fputs(history.c_str(), f);
  fclose(f);
}

void load(const string& filename) {
  ifstream file;
  file.open(filename);
  getline(file, history);
  file.close();
}

void draw(Point point, char c = '#') {
  char ch = read(point);
  if (ch == 'o') {
    beep();
    grow = true;
    // food();
  } else if (ch != ' ' ) {
    beep();
    flash();
    cbreak();
    getch();
    save(history);
    exit(1);
  }
  move(point.first, point.second);
  addch(c);
  move(point.first, point.second);
}



int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

void get_pos(int *r, int *c) {
  msleep(sleepms);
  do {
    int input = replay ? history[idx++] : wgetch(wnd);
    if (input != ERR
        && ((d == 'q' && input != 'a') ||
            (d == 'a' && input != 'q') ||
            (d == 'm' && input != 'n') ||
            (d == 'n' && input != 'm'))) {
      d = input;
    }
    history += d;
    switch(d) {
    case 'q': (*r)--; return; // move up
    case 'a': (*r)++; return; // move down
    case 'n': (*c)--; return; // move left
    case 'm': (*c)++; return; // move right
    }
  } while(true);
}

void get_point(Point* point) {
  get_pos(&point->first, &point->second);
}

void food() {
  int nrows, ncols;
  getmaxyx(wnd,nrows,ncols);
  while(true) {
    Point point(rand() % nrows, rand() % ncols);
    if (read(point) == ' ') {
      draw(point, 'o');
      break;
    }
  }
}

void line(Point start, Point end) {
  if (start.first == end.first) {
    for (Point p(start.first, start.second); p.second < end.second; p.second++) {
      move(p.first, p.second);
      addch('%');
      move(p.first, p.second);
    }
  }
  if (start.second == end.second) {
    for (Point p(start.first, start.second); p.first < end.first; p.first++) {
      move(p.first, p.second);
      addch('%');
      move(p.first, p.second);
    }
  }
}

void oneRoom(int nrows, int ncols) {
  line(Point(0, 0), Point(0, ncols));
  line(Point(nrows, 0), Point(nrows, ncols));
  line(Point(0, 0), Point(nrows, 0));
  line(Point(0, ncols), Point(nrows, ncols));
}

void twoRooms(int nrows, int ncols) {
  oneRoom(nrows, ncols);
  line(Point(0, ncols/2), Point(nrows/2 - 3, ncols/2));
  line(Point(nrows/2 + 3, ncols/2), Point(nrows, ncols/2));
}


int main(int argc, char **argv) {
  int r, c, nrows, ncols;
  const int LEN = 20;

  wnd = initscr();
  cbreak();
  halfdelay(1);
  noecho();
  getmaxyx(wnd,nrows,ncols);
  clear();
  refresh();
  Point point;

  if (argc > 1) {
    replay = true;
    idx = 0;
    load("snake.hist");
  }
  
  oneRoom(nrows, ncols);
  
  r = nrows/2 - 2; c = ncols/2;
  queue<Point> snake;

  for (int i = 0; i < LEN; ++i) {
    Point point(r, c++);
    snake.push(point);
    draw(snake.back());
  }

  for (int i = 0; i < 100; i++) {
    food();
  }
  

  
  while (1) {
    get_point(&point);
    snake.push(point);
    if (!grow) {
      del(snake.front());
      snake.pop();
    } else {
      grow = false;
    }
    draw(snake.back());
    refresh();
  }
  endwin(); // curses call to restore the original window and leave
}
