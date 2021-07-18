#include <windows.h>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <thread>
#include <vector>

#define RANDOM
// RANDOM will start with a randomly genereted first generation
// GLIDER will start will a screen full of gliders
#define FHD
// FHD will force the screen to 1080p, windows scaling fucks up the automatic detection
// NATIVE will allow adapting to the resolution of the display
#define PATIENT
// PATIENT will wait before rendering each genereation
// NODELAY will disable the sleep between renders
#define BRUTAL
// EFFICENT will run until the activity drops low enogh to be boring
// BRUTAL will disable activity detection, running to the bitter end (and probably never getting there)
#define BITMAP
// LINEAR will render the scren as a big chunk (old and slow)
// MULTIRENDER will split rendering into a few columns (old and slow)
// TEXTPRINT will print the grid in text instead of pixels at significantly reduced rez and speed
// BITMAP will try to fill a bitmap and spit in into the screen instead of setpixeling
#define UNCERTAIN
// DOOMED will end the simulation after one iteration ends
// UNCERTAIN will give the user a choise
// RESURGENT will endlessly reset the simulation without confirmation
#define TRAPPED
// LOOPING will cause the screen to loop like in the game asteroids (simplest example)
// TRAPPED will enforce the screen borders, eliminating anything that dares to leave

#define NORMAL
// NORMAL will set the survival parameters to their respective defaults
// LUSH will increase max population, multiplication and survival rates
// PARADISE will unlock the secrets to ethernal life
// CRUEL will see like swiftly eradicated

using namespace std;
#ifdef RANDOM
constexpr auto INITIAL_LIFE_RATIO = 10; // this is an INVERSE (1 is EVERY pixel)
#endif // RANDOM
const bool ALIVE = true;
const bool DEAD = false;

#ifdef NORMAL
const int LONELINESS = 1;
const int MIN_SURVIVAL = 2;
const int MAX_SURVIVAL = 3;
const int MIN_MULTIPLY = 3;
const int MAX_MULTIPLY = 3;
const int OVERPOPULATION = 4;
#endif // NORMAL

#ifdef LUSH
const int LONELINESS = 1;
const int MIN_SURVIVAL = 2;
const int MAX_SURVIVAL = 4;
const int MIN_MULTIPLY = 2;
const int MAX_MULTIPLY = 3;
const int OVERPOPULATION = 5;
#endif // LUSH

#ifdef PARADISE
const int LONELINESS = 0;
const int MIN_SURVIVAL = 1;
const int MAX_SURVIVAL = 4;
const int MIN_MULTIPLY = 2;
const int MAX_MULTIPLY = 4;
const int OVERPOPULATION = 6;
#endif // PARADISE

#ifdef CRUEL
const int LONELINESS = 1;
const int MIN_SURVIVAL = 2;
const int MAX_SURVIVAL = 2;
const int MIN_MULTIPLY = 3;
const int MAX_MULTIPLY = 3;
const int OVERPOPULATION = 3;
#endif // CRUEL


// Set colors
const COLORREF COLOR_ALIVE = RGB(255, 255, 255);
const COLORREF COLOR_DEAD = RGB(0, 0, 0);
const COLORREF COLOR_BORDER = RGB(0, 255, 0);

// Define structures
struct snapshot
{
    int maxX;
    int maxY;
    bool** local_state;
};

struct entry
{
    int max_maxX;
    int max_maxY;
    snapshot* snapshots;
};

struct point
{
    int cellX;
    int cellY;
};

struct sector
{
    int startX;
    int endX;
    int startY;
    int endY;
};


// creates a secors with the given limits
sector CreateSector(int startX, int endX, int startY, int endY) {
    sector new_sector{};
    new_sector.startX = startX;
    new_sector.endX = endX;
    new_sector.startY = startY;
    new_sector.endY = endY;

    return new_sector;
}


// creates a single block matrix allocation and splits it into arrays
template <typename T>
T** CreateMatrix(int maxX, int maxY) {
    T** matrix = new T * [maxX];
    matrix[0] = new T[maxX * maxY];
    for (int cellX = 1; cellX < maxX; cellX++) {
        matrix[cellX] = matrix[0] + cellX * maxY;
    }

    return matrix;
}

// deletes single block allocated matrixes 
template <typename T>
void DeleteMatrix(T** matrix) {
    delete[] matrix[0];
    delete[] matrix;
}

// copies an existing matrix, given size is of the original
template <typename T>
T** CopyMatrix(T** original, int maxX, int maxY) {
    T** new_matrix = CreateMatrix<bool>(maxX, maxY);

    for (int cellX = 0; cellX < maxX; cellX++) {
        copy(original[cellX], original[cellX] + maxY, new_matrix[cellX]);
    }

    return new_matrix;
}


// returns a smaller matrix of an area in the state
bool** CreateLocalState(bool** state, int startX, int endX, int startY, int endY) {
    bool** local_state = CreateMatrix<bool>(endX - startX, endY - startX);

    for (int cellX = startX; cellX < endX; cellX++) {
        for (int cellY = startY; cellY < endY; cellY++) {
            local_state[cellX - startX][cellY - startY] = state[cellX][cellY];
        }
    }

    return local_state;
}

