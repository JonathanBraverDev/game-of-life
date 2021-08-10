#include <windows.h>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <thread>
#include <vector>
#include <queue>

#define RANDOM
// RANDOM will start with a randomly genereted first generation
// GLIDER will start will a screen full of gliders
#define FHD
// FHD will force the screen to 1080p, windows scaling fucks up the automatic detection
// NATIVE will allow adapting to the resolution of the display
#define NODELAY
// PATIENT will wait before rendering each genereation
// SCENIC will slow the rendering enough for comftable viewing
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
#define LARGE
// NOOUTLINES will not show outlines // !IMPORTANT! uses more memory that outlines, 23 vs 16
// SURVIVING will outline clusters who are expected to survive
// NOTABLE will outline clusters with more likly activity
// LARGE will outline clusters of significant size

#define LUSH
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
const int BORDER_SIZE = 1;


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
const int MIN_MULTIPLY = 3;
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

struct link
{
    point data;
    link* next;
    bool  free;
};

//creates a link with the given data
link* AddLink(point data, link* head) {
    link* link = (struct link*)malloc(sizeof(struct link));
    link->data = data;
    link->next = head;
    link->free = false;

    return link;
}

// adding date to empty nodes, creating a new one if needed
// !IMPORTAMT! the new node will REPLACE THE HEAD while data is ADDED TO THE LAST FREE SLOT
link* AddData(point data, link* head) {
    link* backup = head;
    bool added = false;
    while (!added && head != nullptr) {
        if (head->free) {
            head->data = data;
            head->free = false;
            added = true;
        }
        head = head->next;
    }

    if (!added) { // if no free cells were found
        head = AddLink(data, backup);
    }
    else {
        head = backup;
    }

    return head;
}

// deletes the entire link chain
void DeleteChain(link* head) {
    link* tmp;
    while (head != nullptr) {
        tmp = head->next;
        delete head;
        head = tmp;
    }
}

// overrites the data in the nodes with empty points
void DeleteChainData(link* head) {
    while (head != nullptr) {
        head->free = true;
        head = head->next;
    }
}

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

