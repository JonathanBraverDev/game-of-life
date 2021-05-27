#include<windows.h>
#include<iostream>
#include <winuser.h>
#include <cmath>
#include "wtypes.h"
#include <ctime>
#include <cstdlib>

using namespace std;

constexpr auto INITIAL_LIFE_RATIO = 5;

constexpr auto alive = true;
constexpr auto dead = false;


// Get the horizontal and vertical screen sizes in pixel
void GetDesktopResolution(int& horizontal, int& vertical)
{
    RECT desktop;
    // Get a handle to the desktop window
    const HWND hDesktop = GetDesktopWindow();
    // Get the size of screen to the variable desktop
    GetWindowRect(hDesktop, &desktop);
    // The top left corner will have coordinates (0,0)
    // and the bottom right corner will have coordinates
    // (horizontal, vertical)
    horizontal = desktop.right;
    vertical = desktop.bottom;
}

void ShowConsoleCursor(bool showFlag)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_CURSOR_INFO     cursorInfo;

    GetConsoleCursorInfo(out, &cursorInfo);
    cursorInfo.bVisible = showFlag; // set the cursor visibility
    SetConsoleCursorInfo(out, &cursorInfo);
}

void updateScreen(bool** current_state, int horizontal, int vertical) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);


    //Choose any color
    COLORREF ALIVE = RGB(255, 255, 255);
    COLORREF DEAD = RGB(0, 0, 0);


    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (current_state[cellX][cellY] == alive)
            {
                SetPixel(mydc, cellX, cellY, ALIVE);
            }
            else {
                SetPixel(mydc, cellX, cellY, DEAD);
            }
        }
    }

    ReleaseDC(myconsole, mydc);
}

bool cellStatus(bool** old_state, int cellX, int cellY, int horizontal, int vertical) {
    int neighbors = 0;
    bool status;

    // counting neighbors wth bound limits
    if (cellX > 0 && old_state[cellX - 1][cellY] == 1)
        neighbors++;
    if (cellY > 0 && old_state[cellX][cellY - 1] == 1)
        neighbors++;
    if (cellX > 0 && cellY > 0
        && old_state[cellX - 1][cellY - 1] == 1)
        neighbors++;
    if (cellX < horizontal - 1 && old_state[cellX + 1][cellY] == 1)
        neighbors++;
    if (cellY < vertical - 1 && old_state[cellX][cellY + 1] == 1)
        neighbors++;
    if (cellX < horizontal - 1 && cellY < vertical - 1
        && old_state[cellX + 1][cellY + 1] == 1)
        neighbors++;
    if (cellX < horizontal - 1 && cellY > 0
        && old_state[cellX + 1][cellY - 1] == 1)
        neighbors++;
    if (cellX > 0 && cellY < vertical - 1
        && old_state[cellX - 1][cellY + 1] == 1)
        neighbors++;

    if (old_state[cellX][cellY] == alive && neighbors <= 1) { // lonliness
        status = false;
    }
    // IMPORTANT: survival is assumed as no change occurs, between 2-3 neighbors
    else if (old_state[cellX][cellY] == alive && neighbors >= 4) { // ovepopulation
        status = false;
    }
    else if (old_state[cellX][cellY] == dead && neighbors == 3 ) { // multiplication
        status = true;
    }
    else { // just in case, assume dead
        status = false;
    }

    return status;
}

void updateMatrixState(bool** old_state, bool** current_state, int horizontal, int vertical) {
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            current_state[cellX][cellY] = cellStatus(old_state, cellX, cellY, horizontal, vertical);
        }
    }
}

// check if life has exausted itself in the system
bool extinction(bool** state, int horizontal, int vertical) {
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (state[cellX][cellY] == alive)
            {
                return false;
            }
        }
    }

    return true;
}

// checkes if the system reached a stable, static state
bool stasis(bool** old_state, bool** current_state, int horizontal, int vertical) {
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (old_state[cellX][cellY] != current_state[cellX][cellY])
            {
                return false;
            }
        }
    }

    return true;
}


// creates a single block matrix allocation and splits it into arrays
template <typename T>
T** createMatrix(int horizontal, int  vertical) {
    T** matrix = new T* [horizontal];
    matrix[0] = new bool[horizontal * vertical];
    for (int cellX = 1; cellX < horizontal; cellX++) {
        matrix[cellX] = matrix[0] + cellX * vertical;
    }
    
    return matrix;
}

// deletes single block allocated matrixes 
template <typename T>
void deleteMatrix(T** matrix) {
    delete[] matrix[0];
    delete[] matrix;
}



int main()
{
    // sets makes the window fullscreen
    SetConsoleDisplayMode(GetStdHandle(STD_OUTPUT_HANDLE), CONSOLE_FULLSCREEN_MODE, 0);
    ShowScrollBar(GetConsoleWindow(), SB_VERT, 0);
    ShowConsoleCursor(false);
    Sleep(1000);

    // get screen rezsolution
    int horizontal = 0;
    int vertical = 0;
    GetDesktopResolution(horizontal, vertical);

    bool** old_state = createMatrix<bool>(horizontal, vertical);
    bool** current_state = createMatrix<bool>(horizontal, vertical);

    // randomize the first generation
    srand(time(0));
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (rand() % INITIAL_LIFE_RATIO == 0)
            {
                current_state[cellX][cellY] = true;
            }
            else
            {
                current_state[cellX][cellY] = false;
            }
        }
    }
    

    for (int tmp = 0; tmp < 10; tmp++)
    {
        if (extinction(current_state, horizontal, vertical)) {
            cout << "ded" << endl;
            break;
        }
        else if (stasis(old_state, current_state, horizontal, vertical)) {
            cout << "stasis" << endl;
            break;
        }

        updateScreen(current_state, horizontal, vertical);
        Sleep(1500);

        swap(old_state, current_state); // 'save' the old state without copying
        updateMatrixState(old_state, current_state, horizontal, vertical);
    }

    deleteMatrix<bool>(old_state);
    deleteMatrix<bool>(current_state);
    return 0;
}
