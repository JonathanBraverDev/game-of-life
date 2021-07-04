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
#define LINEAR
// LINEAR will render the scren as a big chunk
// MULTIRENDER will split rendering into a few columns
// TEXTPRINT will print the grid in text instead of pixels at significantly reduced rez and speed
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
constexpr auto INITIAL_LIFE_RATIO = 25; // this is an INVERSE (1 is EVERY pixel)
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
    int max_horizontal;
    int max_vertical;
    snapshot* snapshots;
};

struct point
{
    int horizontal;
    int vertical;
};

struct sector
{
    int horizontal_start;
    int horizontal_end;
    int vertical_start;
    int vertical_end;
};


// creates a secors with the given limits
sector CreateSector(int horizontal_start, int horizontal_end, int vertical_start, int vertical_end) {
    sector new_sector{};
    new_sector.horizontal_start = horizontal_start;
    new_sector.horizontal_end = horizontal_end;
    new_sector.vertical_start = horizontal_start;
    new_sector.vertical_end = vertical_end;

    return new_sector;
}


// creates a single block matrix allocation and splits it into arrays
template <typename T>
T** CreateMatrix(int horizontal, int vertical) {
    T** matrix = new T * [horizontal];
    matrix[0] = new T[horizontal * vertical];
    for (int cellX = 1; cellX < horizontal; cellX++) {
        matrix[cellX] = matrix[0] + cellX * vertical;
    }

    return matrix;
}

// copies an existing matrix, given size is of the original
template <typename T>
T** CopyMatrix(T** original, int horizontal, int vertical) {
    T** new_matrix = CreateMatrix<bool>(horizontal, vertical);

    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        copy(original[cellX], original[cellX] + vertical, new_matrix[cellX]);
    }

    return new_matrix;
}


// returns a smaller matrix of an area in the state
bool** CreateLocalState(bool** state, int horizontal_start, int horizontal_end, int vertical_start, int vertical_end) {
    bool** local_state = CreateMatrix<bool>(horizontal_end - horizontal_start, vertical_end - horizontal_start);

    for (int cellX = horizontal_start; cellX < horizontal_end; cellX++)
    {
        for (int cellY = vertical_start; cellY < vertical_end; cellY++)
        {
            local_state[cellX - horizontal_start][cellY - vertical_start] = state[cellX][cellY];
        }
    }

    return local_state;
}

// returns a smaller matrix of an area in the state
bool** CreateLocalState(bool** state, sector sector) {
    bool** local_state = CreateMatrix<bool>(sector.horizontal_end - sector.horizontal_start, sector.vertical_end - sector.horizontal_start);

    for (int cellX = sector.horizontal_start; cellX < sector.horizontal_end; cellX++)
    {
        for (int cellY = sector.vertical_start; cellY < sector.vertical_end; cellY++)
        {
            local_state[cellX - sector.horizontal_start][cellY - sector.vertical_start] = state[cellX][cellY];
        }
    }

    return local_state;
}


// returns a snapshot of a cestion of the state within the given coprdinates
snapshot CreateSnapshot(bool** state, int horizontal_start, int horizontal_end, int vertical_start, int vertical_end) {
    snapshot new_snapshot{};
    new_snapshot.local_state = CreateLocalState(state, horizontal_start, horizontal_end, vertical_start, vertical_end);
    new_snapshot.horizontal = horizontal_end - horizontal_start;
    new_snapshot.vertical = vertical_end - vertical_start;

    return new_snapshot;
}

// returns a snapshot of a cestion of the state within the given coprdinates
snapshot CreateSnapshot(bool** state, sector sector) {
    snapshot new_snapshot{};
    new_snapshot.local_state = CreateLocalState(state, sector.horizontal_start, sector.horizontal_end, sector.vertical_start, sector.vertical_end);
    new_snapshot.horizontal = sector.horizontal_end - sector.horizontal_start;
    new_snapshot.vertical = sector.vertical_end - sector.vertical_start;

    return new_snapshot;
}

