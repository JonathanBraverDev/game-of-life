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
#define NODELAY
// PATIENT will wait before rendering each genereation
// NODELAY will disable the sleep between renders
#define BRUTAL
// EFFICENT will run until the activity drops low enogh to be boring
// BRUTAL will disable activity detection, running to the bitter end (and probably never getting there)
#define TEXTPRINT
// LINEAR will render the scren as a big chunk
// MULTIRENDER will split rendering into a few columns
// TEXTPRINT will print the grid in text instead of pixels at significantly reduced rez and speed
#define UNCERTAIN
// DOOMED will end the simulation after one iteration ends
// UNCERTAIN will give the user a choise
// RESURGENT will endlessly reset the simulation without confirmation
#define LOOPING
// LOOPING will cause the screen to loop like in the game asteroids (simplest example)
// TRAPPED will enforce the screen borders, eliminating anything that dares to leave

using namespace std;
#ifdef RANDOM
constexpr auto INITIAL_LIFE_RATIO = 10; // this is an INVERSE (aka 1 is EVERY pixel)
#endif // RANDOM
const int LONELINESS     = 2;
const int MIN_SURVIVAL   = 2;
const int MAX_SURVIVAL   = 3;
const int MIN_MULTIPLY   = 3;
const int MAX_MULTIPLY   = 3;
const int OVERPOPULATION = 4;
const bool ALIVE = true;
const bool DEAD = false;

// Set colors
const COLORREF COLOR_ALIVE = RGB(255, 255, 255);
const COLORREF COLOR_DEAD = RGB(0, 0, 0);
const COLORREF COLOR_RED = RGB(255, 0, 0);
const COLORREF COLOR_GREEN = RGB(0, 255, 0);

// Define structures
struct snapshot
{
    int horizontal;
    int vertical;
    bool** local_state;
};

struct entry
{
    int maxHorizontal;
    int maxVertical;
    snapshot* snapshots;
};

struct point
{
    int horizontal;
    int vertical;
};

struct sector
{
    int horizontalStart;
    int horizontalEnd;
    int verticalStart;
    int verticalEnd;
};


// creates a secors with the given limits
sector createSector(int horizontalStart, int horizontalEnd, int verticalStart, int verticalEnd) {
    sector newSector{};
    newSector.horizontalStart = horizontalStart;
    newSector.horizontalEnd = horizontalEnd;
    newSector.verticalStart = horizontalStart;
    newSector.verticalEnd = verticalEnd;

    return newSector;
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

// copies an existing matrix, given size is of the original
template <typename T>
T** copyMatrix(T** original, int horizontal, int  vertical) {
    T** newMatrix = new T * [horizontal];
    newMatrix[0] = new T[horizontal * vertical];
    for (int cellX = 1; cellX < horizontal; cellX++) {
        newMatrix[cellX] = newMatrix[0] + cellX * vertical;
    }

    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        copy(original[cellX], original[cellX] + vertical, newMatrix[cellX]);
    }

    return newMatrix;
}


// returns a smaller matrix of an area in the state
bool** createLocalState(bool** state, int horizontalStart, int horizontalEnd, int verticalStart, int verticalEnd) {
    bool** local_state = createMatrix<bool>(horizontalEnd - horizontalStart, verticalEnd - horizontalStart);

    for (int cellX = horizontalStart; cellX < horizontalEnd; cellX++)
    {
        for (int cellY = verticalStart; cellY < verticalEnd; cellY++)
        {
            local_state[cellX - horizontalStart][cellY - verticalStart] = state[cellX][cellY];
        }
    }

    return local_state;
}

// returns a smaller matrix of an area in the state
bool** createLocalState(bool** state, sector sector) {
    bool** local_state = createMatrix<bool>(sector.horizontalEnd - sector.horizontalStart, sector.verticalEnd - sector.horizontalStart);

    for (int cellX = sector.horizontalStart; cellX < sector.horizontalEnd; cellX++)
    {
        for (int cellY = sector.verticalStart; cellY < sector.verticalEnd; cellY++)
        {
            local_state[cellX - sector.horizontalStart][cellY - sector.verticalStart] = state[cellX][cellY];
        }
    }

    return local_state;
}


// returns a snapshot of a cestion of the state within the given coprdinates
snapshot createSnapshot(bool** state, int horizontalStart, int horizontalEnd, int verticalStart, int verticalEnd) {
    snapshot newSnapshot{};
    newSnapshot.local_state = createLocalState(state, horizontalStart, horizontalEnd, verticalStart, verticalEnd);
    newSnapshot.horizontal = horizontalEnd - horizontalStart;
    newSnapshot.vertical = verticalEnd - verticalStart;

    return newSnapshot;
}