// returns a smaller matrix of an area in the state
bool** CreateLocalState(bool** state, sector sector) {
    bool** local_state = CreateMatrix<bool>(sector.endX - sector.startX, sector.endY - sector.startX);

    for (int cellX = sector.startX; cellX < sector.endX; cellX++) {
        for (int cellY = sector.startY; cellY < sector.endY; cellY++) {
            local_state[cellX - sector.startX][cellY - sector.startY] = state[cellX][cellY];
        }
    }

    return local_state;
}


// returns a snapshot of a cestion of the state within the given coprdinates
snapshot CreateSnapshot(bool** state, int startX, int endX, int startY, int endY) {
    snapshot new_snapshot{};
    new_snapshot.local_state = CreateLocalState(state, startX, endX, startY, endY);
    new_snapshot.maxX = endX - startX;
    new_snapshot.maxY = endY - startY;

    return new_snapshot;
}

// returns a snapshot of a cestion of the state within the given coprdinates
snapshot CreateSnapshot(bool** state, sector sector) {
    snapshot new_snapshot{};
    new_snapshot.local_state = CreateLocalState(state, sector.startX, sector.endX, sector.startY, sector.endY);
    new_snapshot.maxX = sector.endX - sector.startX;
    new_snapshot.maxY = sector.endY - sector.startY;

    return new_snapshot;
}

