/*
* ctrl키를 누를고 알파벳을 누르면 1-26까지의 수가 들어온다.
* 
* tcsetattr를 발생 시키는 방법:
*         echo test | cmd /C C:\Users\user\Desktop\C\VTE\main
*         표준입력이 아닌 다른 방법으로 텍스트를 전달하면 오류를 발생시킨다.
*
* \x1b는 이스케이프 문자이건 10진수의 27을 의미하며 이스케이프 시퀀스 명령을 실행한다.
*/

/*** inceludes ***/

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

/*** defines ***/
#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)    // 앞 3비트를 0으로 만들겠다. 앞 3비트를 지우면 패리티비트와 7번, 6번 비트가 사라지는데 이러면 대소문자를 구분하지 않는다.

enum editorKey{
    // char type의 경우 저장 범위가 작아서 오버플로우가 될수 있으음으로 int 타입으로 변경한다
    // ARROW_LEFT = 'a',
    // ARROW_RIGHT = 'd',
    // ARROW_UP = 'w',
    // ARROW_DOWN = 's'

    // 열거형의 경우 처음이 1000이 되면 자동으로 다음에 오는 변수는 1씩 증가한다.
    ARROW_LEFT = 1000,  // 1000
    ARROW_RIGHT,        // 1001
    ARROW_UP,           // 1002
    ARROW_DOWN,          // 1003
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

/*** data ***/

// struct termios orig_termios;
/*
* 편집기의 정보를 담는 전역 변수 구조체를 생성
*/

typedef struct erow {
    int size;
    char *chars;
} erow;

struct editorConfig {
    int cx, cy; // 커서의 위치를 변수

    struct termios orig_termios;

    int screenrows;
    int screencols;

    int numrows;
    erow row;                       // editor row 동적으로 할당된 문자 데이터 및 길이에 대한 포인터로 텍스트 줄을 저장 합니다.
};

struct editorConfig E;




/*** terminal ***/

void die(const char *s){    // 각 라이브러리의 호출을 실패한 경우 다음을 호출하여 에러를 확인
    // 종료 지점에 화면 지우기
    write(STDOUT_FILENO, "\x1b[2J", 4); 
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);  // 대부분의 C라이브러리는 에러에 대비하여 전역변수인 errno를 설정한다. perror는 그 errno를 출력한다.
    exit(1);
}

void disableRawMode(){
    // tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios)
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){    // tcsetattr이 실패시
        die("tcsetattr");        
    }

}

void enableRawMode(){

    // struct termios raw; 전역변수 orig_termios를 만들어 대체
    
    // https://blog.naver.com/choi125496/130034222760 
    // c-lflag는 실제 사용자에게 보여지는 터미널의 속성 == 출력의 반향을 0n/off한다.


    //tcgetattr(STDIN_FILENO, &raw); // 터미널 속성을 읽어온다.
    // 전역변수 orig_termios을 조작
    // tcgetattr(STDIN_FILENO, &orig_termios);
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1){   // tcgetattr이 실패시
        die("tcgetattr");        
    }


    atexit(disableRawMode); // stdlib해더의함수로 메인함수가 종료할때 자동으로 실행하여 터미널을 원래대로 변경한다.

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(IXON       // ctrl-s나 ctrl-q의 경우 스프트웨어 흐름제어라는 다른 곳에서 관리해다 다른 플래그로 꺼야한다.
                                // c_iflag에서 IXON
                    | ICRNL     // ctrl-m의 경우 13번째알파벳인데 10을 출력한다. 10은 j인데?? -> 이는 개행문자가 출력되는거이다. 이것을 해재해보자 ICRNL
                    | BRKINT    // BRKINT가 켜져있으면 중단조건으로 인해 SIGINT를 누르는것과 같은신호가 나간다. ctrl-c처럼
                    | INPCK     // 패리티 검사를 활성화 한다.
                    | ISTRIP);  // 입력바이트의 8번째 비트를 0으로 설정합니다????
    raw.c_oflag &= ~(OPOST);    // 이 플래그는 개행이 되면 자동으로 "캐리지 리턴 \r"이라고 자동으로 커서를 그 행의 제일 처음으로 이동시켜주는데 이 를 꺼버림 
    raw.c_cflag |= (CS8);       // 플래그가 아니라 비트 마스크이기 떄문에 OR연산을 이용하여 설정 문자크기(CS)를 8로 조정
    raw.c_lflag &=              //? 비트 연산으로 termios 구조체의 ECHO 플래그를 해제합니다
                    ~(ECHO      // ECHO기능은 터미널에 입력하는 글자를 바로 터미널에 보여주는 역활을 한다.
                                // ECHO기능을 끔으로써 우리가 치는 건 나오지 터미널에 다시 나오기 않는다.
                                // 원시모드에서는 우릴를 방해 한다고?
                                // ECHO는 비트 플래그 이며 not을 통해 반전하여 echo만 꺼신상태로 만들고 and 연산을 통해 다른 풀래그는 유지하고 echo만 끈다.

                    | ICANON    //ICANON은 표준모드르 플래그로 이를 끄면 최종적으로 입력 내용을 한 줄 씩 읽지 않고 바이트 단위로 읽게 된다.

                    | ISIG      // ISIG ctrl-c나 ctrl-z의 경우 터미널에 대한 명령이 임으로 누르면 수가 안나온다. 그래서 ISIG 플래그를 꺼서 이를 방지한다.

                    | IEXTEN);  // ctrl-v도 붙혀넣기를 하면 복사한 글자가 그대로 들어간다. IEXTEN -> 안되는것 같음

    raw.c_cc[VMIN] = 0;     // 값을 반화하기전 입력의 최소 바이트 수를 설정. 0으로 설정하여 read가 읽자 마자 반환 하도록 설정

    raw.c_cc[VTIME] = 1;    // read가 입력을 대기 하는 시간을 설정 10분의 1초단위로  1로 설정하면 10분의 1초가 된다.
    
    //VSC에서는 VSC가 가져가 버린다.

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){ // tcsetattr이 실패시
        die("tcsetattr");        
    }
    // tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw); // 수정한 터미널 속성을 저장한다. 
    //TCSAFLUSH가 변경 사항을 적용할 시기를 결정한다. 
    //보류 중인 모든 출력이 터미널에 기록될 때까지 대기하며 읽지 않은 입력도 모두 폐기합니다.
    // 프로그램이 종료되면 읽지 않은 입력은 모두 삭제된다.
}

