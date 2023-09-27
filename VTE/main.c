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

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)    // 앞 3비트를 0으로 만들겠다. 앞 3비트를 지우면 패리티비트와 7번, 6번 비트가 사라지는데 이러면 대소문자를 구분하지 않는다.

/*** data ***/

// struct termios orig_termios;
/*
* 편집기의 정보를 담는 전역 변수 구조체를 생성
*/
struct editorConfig {
    struct termios orig_termios;

    int screenrows;
    int screencols;
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

char editorReadKey(){   
    /*
    * 키가 한번 눌리떄까지 기다렸다가 입력한 키를 반환하는 것 
    * 확장하여 이 함수를 확장하여 화살표 키의 경우 처럼 단일 키 누르기를 나타내는 여러 바이트를 읽는 이스케이프 시퀀스를 처리 
    */

    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) {
            die("read");
        }
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
    char *new = realloc(ab->b, ab->len + len)

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** input ***/

void editorProcessKeyPress(){
    /*
    * 키 누르기를 기다린 다음을 처리
    * 나중에 다양한 키 조합과 기타 특수 키를 편집기의 다양한 기능에 매핑하고 
    * 다른 입력이 가능한 키는 텍스트에 삽입한다.
    */

    char c = editorReadKey();

    switch (c){
    case CTRL_KEY('q'):
        // 종료 지점에 화면 지우기
        write(STDOUT_FILENO, "\x1b[2J", 4); 
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** output ***/

void editorDrawRows(){
    /*
    * 편집 중인 텍스트 버퍼에 각 행 그리기를 처리 합니다. 
    * 지금은 각 행에 물결을 그립니다.
    * 이는 행이 파일의 일부가 아니며 텍스트를 포함 할수 없음을 의미 합니다.
    * 아직 터미널의 크기를 모르기 때문에 24줄만 그립니다.
    * <esc>[H 이스케이프 시퀀스를 이용하여 커서을 왼쪽 위로 옯깁니다.
    */

    int y;
    for (y = 0; y < E.screenrows; y++){
        write(STDOUT_FILENO, "~", 1);

        if (y < E.screenrows - 1){
            write(STDOUT_FILENO, "\r\n", 2);
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

    write(STDOUT_FILENO, "\x1b[2J", 4); 

    /*
    * <esc>[2J는 화면 중간에 커서를 남긴다.
    * 이를 위쪽부터 그릴수 있도록 왼쪽위 모서리로 이동 시켜보자
    * 
    * 이 이스케이프 문자는 3바이트 길이이며 H명령 Cursor Position을 사용하여 커서 위치를 지정한다
    * https://vt100.net/docs/vt100-ug/chapter3.html#CUP
    * H명령의 커서를 이동시킬 행과 열의 번호를 두개의 인수로 사용한다. ex) <esc>[12;40H
    * 기본인수가 둘 다 1이다. (행과 열번호는 0이 아닌 1부터 시작한다.) 
    */
   write(STDOUT_FILENO, "\x1b[H", 3);

   editorDrawRows();

   write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/

void initEditor(){
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