// fills the given vector with all nearby living cells
void FindLivingNeighbors(bool** state_copy, int maxX, int maxY, int cellX, int cellY, point* neighbors, bool* filled) {
    int i = 0;
    point tmp;

    // counting neighbors, looping edges
    filled[i] = false;
    if (cellX > 0 && state_copy[cellX - 1][cellY] == ALIVE) { // left
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == 0 && state_copy[maxX - 1][cellY] == ALIVE) { // looping left
        tmp.cellX = maxX - 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellY > 0 && state_copy[cellX][cellY - 1] == ALIVE) { // up
        tmp.cellX = cellX;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellY == 0 && state_copy[cellX][maxY - 1] == ALIVE) { // looping up
        tmp.cellX = cellX;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX < maxX - 1 && state_copy[cellX + 1][cellY] == ALIVE) { // right
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && state_copy[0][cellY] == ALIVE) { // looping right
        tmp.cellX = 0;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellY < maxY - 1 && state_copy[cellX][cellY + 1] == ALIVE) { // down
        tmp.cellX = cellX;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellY == maxY - 1 && state_copy[cellX][0] == ALIVE) { // looping down
        tmp.cellX = cellX;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX > 0 && cellY > 0 // top left
        && state_copy[cellX - 1][cellY - 1] == ALIVE) {
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == 0 && cellY > 0 // x out of bounds
        && state_copy[maxX - 1][cellY - 1] == ALIVE) {
        tmp.cellX = maxX - 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX > 0 && cellY == 0 // y out of bounds
        && state_copy[cellX - 1][maxY - 1] == ALIVE) {
        tmp.cellX = cellX - 1;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == 0 && cellY == 0 // both out of bounds
        && state_copy[maxX - 1][maxY - 1] == ALIVE) {
        tmp.cellX = maxX - 1;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX < maxX - 1 && cellY < maxY - 1 // bottom right
        && state_copy[cellX + 1][cellY + 1] == ALIVE) {
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && cellY < maxY - 1 // x out of bounds
        && state_copy[0][cellY + 1] == ALIVE) {
        tmp.cellX = 0;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX < maxX - 1 && cellY == maxY - 1 // y out of bounds
        && state_copy[cellX + 1][0] == ALIVE) {
        tmp.cellX = cellX + 1;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == maxX - 1 && cellY == maxY - 1 // both out of bounds
        && state_copy[0][0] == ALIVE) {
        tmp.cellX = 0;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX < maxX - 1 && cellY > 0 // top right
        && state_copy[cellX + 1][cellY - 1] == ALIVE) {
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && cellY > 0 // x out of bounds
        && state_copy[0][cellY - 1] == ALIVE) {
        tmp.cellX = 0;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX < maxX - 1 && cellY == 0 // y out of bounds
        && state_copy[cellX + 1][maxY - 1] == ALIVE) {
        tmp.cellX = cellX + 1;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == maxX - 1 && cellY == 0 // both out of bounds
        && state_copy[0][maxY - 1] == ALIVE) {
        tmp.cellX = 0;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX > 0 && cellY < maxY - 1 // bottom left
        && state_copy[cellX - 1][cellY + 1] == ALIVE) {
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == 0 && cellY < maxY - 1 // x out of bounds
        && state_copy[maxX - 1][cellY + 1] == ALIVE) {
        tmp.cellX = maxX - 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX > 0 && cellY == maxY - 1 // y out of bounds 
        && state_copy[cellX - 1][0] == ALIVE) {
        tmp.cellX = cellX - 1;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == 0 && cellY == maxY - 1 // both out of bounds 
        && state_copy[maxX - 1][0] == ALIVE) {
        tmp.cellX = maxX - 1;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
}

// fills the given array with all nearby living cells (mem leak)
void FindAllNeighbors(bool** state_copy, int maxX, int maxY, int cellX, int cellY, point* neighbors, bool* filled) {
    int i = 0;
    point tmp;

    // counting neighbors, looping edges
    filled[i] = false;
    if (cellX > 0) { // left
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == 0) { // looping left
        tmp.cellX = maxX - 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellY > 0) { // up
        tmp.cellX = cellX;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellY == 0) { // looping up
        tmp.cellX = cellX;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX < maxX - 1) { // right
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && state_copy[0][cellY] == ALIVE) { // looping right
        tmp.cellX = 0;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellY < maxY - 1) { // down
        tmp.cellX = cellX;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellY == maxY - 1) { // looping down
        tmp.cellX = cellX;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX > 0 && cellY > 0) { // top left
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == 0 && cellY > 0) { // x out of bounds
        tmp.cellX = maxX - 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX > 0 && cellY == 0) { // y out of bounds
        tmp.cellX = cellX - 1;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == 0 && cellY == 0) { // both out of bounds
        tmp.cellX = maxX - 1;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX < maxX - 1 && cellY < maxY - 1) { // bottom right
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && cellY < maxY - 1) { // x out of bounds
        tmp.cellX = 0;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX < maxX - 1 && cellY == maxY - 1) { // y out of bounds
        tmp.cellX = cellX + 1;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == maxX - 1 && cellY == maxY - 1) { // both out of bounds
        tmp.cellX = 0;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX < maxX - 1 && cellY > 0) { // top right
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && cellY > 0) { // x out of bounds
        tmp.cellX = 0;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX < maxX - 1 && cellY == 0) { // y out of bounds
        tmp.cellX = cellX + 1;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == maxX - 1 && cellY == 0) { // both out of bounds
        tmp.cellX = 0;
        tmp.cellY = maxY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
    i++;

    filled[i] = false;
    if (cellX > 0 && cellY < maxY - 1) { // bottom left
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#ifdef LOOPING
    else if (cellX == 0 && cellY < maxY - 1) { // x out of bounds
        tmp.cellX = maxX - 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX > 0 && cellY == maxY - 1) { // y out of bounds
        tmp.cellX = cellX - 1;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    else if (cellX == 0 && cellY == maxY - 1) { // both out of bounds
        tmp.cellX = maxX - 1;
        tmp.cellY = 0;
        neighbors[i] = tmp;
        filled[i] = true;
    }
#endif
}

// finds a cluster of points connected to the given coordinates and saves them to the given vector
void FindCluster(bool** state_copy, int maxX, int maxY, int cellX, int cellY, vector<point>& current_cluster) {

    //if (state_copy[cellX][cellY] == DEAD) { // prevents 'double tagging' a cell based on the order they recurse at
    //    return;
    //}

    point current_point = point{ cellX, cellY };
    current_cluster.push_back(current_point);
    state_copy[cellX][cellY] = DEAD; // marking saved cells as dead

    point* neighbors = new point[8];
    bool* filled = new bool[8];

    FindLivingNeighbors(state_copy, maxX, maxY, cellX, cellY, neighbors, filled);

    for (int i = 0; i < 8; i++) {
        if (filled[i] && state_copy[neighbors[i].cellX][neighbors[i].cellY] == ALIVE) { // avoid running on checked cells
            FindCluster(state_copy, maxX, maxY, neighbors[i].cellX, neighbors[i].cellY, current_cluster);
        }
    }
}


// finds all clusters in the given state and saves them to the given vector
void FindClusters(bool** state, int maxX, int maxY, vector<vector<point>>& clusters) {

    bool** state_copy = CopyMatrix(state, maxX, maxY);

    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (state_copy[cellX][cellY] == ALIVE) {
                clusters.push_back(vector<point>{});
                FindCluster(state_copy, maxX, maxY, cellX, cellY, clusters.back());
                if (clusters.back().size() <= MIN_SURVIVAL) { // clusters of less than the constant CANNOT survive, no matter the arrangment
                    clusters.pop_back();
                }
            }
        }
    }

    DeleteMatrix<bool>(state_copy);
}

void FindClusterOutline(vector<point>& cluster, sector& outline) {
    point current_point;

    outline.startX = INT_MAX;
    outline.endX = INT_MIN;
    outline.startY = INT_MAX;
    outline.endY = INT_MIN;

    while (!cluster.empty()) {
        current_point = cluster.back();

        if (outline.startX > current_point.cellX) {
            outline.startX = current_point.cellX;
        }

        if (outline.endX < current_point.cellX) {
            outline.endX = current_point.cellX;
        }

        if (outline.startY > current_point.cellY) {
            outline.startY = current_point.cellY;
        }

        if (outline.endY < current_point.cellY) {
            outline.endY = current_point.cellY;
        }

        cluster.pop_back();
    }
}

void FindClusterOutlines(vector<vector<point>>& clusters, vector<sector>& outlines) {
    while (!clusters.empty()) {
        outlines.push_back(sector{});
        FindClusterOutline(clusters.back(), outlines.back());

        clusters.pop_back();
    }
}


// Get the maxX and maxY screen sizes in pixel
void GetDesktopResolution(int& maxX, int& maxY)
{
    RECT desktop;
    // Get a handle to the desktop window
    const HWND hDesktop = GetDesktopWindow();
    // Get the size of screen to the variable desktop
    GetWindowRect(hDesktop, &desktop);
    // The top left corner will have coordinates (0,0)
    // and the bottom right corner will have coordinates
    // (maxX, maxY)
    maxX = desktop.right;
    maxY = desktop.bottom;
}

void ShowConsoleCursor(bool showFlag)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_CURSOR_INFO     cursorInfo;

    GetConsoleCursorInfo(out, &cursorInfo);
    cursorInfo.bVisible = showFlag; // set the cursor visibility
    SetConsoleCursorInfo(out, &cursorInfo);
}

#ifdef MULTIRENDER
void RenderThread(bool** current_state, int maxY, int begin, int end, bool** old_state, bool initial) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    for (int cellX = begin; cellX < end; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (initial || old_state[cellX][cellY] != current_state[cellX][cellY]) { // will not update uncahnged cells
                if (current_state[cellX][cellY] == ALIVE) {
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
#endif // MULTIRENDER


void UpdateScreen(bool** current_state, int maxX, int maxY, bool** old_state = NULL, bool initial = true) {
#ifdef MULTIRENDER
    unsigned int num_of_threads = thread::hardware_concurrency();
    thread* threads = new thread[num_of_threads];
    const unsigned int render_zone_size = maxX / num_of_threads;
    unsigned int first_offset = maxX - render_zone_size * num_of_threads;
    int begin = 0;
    int end = first_offset + render_zone_size; // the first zone takes the rounding error

    threads[0] = thread(RenderThread, current_state, maxY, begin, end, old_state, initial);

    for (int created = 1; created < num_of_threads; created++) { // !IMPORTANT! set to 1 to skip the first thread creation
        begin = end;
        end += render_zone_size;
        threads[created] = thread(RenderThread, current_state, maxY, begin, end, old_state, initial);
    }

    for (int joined = 0; joined < num_of_threads; joined++) { // !IMPORTANT! set to 1 to skip the first thread creation
        threads[joined].join();
    }
#endif // MULTIRENDER

#ifdef LINEAR
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (initial || old_state[cellX][cellY] != current_state[cellX][cellY]) { // will not update uncahnged cells
                if (current_state[cellX][cellY] == ALIVE) {
                    SetPixel(mydc, cellX, cellY, COLOR_ALIVE);
                }
                else {
                    SetPixel(mydc, cellX, cellY, COLOR_DEAD);
                }
            }
        }
    }

    ReleaseDC(myconsole, mydc);
#endif // LINEAR

#ifdef TEXTPRINT
    for (int celly = 0; celly < maxY; celly++) { // y dominaant for newlines
        for (int cellx = 0; cellx < maxX; cellx++) {
            if (current_state[cellx][celly] == ALIVE) {
                cout << 'x';
            }
            else {
                cout << '-';
            }
        }
        cout << endl;
    }
    cout << endl;
#endif // TEXTPRINT

#ifdef BITMAP
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    COLORREF* colors = (COLORREF*)calloc(maxX * maxY, sizeof(COLORREF));
    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (current_state[cellX][cellY] == ALIVE) {
                colors[cellY * maxX + cellX] = COLOR_ALIVE;
            }
            else {
                colors[cellY * maxX + cellX] = COLOR_DEAD;
            }
        }
    }

    HBITMAP bitmap = CreateBitmap(maxX, maxY, 1, 8 * 4, (void*)colors);

    HDC scr = CreateCompatibleDC(mydc);

    SelectObject(scr, bitmap);

    BitBlt(mydc, 0, 0, maxX, maxY, scr, 0, 0, SRCCOPY);

    DeleteObject(bitmap);
    DeleteObject(scr);
    free(colors);
#endif // BITMAP
}