// returns a snapshot of a cestion of the state within the given coprdinates
snapshot createSnapshot(bool** state, sector sector) {
    snapshot newSnapshot{};
    newSnapshot.local_state = createLocalState(state, sector.horizontalStart, sector.horizontalEnd, sector.verticalStart, sector.verticalEnd);
    newSnapshot.horizontal = sector.horizontalEnd - sector.horizontalStart;
    newSnapshot.vertical = sector.verticalEnd - sector.verticalStart;

    return newSnapshot;
}

// finds a cluster of pointds connected to the given coordinates and saves them to the given vector
void findCluster(bool** state_copy, int horizontal, int vertical, int horizontalStart, int verticalStart, vector<point>& current_cluster) {

    if (state_copy[horizontalStart][verticalStart] == DEAD) // prevents 'double tagging' a cell based on the order they recurse at, still calls the function tho
    {
        return;
    }

    point currentPoint = point{ horizontalStart, verticalStart };
    current_cluster.push_back(currentPoint);
    state_copy[horizontalStart][verticalStart] = DEAD; // marking saved cells as dead

    vector<point> neighbors;

    // counting neighbors, looping edges
    if (horizontalStart > 0 && state_copy[horizontalStart - 1][verticalStart] == ALIVE) { // left 
        neighbors.push_back({ horizontalStart - 1,  verticalStart });
    }
    else if (horizontalStart == 0 && state_copy[horizontal - 1][verticalStart] == ALIVE) // looping left
    {
        neighbors.push_back({ horizontal - 1,  verticalStart });
    }

    if (verticalStart > 0 && state_copy[horizontalStart][verticalStart - 1] == ALIVE) { // up
        neighbors.push_back({ horizontalStart,  verticalStart - 1 });
    }
    else if (verticalStart == 0 && state_copy[horizontalStart][vertical - 1] == ALIVE) // looping up
    {
        neighbors.push_back({ horizontalStart,  vertical - 1 });
    }

    if (horizontalStart < horizontal - 1 && state_copy[horizontalStart + 1][verticalStart] == ALIVE) { // right
        neighbors.push_back({ horizontalStart + 1,  verticalStart });
    }
    else if (horizontalStart == horizontal - 1 && state_copy[0][verticalStart] == ALIVE) // looping right
    {
        neighbors.push_back({ 0,  verticalStart });
    }

    if (verticalStart < vertical - 1 && state_copy[horizontalStart][verticalStart + 1] == ALIVE) { // down
        neighbors.push_back({ horizontalStart,  verticalStart + 1 });
    }
    else if (verticalStart == vertical - 1 && state_copy[horizontalStart][0] == ALIVE) // looping down
    {
        neighbors.push_back({ horizontalStart,  0 });
    }

    if (horizontalStart > 0 && verticalStart > 0
        && state_copy[horizontalStart - 1][verticalStart - 1] == ALIVE) { // top left
        neighbors.push_back({ horizontalStart - 1,  verticalStart - 1 });
    }
    else if (horizontalStart == 0 && verticalStart > 0 // x out of bounds
        && state_copy[horizontal - 1][verticalStart - 1] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  verticalStart - 1 });
    }
    else if (horizontalStart > 0 && verticalStart == 0 // y out of bounds
        && state_copy[horizontalStart - 1][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ horizontalStart - 1,  vertical - 1 });
    }
    else if (horizontalStart == 0 && verticalStart == 0 // both out of bounds
        && state_copy[horizontal - 1][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  vertical - 1 });
    }

    if (horizontalStart < horizontal - 1 && verticalStart < vertical - 1 // bottom right
        && state_copy[horizontalStart + 1][verticalStart + 1] == ALIVE) {
        neighbors.push_back({ horizontalStart + 1,  verticalStart + 1 });
    }
    else if (horizontalStart == horizontal - 1 && verticalStart < vertical - 1 // x out of bounds
        && state_copy[0][verticalStart + 1] == ALIVE)
    {
        neighbors.push_back({ 0,  verticalStart + 1 });
    }
    else if (horizontalStart < horizontal - 1 && verticalStart == vertical - 1 // y out of bounds
        && state_copy[horizontalStart + 1][0] == ALIVE)
    {
        neighbors.push_back({ horizontalStart + 1,  0 });
    }
    else if (horizontalStart == horizontal - 1 && verticalStart == vertical - 1 // both out of bounds
        && state_copy[0][0] == ALIVE)
    {
        neighbors.push_back({ 0,  0 });
    }

    if (horizontalStart < horizontal - 1 && verticalStart > 0 // top right
        && state_copy[horizontalStart + 1][verticalStart - 1] == 1) {
        neighbors.push_back({ horizontalStart + 1,  verticalStart - 1 });
    }
    else if (horizontalStart == horizontal - 1 && verticalStart > 0 // x out of bounds
        && state_copy[0][verticalStart - 1] == ALIVE)
    {
        neighbors.push_back({ 0,  verticalStart - 1 });
    }
    else if (horizontalStart < horizontal - 1 && verticalStart == 0 // y out of bounds
        && state_copy[horizontalStart + 1][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ horizontalStart + 1,  vertical - 1 });
    }
    else if (horizontalStart == horizontal - 1 && verticalStart == 0 // both out of bounds
        && state_copy[0][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ 0,  vertical - 1 });
    }

    if (horizontalStart > 0 && verticalStart < vertical - 1 // bottom left
        && state_copy[horizontalStart - 1][verticalStart + 1] == 1) {
        neighbors.push_back({ horizontalStart - 1,  verticalStart + 1 });
    }
    else if (horizontalStart == 0 && verticalStart < vertical - 1 // x out of bounds
        && state_copy[horizontal - 1][verticalStart + 1] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  verticalStart + 1 });
    }
    else if (horizontalStart > 0 && verticalStart == vertical - 1 // y out of bounds 
        && state_copy[horizontalStart - 1][0] == ALIVE)
    {
        neighbors.push_back({ horizontalStart - 1, 0 });
    }
    else if (horizontalStart == 0 && verticalStart == vertical - 1 // both out of bounds 
        && state_copy[horizontal - 1][0] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  0 });
    }

    while (!neighbors.empty())
    {
        // recourse using a neigboring live cells' location
        findCluster(state_copy, horizontal, vertical, neighbors.back().horizontal, neighbors.back().vertical, current_cluster);
        neighbors.pop_back(); // remove the last value
    }
}