// fills the given array with cells nearby, alive or all according to the setting
void FindAllNeighbors(bool** state_copy, int maxX, int maxY, int cellX, int cellY, point* neighbors, bool* filled, bool live_only = false) {
    int i = 0;
    point tmp;

    // counting neighbors, looping edges
    filled[i] = false;
    if (cellX > BORDER_SIZE && (!live_only || state_copy[cellX - 1][cellY] == ALIVE)) { // left
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    i++;

    filled[i] = false;
    if (cellY > BORDER_SIZE && (!live_only || state_copy[cellX][cellY - 1] == ALIVE)) { // up
        tmp.cellX = cellX;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    i++;

    filled[i] = false;
    if (cellX < maxX - BORDER_SIZE - 1 && (!live_only || state_copy[cellX + 1][cellY] == ALIVE)) { // right
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    i++;

    filled[i] = false;
    if (cellY < maxY - BORDER_SIZE - 1 && (!live_only || state_copy[cellX][cellY + 1] == ALIVE)) { // down
        tmp.cellX = cellX;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    i++;

    filled[i] = false;
    if (cellX > BORDER_SIZE && cellY > BORDER_SIZE // top left
        && (!live_only || state_copy[cellX - 1][cellY - 1] == ALIVE)) {
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    i++;

    filled[i] = false;
    if (cellX < maxX - BORDER_SIZE - 1 && cellY < maxY - BORDER_SIZE - 1 // bottom right
        && (!live_only || state_copy[cellX + 1][cellY + 1] == ALIVE)) {
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    i++;

    filled[i] = false;
    if (cellX < maxX - BORDER_SIZE - 1 && cellY > BORDER_SIZE // top right
        && (!live_only || state_copy[cellX + 1][cellY - 1] == ALIVE)) {
        tmp.cellX = cellX + 1;
        tmp.cellY = cellY - 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
    i++;

    filled[i] = false;
    if (cellX > BORDER_SIZE && cellY < maxY - BORDER_SIZE - 1 // bottom left
        && (!live_only || state_copy[cellX - 1][cellY + 1] == ALIVE)) {
        tmp.cellX = cellX - 1;
        tmp.cellY = cellY + 1;
        neighbors[i] = tmp;
        filled[i] = true;
    }
}

// updates the min/max values of x and y of the outline considering the new point as a part of the same cluster
void UpdateClusterOutline(point& point, sector& outline) {
    if (outline.startX > point.cellX) {
        outline.startX = point.cellX;
    }

    if (outline.endX < point.cellX) {
        outline.endX = point.cellX;
    }

    if (outline.startY > point.cellY) {
        outline.startY = point.cellY;
    }

    if (outline.endY < point.cellY) {
        outline.endY = point.cellY;
    }
}

// finds a cluster of points connected to the given coordinates and saves them to the given chain
void FindCluster(bool** state_copy, int maxX, int maxY, int cellX, int cellY, link* head, int& cluster_size, sector& outline, point* neighbors, bool* filled) {
    
    outline.startX = INT_MAX;
    outline.endX = INT_MIN;
    outline.startY = INT_MAX;
    outline.endY = INT_MIN;
    queue<point> check_list;
    cluster_size = 0;

    point current_point = point{ cellX, cellY };
    UpdateClusterOutline(current_point, outline);
    state_copy[cellX][cellY] = DEAD; // marking saved cells as dead
    cluster_size++;

    FindAllNeighbors(state_copy, maxX, maxY, cellX, cellY, neighbors, filled, true);
    for (int i = 0; i < 8; i++) {
        if (filled[i]) {
            check_list.push(point{ neighbors[i].cellX, neighbors[i].cellY });
        }
    }

    while (!check_list.empty())
    {
        current_point = check_list.front();
        if (state_copy[current_point.cellX][current_point.cellY] = ALIVE) // check if the cell was procces by another neighbor
        {
            UpdateClusterOutline(current_point, outline);
            state_copy[current_point.cellX][current_point.cellY] = DEAD;
            cluster_size++;

            FindAllNeighbors(state_copy, maxX, maxY, cellX, cellY, neighbors, filled, true);
            for (int i = 0; i < 8; i++) {
                if (filled[i]) {
                    check_list.push(point{ neighbors[i].cellX, neighbors[i].cellY });
                }
            }
        }

        check_list.pop();
    }
}

// finds all clusters in the given state and saves them to the given vector
//void FindClusters(bool** state, int maxX, int maxY, vector<vector<point>>& clusters) {
//    bool** state_copy = CopyMatrix(state, maxX, maxY);
//
//    for (int cellX = 0; cellX < maxX; cellX++) {
//        for (int cellY = 0; cellY < maxY; cellY++) {
//            if (state_copy[cellX][cellY] == ALIVE) {
//                clusters.push_back(vector<point>{});
//                FindCluster(state_copy, maxX, maxY, cellX, cellY, clusters.back());
//                if (clusters.back().size() <= MIN_SURVIVAL) { // clusters of less than the constant CANNOT survive, no matter the arrangment
//                    clusters.pop_back();
//                }
//            }
//        }
//    }
//
//    DeleteMatrix<bool>(state_copy);
//}

// writes all outlines in the given vector to the given outline vector
//void UpdateClusterOutlines(vector<vector<point>>& clusters, vector<sector>& outlines) {
//    while (!clusters.empty()) {
//        outlines.push_back(sector{});
//        UpdateClusterOutline(clusters.back(), outlines.back());
//
//        clusters.pop_back();
//    }
//}

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

// chances console cursor visibility
void ShowConsoleCursor(bool showFlag)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_CURSOR_INFO     cursorInfo;

    GetConsoleCursorInfo(out, &cursorInfo);
    cursorInfo.bVisible = showFlag; // set the cursor visibility
    SetConsoleCursorInfo(out, &cursorInfo);
}

#ifdef MULTIRENDER
// a thread used to render more places simultaniusly
void RenderThread(bool** current_state, int maxY, int begin, int end, bool** old_state, bool initial) {
    HWND myconsole = GetConsoleWindow();
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

// updates the display to the currest state of the world using the chosen method
void UpdateScreen(bool** current_state, int maxX, int maxY, bool** old_state = NULL, bool initial = true, COLORREF* colors = nullptr) {
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
    HWND myconsole = GetConsoleWindow();
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
    bool del = false;
    int real_maxX = maxX - BORDER_SIZE;
    int real_maxY = maxY - BORDER_SIZE;
    if (colors == nullptr)
    {
        colors = (COLORREF*)calloc(maxX * maxY, sizeof(COLORREF));
        del = true;
    }
    for (int cellX = BORDER_SIZE; cellX < real_maxX; cellX++) {
        for (int cellY = BORDER_SIZE; cellY < real_maxY; cellY++) {
            if (current_state[cellX][cellY] == ALIVE) {
                colors[cellY * real_maxX + cellX] = COLOR_ALIVE;
            }
            else {
                colors[cellY * real_maxX + cellX] = COLOR_DEAD;
            }
        }
    }
    
    HWND myconsole = GetConsoleWindow();
    HDC mydc = GetDC(myconsole);
    HBITMAP bitmap = CreateBitmap(real_maxX, real_maxY, 1, 8 * 4, (void*)colors);
    HDC scr = CreateCompatibleDC(mydc);
    SelectObject(scr, bitmap);
    BitBlt(mydc, 0, 0, real_maxX, real_maxY, scr, 0, 0, SRCCOPY);

    //DeleteObject(bitmap); // destroyed when function ends
    //DeleteObject(scr);
    if (del) {
        free(colors);
    }
#endif // BITMAP
}

// adds an outline of a sector to the display
void DrawSectorOutline(sector sector, COLORREF* colors = nullptr, int maxX = 0, int maxY = 0) {
#if defined(LINEAR) || defined (MULTIRENDER)
    HWND myconsole = GetConsoleWindow();
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
    for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // left line
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
    int cellY = max(sector.startY - 1, 0);

    for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // top line
        if (cellX >= 0 && cellX < maxX) {
            colors[cellY * maxX + cellX] = COLOR_BORDER;
        }
    }

    cellY = min(sector.endY + 1, maxY - BORDER_SIZE - 1);
    for (cellX = sector.startX - 1; cellX <= sector.endX + 1; cellX++) { // bottom line
        if (cellX >= 0 && cellX < maxX) {
            colors[cellY * maxX + cellX] = COLOR_BORDER;
        }
    }

    cellX = max(sector.startX - 1, 0);
    for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // left line
        if (cellY >= 0 && cellY < maxY) {
            colors[cellY * maxX + cellX] = COLOR_BORDER;
        }
    }

    cellX = min(sector.endX + 1, maxX - BORDER_SIZE - 1);
    for (cellY = sector.startY - 1; cellY <= sector.endY + 1; cellY++) { // right line
        if (cellY >= 0 && cellY < maxY) {
            colors[cellY * maxX + cellX] = COLOR_BORDER;
        }
    }
#endif // BITMAP

}

// adds all outlines to the display
//void DrawSectorOutlines(vector<sector> sectors) {
//    HWND myconsole = GetConsoleWindow();
//    HDC mydc = GetDC(myconsole);
//
//    while (!sectors.empty()) {
//        DrawSectorOutline(sectors.back());
//        sectors.pop_back();
//    }
//
//    ReleaseDC(myconsole, mydc);
//}

// deletes the given outline from the display
void ClearSectorOutline(sector& sector) {
    HWND myconsole = GetConsoleWindow();
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

// clears all outlines from the display
void ClearSectorOutlines(vector<sector>& sectors) {
    HWND myconsole = GetConsoleWindow();
    HDC mydc = GetDC(myconsole);

    while (!sectors.empty()) {
        ClearSectorOutline(sectors.back());
        sectors.pop_back();
    }

    ReleaseDC(myconsole, mydc);
}

// returns a true if the cell lives or dies base n neibor count
bool CellStatus(bool** old_state, int cellX, int cellY, int maxX, int maxY) {
    int neighbors = 0;
    bool status = DEAD;

    // counting neighbors with bound limits
    if (old_state[cellX - 1][cellY] == ALIVE)
        neighbors++;
    if (old_state[cellX][cellY - 1] == ALIVE)
        neighbors++;
    if (old_state[cellX - 1][cellY - 1] == ALIVE)
        neighbors++;
    if (old_state[cellX + 1][cellY] == ALIVE)
        neighbors++;
    if (old_state[cellX][cellY + 1] == ALIVE)
        neighbors++;
    if (old_state[cellX + 1][cellY + 1] == ALIVE)
        neighbors++;
    if (old_state[cellX + 1][cellY - 1] == ALIVE)
        neighbors++;
    if (old_state[cellX - 1][cellY + 1] == ALIVE)
        neighbors++;


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

    return status; // initialized to false
}

// updates the neighbors of the given cell
void UpdateNeighbors(bool** old_state, bool** current_state, bool** old_copy, int maxX, int maxY, int cellX, int cellY, point* neighbors, bool* filled) {
    old_copy[cellX][cellY] = DEAD; // marking itself as dead
    int currX;
    int currY;
    FindAllNeighbors(old_copy, maxX, maxY, cellX, cellY, neighbors, filled);

    for (int i = 0; i < 8; i++) {
        currX = neighbors[i].cellX;
        currY = neighbors[i].cellY;
        if (filled[i]) {
            old_copy[currX][currY] = DEAD;
            current_state[currX][currY] = CellStatus(old_state, currX, currY, maxX, maxY);
        }
    }
}

// hardcoded border manager
void BorderSetter(bool** current_state, int maxX, int maxY) {
#ifdef LOOPING
    // handling special cases
    // bottom right to top left
    current_state[BORDER_SIZE - 1][BORDER_SIZE - 1] = current_state[maxX - BORDER_SIZE - 1][maxY - BORDER_SIZE - 1];
    // top left to bottm right
    current_state[maxX - BORDER_SIZE][maxY - BORDER_SIZE] = current_state[BORDER_SIZE][BORDER_SIZE];
    // bottom left to top right
    current_state[maxX - BORDER_SIZE][BORDER_SIZE - 1] = current_state[BORDER_SIZE][maxY - BORDER_SIZE - 1];
    // top right to bottom left 
    current_state[BORDER_SIZE - 1][maxY - BORDER_SIZE] = current_state[maxX - BORDER_SIZE - 1][BORDER_SIZE];

    // copy top and bottom edges to the border
    for (int cellX = BORDER_SIZE; cellX < maxX; cellX++) {
        current_state[cellX][BORDER_SIZE - 1] = current_state[cellX][maxY - BORDER_SIZE - 1];
        current_state[cellX][maxY - BORDER_SIZE] = current_state[cellX][BORDER_SIZE];
    }
    // copy left and right edges to the border
    for (int cellY = BORDER_SIZE - 1; cellY < maxY; cellY++) {
        current_state[BORDER_SIZE - 1][cellY] = current_state[maxX - BORDER_SIZE - 1][cellY];
        current_state[maxX - BORDER_SIZE][cellY] = current_state[BORDER_SIZE][cellY];
    }
#else
    // handling special cases
    // bottom right to top left
    current_state[BORDER_SIZE - 1][BORDER_SIZE - 1] = false;
    // top left to bottm right
    current_state[maxX - BORDER_SIZE][maxY - BORDER_SIZE] = false;
    // bottom left to top right
    current_state[maxX - BORDER_SIZE][BORDER_SIZE - 1] = false;
    // top right to bottom left 
    current_state[BORDER_SIZE - 1][maxY - BORDER_SIZE] = false;

    // false fill top and bottom borders
    for (int cellX = BORDER_SIZE; cellX < maxX; cellX++) {
        current_state[cellX][BORDER_SIZE - 1] = false;
        current_state[cellX][maxY - BORDER_SIZE] = false;
    }
    // false fill left and right borders
    for (int cellY = BORDER_SIZE - 1; cellY < maxY; cellY++) {
        current_state[BORDER_SIZE - 1][cellY] = false;
        current_state[maxX - BORDER_SIZE][cellY] = false;
    }
#endif // LOOPING
}

// updates the world to the next iteration
void UpdateStateMatrix(bool** old_state, bool** current_state, int maxX, int maxY, point* neighbors, bool* filled) {
    bool** old_copy = CopyMatrix(old_state, maxX, maxY);

    // force update left and right edges
    for (int cellX = BORDER_SIZE; cellX < maxX - BORDER_SIZE; cellX++) {
        current_state[cellX][maxY - BORDER_SIZE - 1] = CellStatus(old_state, cellX, maxY - BORDER_SIZE - 1, maxX, maxY);;
        current_state[cellX][BORDER_SIZE] = CellStatus(old_state, cellX, BORDER_SIZE, maxX, maxY);;
    }
    // force update left and right edges
    for (int cellY = BORDER_SIZE - 1; cellY < maxY - BORDER_SIZE; cellY++) {
        current_state[maxX - BORDER_SIZE - 1][cellY] = CellStatus(old_state, maxX - BORDER_SIZE - 1, cellY, maxX, maxY);;
        current_state[BORDER_SIZE][cellY] = CellStatus(old_state, BORDER_SIZE, cellY, maxX, maxY);;
    }


    for (int cellX = BORDER_SIZE; cellX < maxX - BORDER_SIZE; cellX++) {
        for (int cellY = BORDER_SIZE; cellY < maxY - BORDER_SIZE; cellY++) {
            if (old_copy[cellX][cellY] == ALIVE) {
                current_state[cellX][cellY] = CellStatus(old_state, cellX, cellY, maxX, maxY);
                UpdateNeighbors(old_state, current_state, old_copy, maxX, maxY, cellX, cellY, neighbors, filled);
            }
        }
    }

    BorderSetter(current_state, maxX, maxY);

    DeleteMatrix<bool>(old_copy);
    delete[] neighbors;
    delete[] filled;
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

// fill the first generation according to selecte ruels
void Initialize(bool** current_state, int maxX, int maxY) {
#ifdef RANDOM
    // randomize the first generation
    srand(time(0));
    for (int cellX = BORDER_SIZE; cellX < maxX - BORDER_SIZE; cellX++) {
        for (int cellY = BORDER_SIZE; cellY < maxY - BORDER_SIZE; cellY++) {
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

    for (offsetX = BORDER_SIZE; offsetX + sizeX <= maxX - BORDER_SIZE; offsetX += sizeX) {
        for (offsetY = BORDER_SIZE; offsetY + sizeY <= maxY - BORDER_SIZE; offsetY += sizeY) {

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

    BorderSetter(current_state, maxX, maxY);
}

// handle responce options
bool HandleResponse(char response) {
    bool result = false;
    if (response == 'y' or response == 'Y') {
        result = true;
    }

    return result;
}

// handles search and creation of outlines all the way ot the display
void OutlineCaller(bool** current_state, int maxX, int maxY, link* head, COLORREF* colors, point* neighbors, bool* filled) {
    // get sll the cells on screen
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
    int    cluster_size;
    sector outline;

    int pass_size = 0;

#ifdef SURVIVING
    pass_size = MIN_SURVIVAL;
#endif // SURVIVING

#ifdef NOTABLE
    pass_size = OVERPOPULATION;
#endif // NOTABLE

#ifdef LARGE
    pass_size = OVERPOPULATION * 2;
#endif // LARGE

    // add outlines one by one
    for (int cellX = 0; cellX < maxX; cellX++) {
        for (int cellY = 0; cellY < maxY; cellY++) {
            if (state_copy[cellX][cellY] == ALIVE) {
                FindCluster(state_copy, maxX, maxY, cellX, cellY, head, cluster_size, outline, neighbors, filled);
                if (cluster_size > pass_size) { // clusters of less than the constant CANNOT survive, no matter the arrangment
                    DrawSectorOutline(outline, colors, maxX, maxY);
                }
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
    DeleteMatrix<bool>(state_copy);
}

int main() {
    // sets makes the window fullscreen
    SetConsoleDisplayMode(GetStdHandle(STD_OUTPUT_HANDLE), CONSOLE_FULLSCREEN_MODE, 0);
    ShowScrollBar(GetConsoleWindow(), SB_VERT, 0);
    ShowConsoleCursor(false);
    Sleep(1500); // idea: is this delay pointelss wiht bitmap rendering?

    // get screen rezsolution
    int maxX = 0;
    int maxY = 0;

#ifdef NATIVE
    GetDesktopResolution(maxX, maxY);
#endif // NATIVE

#ifdef FHD
    maxX = 1920; // adding borders
    maxY = 1080;
#endif // FHD

#ifdef TEXTPRINT
    //maxX /= 40; // adapt to a managable size
    //maxY /= 40;
    maxX = 6;
    maxY = 3;
#endif // TEXTPRINT

    maxX += BORDER_SIZE * 2;
    maxY += BORDER_SIZE * 2;

    bool** current_state = CreateMatrix<bool>(maxX, maxY);
    bool** old_state = CreateMatrix<bool>(maxX, maxY); // purly to stop the 'uninitialised used'
    bool initial;
    int gen = 0;
    point* neighbors = new point[8];
    bool* filled = new bool[8];

#ifdef BITMAP
    COLORREF* colors = (COLORREF*)calloc(maxX * maxY, sizeof(COLORREF));
#else
    COLORREF* colors = nullptr;
#endif // BITMAP

#ifndef NOOUTLINES
    link* head = nullptr;
#endif // !NOOUTLINES


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
#ifndef NOOUTLINES
            OutlineCaller(current_state, maxX, maxY, head, colors, neighbors, filled);
#else
            UpdateScreen(current_state, maxX, maxY, old_state, true, colors);
#endif // !NOOUTLINES

            while (initial || !EndOfCycle(old_state, current_state, maxX, maxY) && gen < 55) {

#if defined(PATIENT) || defined(TEXTPRINT)
                Sleep(1500);
#endif // defined(PATIENT) || defined(TEXTPRINT)
#ifdef SCENIC
                Sleep(250);
#endif // SCENIC

                gen++;
                initial = false;
                //ClearSectorOutlines(outlines);

                DeleteMatrix<bool>(old_state); // idea: overwrite instead of deleting
                old_state = CopyMatrix(current_state, maxX, maxY);
                UpdateStateMatrix(old_state, current_state, maxX, maxY, neighbors, filled);

#ifndef NOOUTLINES
                OutlineCaller(current_state, maxX, maxY, head, colors, neighbors, filled); // crash here, queue add, not first outline call
#else
                UpdateScreen(current_state, maxX, maxY, old_state, false, colors);
#endif // !NOOUTLINES
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

#ifdef BITMAP
    free(colors);
#endif // BITMAP


#ifndef NOOUTLINES
    DeleteChain(head);
#endif // !NOOUTLINES

    return 0;
}

// refactoring help:
// lover camel case [a-z]+[A-Z0-9][a-z0-9]+[A-Za-z0-9]*
// upper camel case [A-Z][a-z0-9]*[A-Z0-9][a-z0-9]+[A-Za-z0-9]*
// pointer parameters [a-zA-Z>]&
