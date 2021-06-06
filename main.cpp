#include <windows.h>
#include <iostream>
#include <ctime>
#include <cstdlib>

#define N_GLIDER
// GLIDER will start will a screen full of gliders
#define FHD
// FHD will force the screen to 1080p, windows scaling fucks up the automatic detection
#define N_TEXTPRINT
// TEXTPRINT will print the grid in text instead of pixels
#define NODELAY
// NODELAY will disable the sleep between renders
#define N_BRUTAL
// BRUTAL will disable activity detection, running to the bitter end (and probably never getting there)

using namespace std;

constexpr auto INITIAL_LIFE_RATIO = 10;
constexpr auto ALIVE = true;
constexpr auto DEAD = false;

//Set colors
const COLORREF COLOR_ALIVE = RGB(255, 255, 255);
const COLORREF COLOR_DEAD = RGB(0, 0, 0);
const COLORREF COLOR_RED = RGB(255, 0, 0);


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

void updateScreen(bool** old_state, bool** current_state, int horizontal, int vertical) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);


    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (old_state[cellX][cellY] != current_state[cellX][cellY]) { // will not update uncahnged cells
                if (current_state[cellX][cellY] == ALIVE)
                {
                    SetPixel(mydc, cellX, cellY, COLOR_ALIVE);
                }
                else {
                    SetPixel(mydc, cellX, cellY, COLOR_DEAD);
                }
            }
        }
    }

    ReleaseDC(myconsole, mydc);
}

void updateScreen(bool** current_state, int horizontal, int vertical) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

#ifndef TEXTPRINT
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (current_state[cellX][cellY] == ALIVE)
            {
                SetPixel(mydc, cellX, cellY, COLOR_ALIVE);
            }
            else {
                SetPixel(mydc, cellX, cellY, COLOR_DEAD);
            }
        }
    }
#endif // !TEXTPRINT

#ifdef TEXTPRINT
    for (int cellx = 0; cellx < horizontal; cellx++)
    {
        for (int celly = 0; celly < vertical; celly++)
        {
            if (current_state[celly][cellx] == ALIVE)
            {
                if ((cellx == horizontal - 1 || cellx == 0) || (celly == vertical - 1 || celly == 0))
                {
                    cout << 'x';
                }
                else {
                    cout << 'x';
                }
            }
            else {
                cout << '-';
            }
        }
        cout << endl;
    }
    cout << endl;
#endif // TEXTPRINT

    ReleaseDC(myconsole, mydc);
}