// finds all clusters in the given state and saves them to the given vector
void findClusters(bool** state, int horizontal, int vertical, vector<vector<point>>& clusters) {

    bool** state_copy = copyMatrix(state, horizontal, vertical);

    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (state_copy[cellX][cellY] == ALIVE)
            {
                clusters.push_back(vector<point>{});
                findCluster(state_copy, horizontal, vertical, cellX, cellY, clusters.back());
                if (clusters.back().size() < 3) // clusters of less than 3 cells CANNOT survive, no matter the arrangment
                {
                    clusters.pop_back();
                }
            }
        }
    }
}

sector findClusterOutline(vector<point> cluster) {
    sector outline = {};
    point currentPoint;

    outline.horizontalStart = INT_MAX;
    outline.horizontalEnd = INT_MIN;
    outline.verticalStart = INT_MAX;
    outline.verticalEnd = INT_MIN;

    while (!cluster.empty())
    {
        currentPoint = cluster.back();
    
        if (outline.horizontalStart > currentPoint.horizontal) {
            outline.horizontalStart = currentPoint.horizontal;
        } 

        if (outline.horizontalEnd < currentPoint.horizontal) {
            outline.horizontalEnd = currentPoint.horizontal;
        }

        if (outline.verticalStart > currentPoint.vertical) {
            outline.verticalStart = currentPoint.vertical;
        }

        if (outline.verticalEnd < currentPoint.vertical) {
            outline.verticalEnd = currentPoint.vertical;
        }

        cluster.pop_back();
    }

    return outline;
}

vector<sector> findClusterOutlines(vector<vector<point>> clusters) {
    vector<sector> outlines;

    while (!clusters.empty())
    {
        outlines.push_back(findClusterOutline(clusters.back()));

        clusters.pop_back();
    }

    return outlines;
}


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