void DrawSectorOutline(sector sector, COLORREF* colors = nullptr, int maxX = 0, int maxY = 0) {
#if defined(LINEAR) || defined (MULTIRENDER)
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    int cellX;
    int cellY = sector.startY - 1;

    for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // top line
        SetPixel(mydc, cellX, cellY, COLOR_BORDER);
    }

    cellY = sector.endY + 1;
    for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // bottom line
        SetPixel(mydc, cellX, cellY, COLOR_BORDER);
    }

    cellX = sector.startX - 1;
    for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // left line]
        SetPixel(mydc, cellX, cellY, COLOR_BORDER);
    }

    cellX = sector.endX + 1;
    for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // right line
        SetPixel(mydc, cellX, cellY, COLOR_BORDER);
    }

    ReleaseDC(myconsole, mydc);
#endif

#ifdef BITMAP
    int cellX;
    int cellY = sector.startY - 1;

    if (cellY >= 0) {
        for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // top line
            if (cellX >= 0 && cellX < maxX) {
                colors[cellY * maxX + cellX] = COLOR_BORDER;
            }
        }
    }

    cellY = sector.endY + 1;
    if (cellY < maxY) {
        for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // bottom line
            if (cellX >= 0 && cellX < maxX) {
                colors[cellY * maxX + cellX] = COLOR_BORDER;
            }
        }
    }

    cellX = sector.startX - 1;
    if (cellX >= 0) {
        for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // left line
            if (cellY >= 0 && cellY < maxY) {
                colors[cellY * maxX + cellX] = COLOR_BORDER;
            }
        }
    }

    cellX = sector.endX + 1;
    if (cellX < maxX) {
        for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // right line
            if (cellY >= 0 && cellY < maxY) {
                colors[cellY * maxX + cellX] = COLOR_BORDER;
            }
        }
    }