bool cellStatus(bool** old_state, int cellX, int cellY, int horizontal, int vertical, bool looping = false) {
    int neighbors = 0;
    bool status;

    if (!looping)
    {
        // counting neighbors with bound limits
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
    }
    else {
        // counting neighbors, looping edges
        if ((cellX > 0 && old_state[cellX - 1][cellY] == ALIVE) || // left
            (cellX == 0 && old_state[horizontal - 1][cellY] == ALIVE))
            neighbors++;
        if ((cellY > 0 && old_state[cellX][cellY - 1] == ALIVE) || // up
            (cellY == 0 && old_state[cellX][vertical - 1] == ALIVE))
            neighbors++;
        if ((cellX < horizontal - 1 && old_state[cellX + 1][cellY] == ALIVE) ||  // right
            (cellX == horizontal - 1 && old_state[0][cellY] == ALIVE))
            neighbors++;
        if ((cellY < vertical - 1 && old_state[cellX][cellY + 1] == ALIVE) || // down
            (cellY == vertical - 1 && old_state[cellX][0] == ALIVE))
            neighbors++;
        if ((cellX > 0 && cellY > 0
            && old_state[cellX - 1][cellY - 1] == ALIVE) || // top left
            (cellX == 0 && cellY > 0 // x out of bounds
                && old_state[horizontal - 1][cellY - 1] == ALIVE) ||
            (cellX > 0 && cellY == 0 // y out of bounds
                && old_state[cellX - 1][vertical - 1] == ALIVE) ||
            (cellX == 0 && cellY == 0 // both out of bounds
                && old_state[horizontal - 1][vertical - 1] == ALIVE))
            neighbors++;
        if ((cellX < horizontal - 1 && cellY < vertical - 1 // bottom right
            && old_state[cellX + 1][cellY + 1] == ALIVE) ||
            (cellX == horizontal - 1 && cellY < vertical - 1 // x out of bounds
                && old_state[0][cellY + 1] == ALIVE) ||
            (cellX < horizontal - 1 && cellY == vertical - 1 // y out of bounds
                && old_state[cellX + 1][0] == ALIVE) ||
            (cellX == horizontal - 1 && cellY == vertical - 1 // both out of bounds
                && old_state[0][0] == ALIVE))
            neighbors++;
        if ((cellX < horizontal - 1 && cellY > 0 // top right
            && old_state[cellX + 1][cellY - 1] == 1) ||
            (cellX == horizontal - 1 && cellY > 0 // x out of bounds
                && old_state[0][cellY - 1] == ALIVE) ||
            (cellX < horizontal - 1 && cellY == 0 // y out of bounds
                && old_state[cellX + 1][vertical - 1] == ALIVE) ||
            (cellX == horizontal - 1 && cellY == 0 // both out of bounds
                && old_state[0][vertical - 1] == ALIVE))
            neighbors++;
        if ((cellX > 0 && cellY < vertical - 1 // bottom left
            && old_state[cellX - 1][cellY + 1] == 1) ||
            (cellX == 0 && cellY < vertical - 1 // x out of bounds
                && old_state[horizontal - 1][cellY + 1] == ALIVE) ||
            (cellX > 0 && cellY == vertical - 1 // y out of bounds 
                && old_state[cellX - 1][0] == ALIVE) ||
            (cellX == 0 && cellY == vertical - 1 // both out of bounds 
                && old_state[horizontal - 1][0] == ALIVE))
            neighbors++;
    }



    if (old_state[cellX][cellY] == ALIVE && neighbors <= 1) { // loneliness
        status = DEAD;
    }
    else if (old_state[cellX][cellY] == ALIVE && (neighbors == 2 || neighbors == 3)) { // survival
        status = ALIVE;
    }
    else if (old_state[cellX][cellY] == ALIVE && neighbors >= 4) { // overpopulation
        status = DEAD;
    }
    else if (old_state[cellX][cellY] == DEAD && neighbors == 3) { // multiplication
        status = ALIVE;
    }
    else { // just in case, assume DEAD
        status = DEAD;
    }

    return status;
}


void updateStateMatrix(bool** old_state, bool** current_state, int horizontal, int vertical, bool looping = false) {
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            current_state[cellX][cellY] = cellStatus(old_state, cellX, cellY, horizontal, vertical, looping);
        }
    }
}

// check if life has exausted itself in the system
bool extinction(bool** state, int horizontal, int vertical) {
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (state[cellX][cellY] == ALIVE)
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

// checkes if the activity level in the system has dropped too low
bool low_Activity(bool** old_state, bool** current_state, int horizontal, int vertical, float threshold = 1) {
    const long total_cells = vertical * horizontal;
    long cells_changed = 0;
    bool result = false;

    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (old_state[cellX][cellY] != current_state[cellX][cellY])
            {
                cells_changed++;
            }
        }
    }

    if ((float)cells_changed / total_cells * 100 < threshold)
    {
        result = true;
    }

    return result;
}