int editorReadKey(){   
// char editorReadKey(){       
    /*
    * 키가 한번 눌리떄까지 기다렸다가 입력한 키를 반환하는 것 
    * 확장하여 이 함수를 확장하여 화살표 키의 경우 처럼 단일 키 누르기를 나타내는 여러 바이트를 읽는 이스케이프 시퀀스를 처리 
    */

    int nread;
    char c;

    /*
    * 한 문자씩 읽어서 c에 저장한다.
    * read는 읽은 문자수를 반환하는 1이면 계속 진행하고
    * 아니면 에러를 확인하고 프로그램을 종료한다.
    */
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){    
        if (nread == -1 && errno != EAGAIN) {
            die("read");
        }
    }


    if(c=='\x1b'){                                  // 만약 C가 이스케이프 문자이면
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1)    // 이스케이프 문자이후 오는 한 문자를 읽음
        {
            return '\x1b';
        }

        if (read(STDIN_FILENO, &seq[1], 1) != 1)    // 이스케이프 이후에 오는 한 문자를 읽음
        {
            return '\x1b';
        }

        if(seq[0] == '['){                          // 이스케이프 문자중 명령 문자이면
            if (seq[1] >= '0' && seq[1] <= '9'){
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;                      //home키와 end키 추가
                        case '4': return END_KEY;                       // home키는 1,7,H,OH 네가지의 시퀀스를 가진다
                        case '7': return HOME_KEY;                      // end키도  4, 8, F, OF 네가자의 시퀀스를 가진다.
                        case '8': return END_KEY;

                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;

                        case '3': return DEL_KEY;
                    
                    default:
                        break;
                    }
                }
            } else{
                switch (seq[1]){                         // 뒤에 오는 인수에 따라 어떠화살표인지 매칭
                    // 위 쪽의 enum을 이용하여 변경
                    // case 'A': return 'w';
                    // case 'B': return 's';
                    // case 'C': return 'd';
                    // case 'D': return 'a';
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;

                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if(seq[0] == 'O'){                           //
            switch (seq[1]) {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                    break;
                default:
                    break;
            }
        } 
        return '\x1b';
    } else {
        return c;
    }
    
    return c;
}

int getCursorPosition(int *rows, int *cols){

    // 표준 입력으로 들어오는 마우스 포인트 위치를 읽기
    char buf[32];
    unsigned int i = 0;

    // n(Device Status Report)으로 6은 커서의 위치를 표준 입력을 반환
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    // printf("\r\n");
    
    // char c;

    // while (read(STDIN_FILENO, &c, 1) == 1)
    // {
    //     if (iscntrl(c))
    //     {
    //         printf("%d\r\n", c);
    //     } else
    //     {
    //         printf("%d ('%c')\r\n", c, c);
    //     }
                
    // }
    // ****************************************************************

    while (i < sizeof(buf) - 1)     
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }

        if (buf[i] == 'R') {
            break;
        }       
        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;    
}