#endif // BITMAP

}

void DrawSectorOutlines(vector<sector> sectors) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    while (!sectors.empty()) {
        DrawSectorOutline(sectors.back());
        sectors.pop_back();
    }

    ReleaseDC(myconsole, mydc);
}

void ClearSectorOutline(sector& sector) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    int cellX;
    int cellY = sector.startY - 1;

    for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // top line
        SetPixel(mydc, cellX, cellY, COLOR_DEAD);
    }

    cellY = sector.endY + 1;
    for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // bottom line
        SetPixel(mydc, cellX, cellY, COLOR_DEAD);
    }

    cellX = sector.startX - 1;
    for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // left line
        SetPixel(mydc, cellX, cellY, COLOR_DEAD);
    }

    cellX = sector.endX + 1;
    for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // right line
        SetPixel(mydc, cellX, cellY, COLOR_DEAD);
    }

    ReleaseDC(myconsole, mydc);
}

void ClearSectorOutlines(vector<sector>& sectors) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    while (!sectors.empty()) {
        ClearSectorOutline(sectors.back());
        sectors.pop_back();
    }

    ReleaseDC(myconsole, mydc);
}

bool CellStatus(bool** old_state, int cellX, int cellY, int maxX, int maxY) {
    int neighbors = 0;
    bool status = DEAD;

#ifdef LOOPING
    // counting neighbors, looping edges
    if ((cellX > 0 && old_state[cellX - 1][cellY] == ALIVE) || // left
        (cellX == 0 && old_state[maxX - 1][cellY] == ALIVE))
        neighbors++;
    if ((cellY > 0 && old_state[cellX][cellY - 1] == ALIVE) || // up
        (cellY == 0 && old_state[cellX][maxY - 1] == ALIVE))
        neighbors++;
    if ((cellX < maxX - 1 && old_state[cellX + 1][cellY] == ALIVE) ||  // right
        (cellX == maxX - 1 && old_state[0][cellY] == ALIVE))
        neighbors++;
    if ((cellY < maxY - 1 && old_state[cellX][cellY + 1] == ALIVE) || // down
        (cellY == maxY - 1 && old_state[cellX][0] == ALIVE))
        neighbors++;
    if ((cellX > 0 && cellY > 0
        && old_state[cellX - 1][cellY - 1] == ALIVE) || // top left
        (cellX == 0 && cellY > 0 // x out of bounds
            && old_state[maxX - 1][cellY - 1] == ALIVE) ||
        (cellX > 0 && cellY == 0 // y out of bounds
            && old_state[cellX - 1][maxY - 1] == ALIVE) ||
        (cellX == 0 && cellY == 0 // both out of bounds
            && old_state[maxX - 1][maxY - 1] == ALIVE))
        neighbors++;
    if ((cellX < maxX - 1 && cellY < maxY - 1 // bottom right
        && old_state[cellX + 1][cellY + 1] == ALIVE) ||
        (cellX == maxX - 1 && cellY < maxY - 1 // x out of bounds
            && old_state[0][cellY + 1] == ALIVE) ||
        (cellX < maxX - 1 && cellY == maxY - 1 // y out of bounds
            && old_state[cellX + 1][0] == ALIVE) ||
        (cellX == maxX - 1 && cellY == maxY - 1 // both out of bounds
            && old_state[0][0] == ALIVE))
        neighbors++;
    if ((cellX < maxX - 1 && cellY > 0 // top right
        && old_state[cellX + 1][cellY - 1] == 1) ||
        (cellX == maxX - 1 && cellY > 0 // x out of bounds
            && old_state[0][cellY - 1] == ALIVE) ||
        (cellX < maxX - 1 && cellY == 0 // y out of bounds
            && old_state[cellX + 1][maxY - 1] == ALIVE) ||
        (cellX == maxX - 1 && cellY == 0 // both out of bounds
            && old_state[0][maxY - 1] == ALIVE))
        neighbors++;
    if ((cellX > 0 && cellY < maxY - 1 // bottom left
        && old_state[cellX - 1][cellY + 1] == 1) ||
        (cellX == 0 && cellY < maxY - 1 // x out of bounds
            && old_state[maxX - 1][cellY + 1] == ALIVE) ||
        (cellX > 0 && cellY == maxY - 1 // y out of bounds 
            && old_state[cellX - 1][0] == ALIVE) ||
        (cellX == 0 && cellY == maxY - 1 // both out of bounds 
            && old_state[maxX - 1][0] == ALIVE))
        neighbors++;
#endif // LOOPING

#ifdef TRAPPED
    // counting neighbors with bound limits
    if (cellX > 0 && old_state[cellX - 1][cellY] == 1)
        neighbors++;
    if (cellY > 0 && old_state[cellX][cellY - 1] == 1)
        neighbors++;
    if (cellX > 0 && cellY > 0
        && old_state[cellX - 1][cellY - 1] == 1)
        neighbors++;
    if (cellX < maxX - 1 && old_state[cellX + 1][cellY] == 1)
        neighbors++;
    if (cellY < maxY - 1 && old_state[cellX][cellY + 1] == 1)
        neighbors++;
    if (cellX < maxX - 1 && cellY < maxY - 1
        && old_state[cellX + 1][cellY + 1] == 1)
        neighbors++;
    if (cellX < maxX - 1 && cellY > 0
        && old_state[cellX + 1][cellY - 1] == 1)
        neighbors++;
    if (cellX > 0 && cellY < maxY - 1
        && old_state[cellX - 1][cellY + 1] == 1)
        neighbors++;
#endif // TRAPPED


    if (old_state[cellX][cellY] == ALIVE && neighbors <= LONELINESS) { // loneliness
        status = DEAD;
    }
    else if (old_state[cellX][cellY] == ALIVE && (neighbors >= MIN_SURVIVAL && neighbors <= MAX_SURVIVAL)) { // survival
        status = ALIVE;
    }
    else if (old_state[cellX][cellY] == DEAD && (neighbors >= MIN_MULTIPLY && neighbors <= MAX_MULTIPLY)) { // multiplication
        status = ALIVE;
    }
    else if (old_state[cellX][cellY] == ALIVE && neighbors >= OVERPOPULATION) { // overpopulation
        status = DEAD;
    }
    else { // just in case, assume DEAD
        status = DEAD;
    }

    return status;
}

