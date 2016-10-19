#include "InputHandler.h"

int getch (void)
{
    return getchar();
}

enum KeyTable {
    KeyEnter = 10,
    KeyTab = 9,
    KeyBackSpace = 8
};

string InputHandler::Getline()
{
    history.push_back(string());
    auto line = history.rbegin();

    while(1)
    {
        int ch = getch();
		if(ch == EOF)
			return "";
        if(ch == KeyEnter) {
			putchar('\n');
            return *line;
		}
		if(ch == '\033') {
			getch(); 
			ch = getch();
			if(ch == 'A') // up
            {
                if(line+1 != history.rend()) {
                    putchar('\r');
                    int length = line->size();
                    line++;
                    printf("$ %-*s",length,line->c_str());
                    for( size_t i = 0 ; i < length - line->size() + 2 ; ++i ) {
                        putchar('\b');
                    }
                }
            }
			if(ch == 'B') // down
				putchar('B');
			if(ch == 'C') // right
				putchar('C');
			if(ch == 'D') // left
				putchar('D');
			continue;
		}
        if(ch != KeyTab && ch != KeyBackSpace) {
            //putchar(ch);
            *line += ch;
        }

        if(ch == KeyBackSpace) {
            printf("\b\033[K");
            if(line->size() > 0)
                line->pop_back();
        }
    }
}