// finds a cluster of points connected to the given coordinates and saves them to the given vector
void FindCluster(bool** state_copy, int horizontal, int vertical, int horizontal_start, int vertical_start, vector<point>& current_cluster) {

    if (state_copy[horizontal_start][vertical_start] == DEAD) // prevents 'double tagging' a cell based on the order they recurse at, still calls the function tho
    {
        return;
    }

    point current_point = point{ horizontal_start, vertical_start };
    current_cluster.push_back(current_point);
    state_copy[horizontal_start][vertical_start] = DEAD; // marking saved cells as dead

    vector<point> neighbors;

    // counting neighbors, looping edges
    if (horizontal_start > 0 && state_copy[horizontal_start - 1][vertical_start] == ALIVE) { // left 
        neighbors.push_back({ horizontal_start - 1,  vertical_start });
    }
#ifdef LOOPING
    else if (horizontal_start == 0 && state_copy[horizontal - 1][vertical_start] == ALIVE) // looping left
    {
        neighbors.push_back({ horizontal - 1,  vertical_start });
    }
#endif

    if (vertical_start > 0 && state_copy[horizontal_start][vertical_start - 1] == ALIVE) { // up
        neighbors.push_back({ horizontal_start,  vertical_start - 1 });
    }
#ifdef LOOPING
    else if (vertical_start == 0 && state_copy[horizontal_start][vertical - 1] == ALIVE) // looping up
    {
        neighbors.push_back({ horizontal_start,  vertical - 1 });
    }
#endif

    if (horizontal_start < horizontal - 1 && state_copy[horizontal_start + 1][vertical_start] == ALIVE) { // right
        neighbors.push_back({ horizontal_start + 1,  vertical_start });
    }
#ifdef LOOPING
    else if (horizontal_start == horizontal - 1 && state_copy[0][vertical_start] == ALIVE) // looping right
    {
        neighbors.push_back({ 0,  vertical_start });
    }
#endif

    if (vertical_start < vertical - 1 && state_copy[horizontal_start][vertical_start + 1] == ALIVE) { // down
        neighbors.push_back({ horizontal_start,  vertical_start + 1 });
    }
#ifdef LOOPING
    else if (vertical_start == vertical - 1 && state_copy[horizontal_start][0] == ALIVE) // looping down
    {
        neighbors.push_back({ horizontal_start,  0 });
    }
#endif

    if (horizontal_start > 0 && vertical_start > 0
        && state_copy[horizontal_start - 1][vertical_start - 1] == ALIVE) { // top left
        neighbors.push_back({ horizontal_start - 1,  vertical_start - 1 });
    }
#ifdef LOOPING
    else if (horizontal_start == 0 && vertical_start > 0 // x out of bounds
        && state_copy[horizontal - 1][vertical_start - 1] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  vertical_start - 1 });
    }
    else if (horizontal_start > 0 && vertical_start == 0 // y out of bounds
        && state_copy[horizontal_start - 1][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ horizontal_start - 1,  vertical - 1 });
    }
    else if (horizontal_start == 0 && vertical_start == 0 // both out of bounds
        && state_copy[horizontal - 1][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  vertical - 1 });
    }
#endif

    if (horizontal_start < horizontal - 1 && vertical_start < vertical - 1 // bottom right
        && state_copy[horizontal_start + 1][vertical_start + 1] == ALIVE) {
        neighbors.push_back({ horizontal_start + 1,  vertical_start + 1 });
    }
#ifdef LOOPING
    else if (horizontal_start == horizontal - 1 && vertical_start < vertical - 1 // x out of bounds
        && state_copy[0][vertical_start + 1] == ALIVE)
    {
        neighbors.push_back({ 0,  vertical_start + 1 });
    }
    else if (horizontal_start < horizontal - 1 && vertical_start == vertical - 1 // y out of bounds
        && state_copy[horizontal_start + 1][0] == ALIVE)
    {
        neighbors.push_back({ horizontal_start + 1,  0 });
    }
    else if (horizontal_start == horizontal - 1 && vertical_start == vertical - 1 // both out of bounds
        && state_copy[0][0] == ALIVE)
    {
        neighbors.push_back({ 0,  0 });
    }
#endif

    if (horizontal_start < horizontal - 1 && vertical_start > 0 // top right
        && state_copy[horizontal_start + 1][vertical_start - 1] == 1) {
        neighbors.push_back({ horizontal_start + 1,  vertical_start - 1 });
    }
#ifdef LOOPING
    else if (horizontal_start == horizontal - 1 && vertical_start > 0 // x out of bounds
        && state_copy[0][vertical_start - 1] == ALIVE)
    {
        neighbors.push_back({ 0,  vertical_start - 1 });
    }
    else if (horizontal_start < horizontal - 1 && vertical_start == 0 // y out of bounds
        && state_copy[horizontal_start + 1][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ horizontal_start + 1,  vertical - 1 });
    }
    else if (horizontal_start == horizontal - 1 && vertical_start == 0 // both out of bounds
        && state_copy[0][vertical - 1] == ALIVE)
    {
        neighbors.push_back({ 0,  vertical - 1 });
    }