#ifdef MULTIRENDER
void renderThread(bool** current_state, int vertical, int begin, int end, bool** old_state, bool initial) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    for (int cellX = begin; cellX < end; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (initial || old_state[cellX][cellY] != current_state[cellX][cellY]) { // will not update uncahnged cells
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
#endif // MULTIRENDER


void updateScreen(bool** current_state, int horizontal, int vertical, bool** old_state = NULL, bool initial = true) {
#ifdef MULTIRENDER
    unsigned int numOfThreads = thread::hardware_concurrency();
    thread* threads = new thread[numOfThreads];
    const unsigned int renderZoneSize = horizontal / numOfThreads;
    unsigned int firstOffset = horizontal - renderZoneSize * numOfThreads;
    int begin = 0;
    int end = firstOffset + renderZoneSize; // the first zone takes the rounding error

    threads[0] = thread(renderThread, current_state, vertical, begin, end, old_state, initial);

    for (int created = 1; created < numOfThreads; created++) // !IMPORTANT! set to 1 to skip the first thread creation
    {
        begin = end;
        end += renderZoneSize;
        threads[created] = thread(renderThread, current_state, vertical, begin, end, old_state, initial);
    }

    for (int joined = 0; joined < numOfThreads; joined++) // !IMPORTANT! set to 1 to skip the first thread creation
    {
        threads[joined].join();
    }
#endif // MULTIRENDER

#ifdef LINEAR
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (initial || old_state[cellX][cellY] != current_state[cellX][cellY]) { // will not update uncahnged cells
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
#endif // LINEAR

#ifdef TEXTPRINT
    for (int celly = 0; celly < vertical; celly++) // y dominaant for newlines
    {
        for (int cellx = 0; cellx < horizontal; cellx++)
        {
            if (current_state[cellx][celly] == ALIVE)
            {
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

#if defined(PATIENT) || defined(TEXTPRINT)
    Sleep(1500);
#endif // defined(PATIENT) || defined(TEXTPRINT)
}

bool cellStatus(bool** old_state, int cellX, int cellY, int horizontal, int vertical) {
    int neighbors = 0;
    bool status = DEAD;

#ifdef LOOPING
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
#endif // TRAPPED


    if (old_state[cellX][cellY] == ALIVE && neighbors <= LONELINESS) {
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


void updateStateMatrix(bool** old_state, bool** current_state, int horizontal, int vertical) {
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

// deletes single block allocated matrixes 
template <typename T>
void deleteMatrix(T** matrix) {
    delete[] matrix[0];
    delete[] matrix;
}

// checks all stop conditions for the current run
bool endOfCycle(bool** old_state, bool** current_state, int horizontal, int vertical) {
    bool result = false;

#ifdef EFFICENT
    if (low_Activity(old_state, current_state, horizontal, vertical)) {
        cout << "low activity" << endl;
        result = true;
    }
#endif // EFFICENT

#ifdef BRUTAL
    if (extinction(current_state, horizontal, vertical)) {
        cout << "life has beed wiped out" << endl;
        result = true;
    }
    else if (stasis(old_state, current_state, horizontal, vertical)) { // this is effectivly impossible
        cout << "a static state has beed achived" << endl;
        result = true;
    }
#endif // BRUTAL

    return result;
}

// fil the first generation according to selecte ruels
void initialize(bool** current_state, int horizontal, int vertical) {
#ifdef RANDOM
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
#endif // RANDOM

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

#ifdef TEXTPRINT
    sizeX /= 4;
    sizeY /= 4;
#endif // TEXTPRINT

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

}

// handle responce options
bool handleResponse(char response) {
    bool result = false;
    if (response == 'y' or response == 'Y')
    {
        result = true;
    }

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

#ifdef NATIVE
    GetDesktopResolution(horizontal, vertical);
#endif // NATIVE

#ifdef FHD
    horizontal = 1920;
    vertical = 1080;
#endif // FHD

#ifdef TEXTPRINT
    horizontal /= 40; // adapt to a managable size
    vertical /= 40;
#endif // TEXTPRINT


    bool** old_state = createMatrix<bool>(horizontal, vertical);
    bool** current_state = createMatrix<bool>(horizontal, vertical);
    char response = 'y';

#ifdef RESURGENT
    while (true)
#endif // RESURGENT
#ifdef UNCERTAIN
        while (handleResponse(response) != false)
#endif // UNCERTAIN

#ifndef DOOMED
        {
#endif // DOOMED

            // fill the sceeen according to settings
            initialize(current_state, horizontal, vertical);

            // initial render
            updateScreen(current_state, horizontal, vertical);

            bool** tmp = copyMatrix(current_state, horizontal, vertical);
            vector<vector<point>> clusters;
            findClusters(tmp, horizontal, vertical, clusters);
            vector<sector> tmpp = findClusterOutlines(clusters);

            while (!endOfCycle(old_state, current_state, horizontal, vertical))
            {
                swap(old_state, current_state); // 'save' the old state without copying
                updateStateMatrix(old_state, current_state, horizontal, vertical);

                updateScreen(current_state, horizontal, vertical, old_state, false);
            }

#ifdef UNCERTAIN
            cout << "restart? (y/n)" << endl;
            cin >> response;
#endif // UNCERTAIN

#ifndef DOOMED
        }
#endif // DOOMED


    deleteMatrix<bool>(old_state);
    deleteMatrix<bool>(current_state);
    return 0;
}