// creates a single block matrix allocation and splits it into arrays
template <typename T>
T** createMatrix(int horizontal, int  vertical) {
    T** matrix = new T * [horizontal];
    matrix[0] = new T[horizontal * vertical];
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

// checks all stop conditions for the current run
bool endOfCycle(bool** old_state, bool** current_state, int horizontal, int vertical) {
    bool result = false;

#ifndef BRUTAL
    if (low_Activity(old_state, current_state, horizontal, vertical)) {
        cout << "low activity" << endl;
        result = true;
    }
#endif // !BRUTAL

#ifdef BRUTAL
    if (extinction(current_state, horizontal, vertical)) {
        cout << "ded" << endl;
        result = true;
}
    else if (stasis(old_state, current_state, horizontal, vertical)) { // this is effectivly impossible
        cout << "stasis" << endl;
        result = true;
    }
#endif // BRUTAL

    return result;
}




int main()
{
    // sets makes the window fullscreen
    SetConsoleDisplayMode(GetStdHandle(STD_OUTPUT_HANDLE), CONSOLE_FULLSCREEN_MODE, 0);
    ShowScrollBar(GetConsoleWindow(), SB_VERT, 0);
    ShowConsoleCursor(false);
    Sleep(1500);


    // get screen rezsolution
    int horizontal = 0;
    int vertical = 0;
    GetDesktopResolution(horizontal, vertical);

#ifdef FHD
    horizontal = 1920;
    vertical = 1080;
#endif // FHD

    bool** old_state = createMatrix<bool>(horizontal, vertical);
    bool** current_state = createMatrix<bool>(horizontal, vertical);

#ifndef GLIDER
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
#endif // !GLIDER

#ifdef GLIDER
     //initialize an empty screen
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            current_state[cellX][cellY] = false;
        }
    }

    int offsetX = 0;
    int offsetY = 0;
    int sizeX = 40;
    int sizeY = 40;
    int state = 0;

    for (offsetX = 0; offsetX + sizeX <= horizontal; offsetX += sizeX)
    {
        for (offsetY = 0; offsetY + sizeY <= vertical; offsetY += sizeY)
        {

            switch (state)
            {
            case (0): {
                current_state[offsetX + 0][offsetY + 2] = true;
                current_state[offsetX + 1][offsetY + 0] = true;
                current_state[offsetX + 1][offsetY + 2] = true;
                current_state[offsetX + 2][offsetY + 2] = true;
                current_state[offsetX + 2][offsetY + 1] = true;
                break;
            }
            case (1): {
                current_state[offsetX + 0][offsetY + 0] = true;
                current_state[offsetX + 1][offsetY + 1] = true;
                current_state[offsetX + 1][offsetY + 2] = true;
                current_state[offsetX + 2][offsetY + 0] = true;
                current_state[offsetX + 2][offsetY + 1] = true;
                break;
            }
            case (2): {
                current_state[offsetX + 0][offsetY + 1] = true;
                current_state[offsetX + 1][offsetY + 2] = true;
                current_state[offsetX + 2][offsetY + 0] = true;
                current_state[offsetX + 2][offsetY + 1] = true;
                current_state[offsetX + 2][offsetY + 2] = true;
                break;
            }
            case (3): {
                current_state[offsetX + 0][offsetY + 0] = true;
                current_state[offsetX + 0][offsetY + 2] = true;
                current_state[offsetX + 1][offsetY + 1] = true;
                current_state[offsetX + 1][offsetY + 2] = true;
                current_state[offsetX + 2][offsetY + 1] = true;
                break;
            }
            default: {
                break;
            }
            }
            state = (state + 1) % 4;
        }
    }
#endif // GLIDER


    // initial render
    updateScreen(current_state, horizontal, vertical);
#ifndef NODELAY
    Sleep(1500);
#endif // NODELAY

    while (!endOfCycle(old_state, current_state, horizontal, vertical))
    {
        swap(old_state, current_state); // 'save' the old state without copying
        updateStateMatrix(old_state, current_state, horizontal, vertical);

        updateScreen(old_state, current_state, horizontal, vertical);
#ifndef NODELAY
        Sleep(1500);
#endif // NODELAY
    }

    deleteMatrix<bool>(old_state);
    deleteMatrix<bool>(current_state);
    return 0;
}