#endif

    if (horizontal_start > 0 && vertical_start < vertical - 1 // bottom left
        && state_copy[horizontal_start - 1][vertical_start + 1] == 1) {
        neighbors.push_back({ horizontal_start - 1,  vertical_start + 1 });
    }
#ifdef LOOPING
    else if (horizontal_start == 0 && vertical_start < vertical - 1 // x out of bounds
        && state_copy[horizontal - 1][vertical_start + 1] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  vertical_start + 1 });
    }
    else if (horizontal_start > 0 && vertical_start == vertical - 1 // y out of bounds 
        && state_copy[horizontal_start - 1][0] == ALIVE)
    {
        neighbors.push_back({ horizontal_start - 1, 0 });
    }
    else if (horizontal_start == 0 && vertical_start == vertical - 1 // both out of bounds 
        && state_copy[horizontal - 1][0] == ALIVE)
    {
        neighbors.push_back({ horizontal - 1,  0 });
    }
#endif

    while (!neighbors.empty())
    {
        // recourse using a neigboring live cells' location
        FindCluster(state_copy, horizontal, vertical, neighbors.back().horizontal, neighbors.back().vertical, current_cluster);
        neighbors.pop_back(); // remove the last value
    }
}


// finds all clusters in the given state and saves them to the given vector
void FindClusters(bool** state, int horizontal, int vertical, vector<vector<point>>& clusters) {

    bool** state_copy = CopyMatrix(state, horizontal, vertical);

    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            if (state_copy[cellX][cellY] == ALIVE)
            {
                clusters.push_back(vector<point>{});
                FindCluster(state_copy, horizontal, vertical, cellX, cellY, clusters.back());
                if (clusters.back().size() < 3) // clusters of less than 3 cells CANNOT survive, no matter the arrangment
                {
                    clusters.pop_back();
                }
            }
        }
    }
}

sector FindClusterOutline(vector<point> cluster) {
    sector outline = {};
    point current_point;

    outline.horizontal_start = INT_MAX;
    outline.horizontal_end = INT_MIN;
    outline.vertical_start = INT_MAX;
    outline.vertical_end = INT_MIN;

    while (!cluster.empty())
    {
        current_point = cluster.back();

        if (outline.horizontal_start > current_point.horizontal) {
            outline.horizontal_start = current_point.horizontal;
        }

        if (outline.horizontal_end < current_point.horizontal) {
            outline.horizontal_end = current_point.horizontal;
        }

        if (outline.vertical_start > current_point.vertical) {
            outline.vertical_start = current_point.vertical;
        }

        if (outline.vertical_end < current_point.vertical) {
            outline.vertical_end = current_point.vertical;
        }

        cluster.pop_back();
    }

    return outline;
}