int getWindowSize(int *rows, int *cols){

    /*
    * iocrl()은 TIOCWINSZ요청을 통해 대부분의 터미널의 창의 크기를 불러온다.
    * iocrl 함수는 winsz 구조체를 반환한다. 
    * 실패시 -1을 반환한다.
    * 성공적으로 반환하면 참조를 통해 값을 넘겨준다.
    */

    struct winsize ws;

    // ioctl이 작동하지 않을 경우 두번째 방법을 어떻게 작동하는지 확인하기 우하여 1을 테스트를 위해 추가
    // 커서가 오른쪽 아래로 이동하고 키를 입력하자 오류가 발생
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        // ioctl이 모든 시스탬에서 창의 크기를 요청 할 수 있다는 보장이 없기 때문에 다른 방법을 이용
        /*
        * C(Cursor Forward)는 커서를 오른쪽으로 이동하고 B(Cursor Down)명령은 커서를 아래로 이동합니다.
        * C와 B는 화면을 넘어가지 않도록 되어있습니다. 반면 H는 화면 밖으로 넘어가기때문에 사용하지 않습니다.
        * 
        */
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        
        return getCursorPosition(rows, cols);

    }else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
    
}

/*** file i/o***/

/*
* 파일에서 문자열을 읽어 오는 함수
*/
void editorOpen(){
    char * line = "Hello, world";
    ssize_t linelen = 13;

    E.row.size = linelen;
    E.row.chars = malloc(linelen + 1);
    memcpy(E.row.chars, line, linelen);
    E.row.chars[linelen] = '\0';
    E.numrows = 1;
}


/*** append buffer ***/
// 이 버퍼는 
struct abuf
{
    char *b;
    int len;
};

// 빈 버퍼를 나타내는 상수
#define ABUF_INT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len); // 할당된 메모리의 크기를 변화 연속된 메모리를 잡을 수 없을 경우 새로운 주소를 반환

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);  // 버퍼에 있는 문자를 매개변수로 들어온 s에 복사
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {  // 동적 메모리를 지우는 함수
    free(ab->b);
}

/*** input ***/

/*
* 키보드의 뱡향키를 누르면 커서의 위치가 바뀌도록 하는 함수
*/
void editorMoveCursor(int key){
// void editorMoveCursor(char key){
    switch (key)
    {
    // 위 쪽의 enum을 이용하여 변경
    // case 'a':
    //     E.cx--;
    //     break;
    // case 'd':
    //     E.cx++;
    //     break;
    // case 'w':
    //     E.cy--;
    //     break;
    // case 's':
    //     E.cy++;
    //     break;


    case ARROW_LEFT:
    if (E.cx != 0){     // 커서의 위치가 나가지 않도록 범위를 제한
        E.cx--;
    }
        break;
    case ARROW_RIGHT:
    if (E.cx != E.screencols - 1){
        E.cx++;
    }
        break;
    case ARROW_UP:
    if (E.cy != 0){
        E.cy--;
    }
        break;
    case ARROW_DOWN:
    if (E.cy != E.screenrows - 1){
        E.cy++;
    }
        break;
    }
}