//void UpdateCell(bool** old_state, bool** current_state, int maxX, int maxY, int cellX, int cellY) {
//    current_state[cellX][cellY] = CellStatus(old_state, cellX, cellY, maxX, maxY);
//}

void UpdateNeighbors(bool** old_state, bool** current_state, bool** old_copy, int maxX, int maxY, int cellX, int cellY) {
    old_copy[cellX][cellY] = DEAD; // marking itself as dead
    point* neighbors = new point[8];
    bool* filled = new bool[8];
    FindAllNeighbors(old_copy, maxX, maxY, cellX, cellY, neighbors, filled);

    // counting neighbors, looping edges
    if (cellX > 0) { // left 
        old_copy[cellX - 1][cellY] = DEAD;
        current_state[cellX - 1][cellY] = CellStatus(old_state, cellX - 1, cellY, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellX == 0) { // looping left
        old_copy[maxX - 1][cellY] = DEAD;
        current_state[maxX - 1][cellY] = CellStatus(old_state, maxX - 1, cellY, maxX, maxY);
    }
#endif

    if (cellY > 0) { // up
        old_copy[cellX][cellY - 1] = DEAD;
        current_state[cellX][cellY - 1] = CellStatus(old_state, cellX, cellY - 1, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellY == 0) { // looping up
        old_copy[cellX][maxY - 1] = DEAD;
        current_state[cellX][maxY - 1] = CellStatus(old_state, cellX, maxY - 1, maxX, maxY);
    }
#endif

    if (cellX < maxX - 1) { // right
        old_copy[cellX + 1][cellY] = DEAD;
        current_state[cellX + 1][cellY] = CellStatus(old_state, cellX + 1, cellY, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellX == maxX - 1) { // looping right
        old_copy[0][cellY] = DEAD;
        current_state[0][cellY] = CellStatus(old_state, 0, cellY, maxX, maxY);
    }
#endif

    if (cellY < maxY - 1) { // down
        old_copy[cellX][cellY + 1] = DEAD;
        current_state[cellX][cellY + 1] = CellStatus(old_state, cellX, cellY + 1, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellY == maxY - 1) { // looping down
        old_copy[cellX][0] = DEAD;
        current_state[cellX][0] = CellStatus(old_state, cellX, 0, maxX, maxY);
    }
#endif

    if (cellX > 0 && cellY > 0) { // top left
        old_copy[cellX - 1][cellY - 1] = DEAD;
        current_state[cellX - 1][cellY - 1] = CellStatus(old_state, cellX - 1, cellY - 1, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellX == 0 && cellY > 0) { // x out of bounds
        old_copy[maxX - 1][cellY - 1] = DEAD;
        current_state[maxX - 1][cellY - 1] = CellStatus(old_state, maxX - 1, cellY - 1, maxX, maxY);
    }
    else if (cellX > 0 && cellY == 0) { // y out of bounds
        old_copy[cellX - 1][maxY - 1] = DEAD;
        current_state[cellX - 1][maxY - 1] = CellStatus(old_state, cellX - 1, maxY - 1, maxX, maxY);
    }
    else if (cellX == 0 && cellY == 0) { // both out of bounds
        old_copy[maxX - 1][maxY - 1] = DEAD;
        current_state[maxX - 1][maxY - 1] = CellStatus(old_state, maxX - 1, maxY - 1, maxX, maxY);
    }
#endif

    if (cellX < maxX - 1 && cellY < maxY - 1) { // bottom right
        old_copy[cellX + 1][cellY + 1] = DEAD;
        current_state[cellX + 1][cellY + 1] = CellStatus(old_state, cellX + 1, cellY + 1, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && cellY < maxY - 1) { // x out of bounds
        old_copy[0][cellY + 1] = DEAD;
        current_state[0][cellY + 1] = CellStatus(old_state, 0, cellY + 1, maxX, maxY);
    }
    else if (cellX < maxX - 1 && cellY == maxY - 1) { // y out of bounds
        old_copy[cellX + 1][0] = DEAD;
        current_state[cellX + 1][0] = CellStatus(old_state, cellX + 1, 0, maxX, maxY);
    }
    else if (cellX == maxX - 1 && cellY == maxY - 1) { // both out of bounds
        old_copy[0][0] = DEAD;
        current_state[0][0] = CellStatus(old_state, 0, 0, maxX, maxY);
    }
#endif

    if (cellX < maxX - 1 && cellY > 0) { // top right
        old_copy[cellX + 1][cellY - 1] = DEAD;
        current_state[cellX + 1][cellY - 1] = CellStatus(old_state, cellX + 1, cellY - 1, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellX == maxX - 1 && cellY > 0) { // x out of bounds
        old_copy[0][cellY - 1] = DEAD;
        current_state[0][cellY - 1] = CellStatus(old_state, 0, cellY - 1, maxX, maxY);
    }
    else if (cellX < maxX - 1 && cellY == 0) { // y out of bounds
        old_copy[cellX + 1][maxY - 1] = DEAD;
        current_state[cellX + 1][maxY - 1] = CellStatus(old_state, cellX + 1, maxY - 1, maxX, maxY);
    }
    else if (cellX == maxX - 1 && cellY == 0) { // both out of bounds
        old_copy[0][maxY - 1] = DEAD;
        current_state[0][maxY - 1] = CellStatus(old_state, 0, maxY - 1, maxX, maxY);
    }
#endif

    if (cellX > 0 && cellY < maxY - 1) { // bottom left
        old_copy[cellX - 1][cellY + 1] = DEAD;
        current_state[cellX - 1][cellY + 1] = CellStatus(old_state, cellX - 1, cellY + 1, maxX, maxY);
    }
#ifdef LOOPING
    else if (cellX == 0 && cellY < maxY - 1) { // x out of bounds
        old_copy[maxX - 1][cellY + 1] = DEAD;
        current_state[maxX - 1][cellY + 1] = CellStatus(old_state, maxX - 1, cellY + 1, maxX, maxY);
    }
    else if (cellX > 0 && cellY == maxY - 1) { // y out of bounds
        old_copy[cellX - 1][0] = DEAD;
        current_state[cellX - 1][0] = CellStatus(old_state, cellX - 1, 0, maxX, maxY);
    }
    else if (cellX == 0 && cellY == maxY - 1) { // both out of bounds
        old_copy[maxX - 1][0] = DEAD;
        current_state[maxX - 1][0] = CellStatus(old_state, maxX - 1, 0, maxX, maxY);
    }
#endif
}

void UpdateStateMatrix(bool** old_state, bool** current_state, int maxX, int maxY) {
    bool** old_copy = CopyMatrix(old_state, maxX, maxY);

    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (old_copy[cellX][cellY] == ALIVE) {
                current_state[cellX][cellY] = CellStatus(old_state, cellX, cellY, maxX, maxY);
                UpdateNeighbors(old_state, current_state, old_copy, maxX, maxY, cellX, cellY);
            }
        }
    }
}

// check if life has exausted itself in the system
bool Extinction(bool** state, int maxX, int maxY) {
    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (state[cellX][cellY] == ALIVE) {
                return false;
            }
        }
    }

    return true;
}

// checkes if the system reached a stable, static state
bool Stasis(bool** old_state, bool** current_state, int maxX, int maxY) {
    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (old_state[cellX][cellY] != current_state[cellX][cellY]) {
                return false;
            }
        }
    }

    return true;
}

// checkes if the activity level in the system has dropped too low
bool LowActivity(bool** old_state, bool** current_state, int maxX, int maxY, float threshold = 1) {
    const long total_cells = maxY * maxX;
    long cells_changed = 0;
    bool result = false;

    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (old_state[cellX][cellY] != current_state[cellX][cellY]) {
                cells_changed++;
            }
        }
    }

    if ((float)cells_changed / total_cells * 100 < threshold) {
        result = true;
    }

    return result;
}