vector<sector> FindClusterOutlines(vector<vector<point>> clusters) {
    vector<sector> outlines;

    while (!clusters.empty())
    {
        outlines.push_back(FindClusterOutline(clusters.back()));

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
void RenderThread(bool** current_state, int vertical, int begin, int end, bool** old_state, bool initial) {
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


void UpdateScreen(bool** current_state, int horizontal, int vertical, bool** old_state = NULL, bool initial = true) {
#ifdef MULTIRENDER
    unsigned int num_of_threads = thread::hardware_concurrency();
    thread* threads = new thread[num_of_threads];
    const unsigned int render_zone_size = horizontal / num_of_threads;
    unsigned int first_offset = horizontal - render_zone_size * num_of_threads;
    int begin = 0;
    int end = first_offset + render_zone_size; // the first zone takes the rounding error

    threads[0] = thread(RenderThread, current_state, vertical, begin, end, old_state, initial);

    for (int created = 1; created < num_of_threads; created++) // !IMPORTANT! set to 1 to skip the first thread creation
    {
        begin = end;
        end += render_zone_size;
        threads[created] = thread(RenderThread, current_state, vertical, begin, end, old_state, initial);
    }

    for (int joined = 0; joined < num_of_threads; joined++) // !IMPORTANT! set to 1 to skip the first thread creation
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

    ReleaseDC(myconsole, mydc);
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

void DrawSectorOutline(sector sector) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    int cellX;
    int cellY = sector.vertical_start - 1;

    for (cellX = sector.horizontal_start - 1; cellX <= sector.horizontal_end + 1; cellX++) // top line
    {
        SetPixel(mydc, cellX, cellY, COLOR_GREEN);
    }

    cellY = sector.vertical_end + 1;
    for (cellX = sector.horizontal_start - 1; cellX <= sector.horizontal_end + 1; cellX++) // bottom line
    {
        SetPixel(mydc, cellX, cellY, COLOR_GREEN);
    }

    cellX = sector.horizontal_start - 1;
    for (cellY = sector.vertical_start - 1; cellY <= sector.vertical_end + 1; cellY++) // left line
    {
        SetPixel(mydc, cellX, cellY, COLOR_GREEN);
    }

    cellX = sector.horizontal_end + 1;
    for (cellY = sector.vertical_start - 1; cellY <= sector.vertical_end + 1; cellY++) // right line
    {
        SetPixel(mydc, cellX, cellY, COLOR_GREEN);
    }

    ReleaseDC(myconsole, mydc);
}

void DrawSectorOutlines(vector<sector> sectors) {
    //Get a console handle
    HWND myconsole = GetConsoleWindow();
    //Get a handle to device context
    HDC mydc = GetDC(myconsole);

    while (!sectors.empty())
    {
        DrawSectorOutline(sectors.back());
        sectors.pop_back();
    }

    ReleaseDC(myconsole, mydc);
}

bool CellStatus(bool** old_state, int cellX, int cellY, int horizontal, int vertical) {
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


void UpdateStateMatrix(bool** old_state, bool** current_state, int horizontal, int vertical) {
    for (int cellX = 0; cellX < horizontal; cellX++)
    {
        for (int cellY = 0; cellY < vertical; cellY++)
        {
            current_state[cellX][cellY] = CellStatus(old_state, cellX, cellY, horizontal, vertical);
        }
    }
}

// check if life has exausted itself in the system
bool Extinction(bool** state, int horizontal, int vertical) {
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
bool Stasis(bool** old_state, bool** current_state, int horizontal, int vertical) {
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
bool LowActivity(bool** old_state, bool** current_state, int horizontal, int vertical, float threshold = 1) {
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
void DeleteMatrix(T** matrix) {
    delete[] matrix[0];
    delete[] matrix;
}

// checks all stop conditions for the current run
bool EndOfCycle(bool** old_state, bool** current_state, int horizontal, int vertical) {
    bool result = false;

#ifdef EFFICENT
    if (LowActivity(old_state, current_state, horizontal, vertical)) {
        cout << "low activity" << endl;
        result = true;
    }
#endif // EFFICENT

#ifdef BRUTAL
    if (Extinction(current_state, horizontal, vertical)) {
        cout << "life has beed wiped out" << endl;
        result = true;
    }
    else if (Stasis(old_state, current_state, horizontal, vertical)) { // this is effectivly impossible
        cout << "a static state has beed achived" << endl;
        result = true;
    }
#endif // BRUTAL

    return result;
}

// fil the first generation according to selecte ruels
void Initialize(bool** current_state, int horizontal, int vertical) {
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
bool HandleResponse(char response) {
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


    bool** old_state = CreateMatrix<bool>(horizontal, vertical);
    bool** current_state = CreateMatrix<bool>(horizontal, vertical);
    char response = 'y';

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
            Initialize(current_state, horizontal, vertical);

            // initial render
            UpdateScreen(current_state, horizontal, vertical);

            bool** tmp = CopyMatrix(current_state, horizontal, vertical);
            vector<vector<point>> clusters;
            FindClusters(tmp, horizontal, vertical, clusters);
            vector<sector> tmpp = FindClusterOutlines(clusters);
            DrawSectorOutlines(tmpp);

            DeleteMatrix<bool>(tmp);

            while (!EndOfCycle(old_state, current_state, horizontal, vertical))
            {
                swap(old_state, current_state); // 'save' the old state without copying
                UpdateStateMatrix(old_state, current_state, horizontal, vertical);

                UpdateScreen(current_state, horizontal, vertical, old_state, false);
            }

#ifdef UNCERTAIN
            cout << "restart? (y/n)" << endl;
            cin >> response;
#endif // UNCERTAIN

#ifndef DOOMED
        }
#endif // DOOMED


    DeleteMatrix<bool>(old_state);
    DeleteMatrix<bool>(current_state);
    return 0;
}

// refactoring help:
// lover camel case [a-z]+[A-Z0-9][a-z0-9]+[A-Za-z0-9]*
// upper camel case [A-Z][a-z0-9]*[A-Z0-9][a-z0-9]+[A-Za-z0-9]*