void editorProcessKeyPress(){
    /*
    * 키 누르기를 기다린 다음을 처리
    * 나중에 다양한 키 조합과 기타 특수 키를 편집기의 다양한 기능에 매핑하고 
    * 다른 입력이 가능한 키는 텍스트에 삽입한다.
    */

   int c = editorReadKey();
    // char c = editorReadKey();

    switch (c){
    case CTRL_KEY('q'):
        // 종료 지점에 화면 지우기
        write(STDOUT_FILENO, "\x1b[2J", 4); 
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    // 위 쪽의 enum을 이용하여 변경
    // case 'w':
    // case 'a':
    // case 's':
    // case 'd':
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    
    // page를 올리거나 내리면 마우스 커서를 열심히 옮김
    case PAGE_UP:
    case PAGE_DOWN:
        {   // 변수를 선언하기 위해서
            int times = E.screenrows;
            while(times--){
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
        }
        break;
    
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screencols - 1;
        break;
    }
}

/*** output ***/

void editorDrawRows(struct abuf *ab){
    /*
    * 편집 중인 텍스트 버퍼에 각 행 그리기를 처리 합니다. 
    * 지금은 각 행에 물결을 그립니다.
    * 이는 행이 파일의 일부가 아니며 텍스트를 포함 할수 없음을 의미 합니다.
    * 아직 터미널의 크기를 모르기 때문에 24줄만 그립니다.
    * <esc>[H 이스케이프 시퀀스를 이용하여 커서을 왼쪽 위로 옯깁니다.
    */

    int y;
    for (y = 0; y < E.screenrows; y++){                         // 화면의 줄 수만큼 텍스트를 출럭
        if (y >= E.numrows) {
            if (y == E.screenrows / 3){
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > E.screenrows){
                    welcomelen = E.screencols;
                }
                int padding = (E.screencols - welcomelen) / 2;
                if (padding){
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                {
                    abAppend(ab, " ", 1);
                }
                
                abAppend(ab, welcome, welcomelen);
            }else{
                abAppend(ab, "~", 1);
            }
        } else{
            int len = E.row.size;
            if(len > E.screencols) len = E.screencols;
            abAppend(ab, E.row.chars, len);
        }
        
        



        // write(STDOUT_FILENO, "~", 1); 애래로 대체
        // abAppend(ab, "~", 1);    위로 이동

        abAppend(ab, "\x1b[K", 3); // 현재 줄을 지우는 명령어 J의 경우 2는 전체를 지우고 1은 커서 왼쪽에 있는 줄의 부분을 지우고 0은 커서의 오른쪽에 있는 줄의 부분을 지운다.
        // K의 기본 인수는 0이고 
        if (y < E.screenrows - 1){
            // write(STDOUT_FILENO, "\r\n", 2);
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen(){
    /*
    * 화면에 4바이트를 쓰고있는 것을 의미
    * \x1b는 이스케이프 문자이건 10진수의 27을 의미하며 이스케이프 시퀀스 명령을 실행한다.
    * 나머지 3바이트는 [2J
    * 
    * 우리는 이스케이프 시퀀스를 작성하고 있습니다.
    * 이스케이프 시퀀스는 항상 27로 시작하고 뒤에 [문자가 온다.
    * 이스케이프 시퀀스는 터미널에 텍스트 색상 지정, 커서이동, 화면 일부 지우기와 같은 텍스트 지정 작업을 수행하도록 지시 합니다.
    * 
    * 화면을 지우려면 J명령을 사용하고 있습니다.
    * 이스케이프 시퀀스 명령은 명령 앞에 오는 인수를 사용합니다.  2는 전체를 지우라는 것 입니다.
    * <esc>[1J는 커서가 있는 곳까지 화면을 지우고 
    * <esc>[0J는 커서부터 화며 끝까지 지웁니다 0은 기본 인수 이므로  <esc>[J와 동일한 명령이다.
    */

    // write(STDOUT_FILENO, "\x1b[2J", 4); 

    /*
    * <esc>[2J는 화면 중간에 커서를 남긴다.
    * 이를 위쪽부터 그릴수 있도록 왼쪽위 모서리로 이동 시켜보자
    * 
    * 이 이스케이프 문자는 3바이트 길이이며 H명령 Cursor Position을 사용하여 커서 위치를 지정한다
    * https://vt100.net/docs/vt100-ug/chapter3.html#CUP
    * H명령의 커서를 이동시킬 행과 열의 번호를 두개의 인수로 사용한다. ex) <esc>[12;40H
    * 기본인수가 둘 다 1이다. (행과 열번호는 0이 아닌 1부터 시작한다.) 
    */
    //    write(STDOUT_FILENO, "\x1b[H", 3);

    //    editorDrawRows();

    //    write(STDOUT_FILENO, "\x1b[H", 3);


    /*
    * 위 부분을 대체된 부분
    *  화면을 새로 고침할 때마다 비어있는 새로운 버퍼를 만듬
    * 버퍼에 있는 내용을 write함수를 이용하여 내보고
    * 버퍼를 지움
    */

    struct abuf ab = ABUF_INT;

    // h와 ㅣ은 다양한 터미널의 모드를 켜고 끄는 명령어 이다 ?25의 경우 마우스 커서를 끄고 켠다.
    abAppend(&ab, "\x1b[?25l", 6);  
    // abAppend(&ab, "\x1b[2J", 4); 모든 줄을 지우는 [2J 대신 한 줄만 변경  abAppend(ab, "\x1b[K", 3);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    // abAppend(&ab, "\x1b[H", 3);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);  // 화면을 새로고침 할 때 커서의 위치를 이동
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);


    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** init ***/

void initEditor(){
    //  커서위치를 초기화
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;

    /*
    * 화면의 크기를 정보를 받아오는 함수
    */
    if(getWindowSize(&E.screenrows, &E.screencols) == -1){
        die("getWindowSize");
    }
}

int main(){
    enableRawMode();
    initEditor();
    editorOpen();

    char c;
    // while (1){
    //     char c = '\0';
    //     if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");    // read 실패시 cygwin은 read의 시간초과시 0이 아닌 -1을 리턴해서 EAGAIN을 예외 처리
    //     if (iscntrl(c)){ // iscntrl은 들어오는 입력이 제어 문자인지 ㅇ확인한다. 아스키코드의 0~31은 개행과 제어문자이다
    //         printf("%d\r\n", c);//read() 한 문자싞 읽어서 c에 저장한다
    //     } else{
    //         printf("%d ('%c')\r\n", c, c);
    //     }
    //     if(c == CTRL_KEY('q')) {    // 
    //         break;
    //     }
    // } 

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeyPress();
    }
    

    return 0;
}