// checks all stop conditions for the current run
bool EndOfCycle(bool** old_state, bool** current_state, int maxX, int maxY) {
    bool result = false;

#ifdef EFFICENT
    if (LowActivity(old_state, current_state, maxX, maxY)) {
        cout << "low activity" << endl;
        result = true;
    }
#endif // EFFICENT

#ifdef BRUTAL
    if (Extinction(current_state, maxX, maxY)) {
        cout << "life has beed wiped out" << endl;
        result = true;
    }
    else if (Stasis(old_state, current_state, maxX, maxY)) { // this is effectivly impossible
        cout << "a static state has beed achived" << endl;
        result = true;
    }
#endif // BRUTAL

    return result;
}

// fil the first generation according to selecte ruels
void Initialize(bool** current_state, int maxX, int maxY) {
#ifdef RANDOM
    // randomize the first generation
    srand(time(0));
    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (rand() % INITIAL_LIFE_RATIO == 0) {
                current_state[cellX][cellY] = true;
            }
            else {
                current_state[cellX][cellY] = false;
            }
        }
    }
#endif // RANDOM

#ifdef GLIDER
    //initialize an empty screen
    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            current_state[cellX][cellY] = false;
        }
    }

    int offsetX = 0;
    int offsetY = 0;
    int sizeX = 40;
    int sizeY = 40;
    int state = 0;

