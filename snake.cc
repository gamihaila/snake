#include <curses.h> // required
#include <time.h>
#include <iostream>
#include <queue>

using namespace std;
typedef pair<int, int> Point;
WINDOW *wnd;
char d = 'm';
chtype existing[300];
bool grow = false;
int sleepms = 100;

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

void draw(Point point, char c = '#') {
  char ch = read(point);
  if (ch == 'o') {
    beep();
    grow = true;
    food();
  } else if (ch != ' ' ) {
    beep();
    flash();
    cbreak();
    getch();
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
    int input = wgetch(wnd);
    if (input != ERR
        && ((d == 'q' && input != 'a') ||
            (d == 'a' && input != 'q') ||
            (d == 'm' && input != 'n') ||
            (d == 'n' && input != 'm'))) {
      d = input;
    }
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
  Point point;
  while(true) {
    point.first = rand() % nrows;
    point.second = rand() % ncols;
    if (read(point) == ' ') {
      draw(point, 'o');
      break;
    }
  }
}

int main() {
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

  point.first = 0;
  for (point.second = 0; point.second < ncols; point.second++)
    draw(point, '%');
  point.first = nrows - 1;
  for (point.second = 0; point.second < ncols; point.second++)
    draw(point, '%');
  point.second = 0;
  for (point.first = 1; point.first < nrows - 1; point.first++)
    draw(point, '%');
  point.second = ncols - 1;
  for (point.first = 1; point.first < nrows - 1; point.first++)
    draw(point, '%');


  
  r = nrows/2; c = ncols/2;
  queue<Point> snake;

  for (int i = 0; i < LEN; ++i) {
    point = make_pair(r, c++);
    snake.push(point);
    draw(snake.back());
  }

  for (int i = 0; i < 500; i++) {
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
