#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <testkit.h>
#include "labyrinth.h"

#define VERSION_INFO "Labyrinth Game"


int main(int argc, char *argv[]) {

    const char *map_file = NULL;
    char playerId = -1;
    const char *move_dir = NULL;
    bool show_version = false;
    int arg_cnt = 0;

    // argv[0] is Program's name
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            show_version = true;
        } else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--map") == 0) && i + 1 < argc) {
            map_file = argv[++i];
            arg_cnt++;
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--player") == 0) && i + 1 < argc) {
            playerId = argv[++i][0];
            arg_cnt++;
        } else if ((strcmp(argv[i], "--move") == 0) && i + 1 < argc) {
            move_dir = argv[++i];
            arg_cnt++;
        }
    }

    // 参数解析完之后处理 --version
    if (show_version) {
        if (argc == 2) { // 只有 ./labyrinth --version
            printf("%s\n", VERSION_INFO);
            return 0;
        } else {
            fprintf(stderr, "--version cannot be combined with other options\n");
            return 1;
        }
    }


    if (!map_file || !isValidPlayer(playerId)) {
        fprintf(stderr, "Missing or invalid parameters\n");
        return 1;
    }

    Labyrinth lab;
    if (!loadMap(&lab, map_file)) {
        fprintf(stderr, "Failed to load map\n");
        return 1;
    }

    if (move_dir) {
        if (!movePlayer(&lab, playerId, move_dir)) {
            fprintf(stderr, "Invalid move\n");
            return 1;
        }

        if (!saveMap(&lab, map_file)) {
            fprintf(stderr, "Failed to save updated map\n");
            return 1;
        }
    }

    for (int i = 0; i < lab.rows; i++) {
        for (int j = 0; j < lab.cols; j++) {
            putchar(lab.map[i][j]);
        }
        putchar('\n');
    }    
   
    return 0;
}

void printUsage() {
    printf("Usage:\n");
    printf("  labyrinth --map map.txt --player id\n");
    printf("  labyrinth -m map.txt -p id\n");
    printf("  labyrinth --map map.txt --player id --move direction\n");
    printf("  labyrinth --version\n");
}

bool isValidPlayer(char playerId) {
    if (playerId >= '0' && playerId <= '9') {
        return true;
    }
    return false;
}

bool loadMap(Labyrinth *labyrinth, const char *filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("No such file ");
        return false;
    }
    
    char line[MAX_ROWS + 2]; // max 102 byte
    int row = 0;
    int expected_cols = -1;

    while(fgets(line, sizeof(line), fp)) {
        // delete '\n'
        line[strcspn(line, "\r\n")] = '\0';

        int len = strlen(line);
        if (len == 0) continue;

        if (len > MAX_COLS) {
            fprintf(stderr," map to big" );
            fclose(fp);
            return false;
        }

        if (expected_cols == -1) {
            expected_cols = len;
        } else if (len != expected_cols) {
            fprintf(stderr, "col is not equal row");
            fclose(fp);
            return false;
        }

        if (row >= MAX_ROWS) {
            fprintf(stderr, "地图过大：行数超过最大限制 %d\n", MAX_ROWS);
            fclose(fp);
            return false;
        }

        // 检查字符合法性
        for (int i = 0; i < len; i++) {
            char c = line[i];
            if (!(c == '#' || c == '.' || (c >= '0' && c <= '9'))) {
                fprintf(stderr, "Invalid character in map: %c\n", c);
                fclose(fp);
                return false;
            }
            labyrinth->map[row][i] = c;
        }
        row++;
    }

    fclose(fp);
    if (row == 0 && expected_cols == -1) {
        fprintf(stderr, "EMPTY MAP\n");
        return false;
    }

    labyrinth->cols = expected_cols;
    labyrinth->rows = row;

    if (!isConnected(labyrinth)) {
        fprintf(stderr, "MAP IS NOT CONNECTED");
        return false;
    }

    return true;
}


Position findPlayer(Labyrinth *labyrinth, char playerId) {
    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            if (labyrinth->map[i][j] == playerId) {
                Position p = {i, j};
                return p;
            }
        }
    }   
    Position pos = {-1, -1};
    return pos;
}

Position findFirstEmptySpace(Labyrinth *labyrinth) {
    
    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            if (isEmptySpace(labyrinth, i , j)) {
                Position pos = {i,j};
                return pos;
            }
        }
    }   
    Position pos = {-1, -1};
    return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
    
    char c = labyrinth->map[row][col];
    if (c == '.' || (c >= '0' && c <= '9')) {
        return true;
    }
    return false;
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
    
    if (direction == NULL) return false;

    Position pos = findPlayer(labyrinth, playerId);

    // ✅ 玩家不存在，插入到第一个空地
    if (pos.row == -1) {
        pos = findFirstEmptySpace(labyrinth);
        if (pos.row == -1) return false;  // 没有空地了
        labyrinth->map[pos.row][pos.col] = playerId;
        // 插入完成后，继续从这里开始移动
    }

    int drow = 0, dcol = 0;

    switch (direction[0]) {
        case 'u': drow = -1; break;        
        case 'd': drow = 1; break;
        case 'l': dcol = -1; break;
        case 'r': dcol = 1; break;

        default: return false;
    }

    int new_row = pos.row + drow;
    int new_col = pos.col + dcol;

    if (new_row < 0 || new_row >= labyrinth->rows || new_col < 0 || new_col >= labyrinth->cols) return false;

    if(!isEmptySpace(labyrinth, new_row, new_col)) return false;

    labyrinth->map[new_row][new_col] = playerId;
    labyrinth->map[pos.row][pos.col] = '.';

    return true;
}

bool saveMap(Labyrinth *labyrinth, const char *filename) {
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to open file to writing");
        return false;
    }

    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            fputc(labyrinth->map[i][j], fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);

    return true;
}

// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col, bool visited[MAX_ROWS][MAX_COLS]) {
    if (row < 0 || row > labyrinth->rows || col < 0 || col > labyrinth->cols) return;
    if (visited[row][col]) return;
    if (!isEmptySpace(labyrinth, row, col)) return;  // not a empty

    visited[row][col] = true;

    dfs(labyrinth, row - 1, col, visited);
    dfs(labyrinth, row + 1, col, visited);
    dfs(labyrinth, row, col - 1, visited);
    dfs(labyrinth, row, col + 1, visited);

}

bool isConnected(Labyrinth *labyrinth) {

    bool visited[MAX_ROWS][MAX_COLS] = {false};

    Position start = findFirstEmptySpace(labyrinth);
    if (start.col == -1) return false;
    
    dfs(labyrinth, start.row, start.col, visited);

    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            if (isEmptySpace(labyrinth, i, j) && !visited[i][j]) {
                return false; // 存在未访问的空地，说明不连通
            }
        }
    }
    return true;
}