#ifdef TEXTPRINT
    sizeX /= 4;
    sizeY /= 4;
#endif // TEXTPRINT

    for (offsetX = 0; offsetX + sizeX <= maxX; offsetX += sizeX) {
        for (offsetY = 0; offsetY + sizeY <= maxY; offsetY += sizeY) {

            switch (state) {
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

}

// handle responce options
bool HandleResponse(char response) {
    bool result = false;
    if (response == 'y' or response == 'Y') {
        result = true;
    }

    return result;
}

void OutLineCaller(bool** current_state, int maxX, int  maxY) {


    COLORREF* colors = (COLORREF*)calloc(maxX * maxY, sizeof(COLORREF));
    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (current_state[cellX][cellY] == ALIVE) {
                colors[cellY * maxX + cellX] = COLOR_ALIVE;
            }
            else {
                colors[cellY * maxX + cellX] = COLOR_DEAD;
            }
        }
    }


    bool** state_copy = CopyMatrix(current_state, maxX, maxY);
    vector<point> cluster;
    sector outline;

    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (state_copy[cellX][cellY] == ALIVE) {
                FindCluster(state_copy, maxX, maxY, cellX, cellY, cluster);
                if (cluster.size() > MIN_SURVIVAL) { // clusters of less than the constant CANNOT survive, no matter the arrangment
                    FindClusterOutline(cluster, outline);
                    DrawSectorOutline(outline, colors, maxX, maxY);
                }
                cluster.clear();
            }
        }
    }

    // display block
    HWND myconsole = GetConsoleWindow();
    HDC mydc = GetDC(myconsole);
    HBITMAP bitmap = CreateBitmap(maxX, maxY, 1, 8 * 4, (void*)colors);
    HDC scr = CreateCompatibleDC(mydc);
    SelectObject(scr, bitmap);
    BitBlt(mydc, 0, 0, maxX, maxY, scr, 0, 0, SRCCOPY);
    
    DeleteObject(bitmap);
    DeleteObject(scr);
    free(colors);
    DeleteMatrix<bool>(state_copy);
}

int main() {
    // sets makes the window fullscreen
    SetConsoleDisplayMode(GetStdHandle(STD_OUTPUT_HANDLE), CONSOLE_FULLSCREEN_MODE, 0);
    ShowScrollBar(GetConsoleWindow(), SB_VERT, 0);
    ShowConsoleCursor(false);
    Sleep(1500);


    // get screen rezsolution
    int maxX = 0;
    int maxY = 0;

#ifdef NATIVE
    GetDesktopResolution(maxX, maxY);
#endif // NATIVE

#ifdef FHD
    maxX = 1920;
    maxY = 1080;
#endif // FHD

#ifdef TEXTPRINT
    //maxX /= 40; // adapt to a managable size
    //maxY /= 40;
    maxX = 6;
    maxY = 3;
#endif // TEXTPRINT

    bool** current_state = CreateMatrix<bool>(maxX, maxY);
    bool** old_state = CreateMatrix<bool>(maxX, maxY); // purly to stop the 'uninitialised used'
    bool initial;
    int gen = 0;

#ifdef UNCERTAIN
    char response = 'y';
#endif // UNCERTAIN

#ifdef RESURGENT
    while (true)
#endif // RESURGENT
#ifdef UNCERTAIN
        while (HandleResponse(response) != false)
#endif // UNCERTAIN

#ifndef DOOMED
        {
#endif // DOOMED

            // fill the sceeen according to settings
            Initialize(current_state, maxX, maxY);
            DeleteMatrix<bool>(old_state);
            old_state = CopyMatrix(current_state, maxX, maxY);
            initial = true;
            gen = 0;

            // initial render
            //UpdateScreen(current_state, maxX, maxY);
            OutLineCaller(current_state, maxX, maxY);

            while (initial || !EndOfCycle(old_state, current_state, maxX, maxY) && gen < 100) {

#if defined(PATIENT) || defined(TEXTPRINT)
                Sleep(1500);
#endif // defined(PATIENT) || defined(TEXTPRINT)

                gen++;
                initial = false;
                //ClearSectorOutlines(outlines);

                DeleteMatrix<bool>(old_state);
                old_state = CopyMatrix(current_state, maxX, maxY);
                UpdateStateMatrix(old_state, current_state, maxX, maxY);

                //UpdateScreen(current_state, maxX, maxY, old_state, false);
                OutLineCaller(current_state, maxX, maxY);
            }

#ifdef UNCERTAIN
            cout << "restart? (y/n)" << endl;
            cin >> response;
#endif // UNCERTAIN

#ifndef DOOMED
        }
#endif // DOOMED


        //DeleteMatrix<bool>(old_state);
        DeleteMatrix<bool>(current_state);
        return 0;
    }

    // refactoring help:
    // lover camel case [a-z]+[A-Z0-9][a-z0-9]+[A-Za-z0-9]*
    // upper camel case [A-Z][a-z0-9]*[A-Z0-9][a-z0-9]+[A-Za-z0-9]*
    // pointer parameters [a-zA-Z>]& 
