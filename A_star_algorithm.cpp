#include <iostream>
#include <array>
#include <queue>
#include <map>
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <random>

using Board = std::array<uint8_t, 16>;

uint64_t encodeBoard(const Board& board) {
    uint64_t hash = 0;
    for (int i = 0; i < 16; ++i) {
        hash |= static_cast<uint64_t>(board[i]) << (i * 4);
    }
    return hash;
}

int manhattanHeuristic(const Board& board) {
    int h = 0;
    for (int i = 0; i < 16; ++i) {
        if (board[i] == 0) continue;
        int target = board[i] - 1;
        h += std::abs(i / 4 - target / 4) + std::abs(i % 4 - target % 4);
    }
    return h;
}

int linearConflictHeuristic(const Board& board) {
    int h = manhattanHeuristic(board);
    int lc = 0;
    for (int row = 0; row < 4; ++row) {
        for (int i = 0; i < 4; ++i) {
            int idx_i = row * 4 + i;
            uint8_t val_i = board[idx_i];
            if (val_i == 0 || (val_i - 1) / 4 != row) continue;
            for (int j = i + 1; j < 4; ++j) {
                int idx_j = row * 4 + j;
                uint8_t val_j = board[idx_j];
                if (val_j == 0 || (val_j - 1) / 4 != row) continue;
                if (val_i > val_j) lc += 2;
            }
        }
    }
    for (int col = 0; col < 4; ++col) {
        for (int i = 0; i < 4; ++i) {
            int idx_i = i * 4 + col;
            uint8_t val_i = board[idx_i];
            if (val_i == 0 || (val_i - 1) % 4 != col) continue;
            for (int j = i + 1; j < 4; ++j) {
                int idx_j = j * 4 + col;
                uint8_t val_j = board[idx_j];
                if (val_j == 0 || (val_j - 1) % 4 != col) continue;
                if (val_i > val_j) lc += 2;
            }
        }
    }
    return h + lc;
}

int inversionDistanceHeuristic(const Board& board) {
    int inv = 0;
    for (int i = 0; i < 16; ++i) {
        if (board[i] == 0) continue;
        for (int j = i + 1; j < 16; ++j) {
            if (board[j] == 0) continue;
            if (board[i] > board[j]) ++inv;
        }
    }

    // Każdy ruch może zmniejszyć maksymalnie 3 inwersje
    // (np. poprzez przestawienie większego kafelka nad mniejszym)
    // Dzielimy przez 3 zaokrąglając w górę (czyli ceil)
    return (inv + 2) / 3;
}

int maxMDIDHeuristic(const Board& board) {
    return std::max(manhattanHeuristic(board), inversionDistanceHeuristic(board));
}

bool isSolvable(const Board& board) {
    int inversions = 0;
    int blankRow = -1;
    for (int i = 0; i < 16; ++i) {
        if (board[i] == 0) {
            blankRow = i / 4;
            continue;
        }
        for (int j = i + 1; j < 16; ++j) {
            if (board[j] == 0) continue;
            if (board[i] > board[j]) inversions++;
        }
    }
    int blankRowFromBottom = 4 - blankRow;
    return (inversions + blankRowFromBottom) % 2 == 1;
}

Board generateRandomBoard() {
    Board board;
    std::iota(board.begin(), board.end(), 0);
    std::random_device rd;
    std::mt19937 g(rd());
    do {
        std::shuffle(board.begin(), board.end(), g);
    } while (!isSolvable(board));
    return board;
}

Board generateRandomZad() {
    Board board;
    std::iota(board.begin(), board.end() - 1, 1);  // Fill 1-15
    board[15] = 0;  // Ensure blank is at the end
    std::random_device rd;
    std::mt19937 g(rd());

    do {
        std::shuffle(board.begin(), board.end() - 1, g); // shuffle only 0..14
    } while (!isSolvable(board));
    
    return board;
}

void solve(const Board& start, const Board& goal, int (*heuristic)(const Board&)) {
    struct Node {
        uint64_t id;
        int g, h;
        uint64_t parent; 
        int f() const { return g + h; }
        bool operator>(const Node& other) const {
            return f() == other.f() ? h > other.h : f() > other.f();
        }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    std::map<uint64_t, std::pair<int, uint64_t>> visited;

    uint64_t start_id = encodeBoard(start);
    open.push({start_id, 0, heuristic(start), start_id});
    visited[start_id] = {0, start_id};

    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    size_t states_explored = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (!open.empty()) {
        Node current = open.top(); open.pop();

        Board board;
        for (int i = 0; i < 16; ++i)
            board[i] = (current.id >> (4 * i)) & 0xF;

        if (board == goal) {
            auto end_time = std::chrono::high_resolution_clock::now();
            double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Odtwarzanie ścieżki
            std::vector<Board> path;
            uint64_t curr_id = current.id;
            while (true) {
                Board b;
                for (int i = 0; i < 16; ++i)
                    b[i] = (curr_id >> (4 * i)) & 0xF;
                path.push_back(b);
                if (curr_id == start_id) break;
                curr_id = visited[curr_id].second;
            }
            std::reverse(path.begin(), path.end());

            // Wypisywanie ruchów
            std::cout << "Moves to solve:\n";
            for (size_t i = 1; i < path.size(); ++i) {
                int zero_prev = std::find(path[i - 1].begin(), path[i - 1].end(), 0) - path[i - 1].begin();
                int zero_curr = std::find(path[i].begin(), path[i].end(), 0) - path[i].begin();
                std::cout << (int)path[i - 1][zero_curr] << " ";
            }
            std::cout << "\n";

            std::cout << current.g << ";" << states_explored << ";" << duration_ms << std::endl;
            return;
        }

        ++states_explored;
        int zero = std::find(board.begin(), board.end(), 0) - board.begin();
        int r = zero / 4, c = zero % 4;

        for (int d = 0; d < 4; ++d) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr < 0 || nr >= 4 || nc < 0 || nc >= 4) continue;
            int new_pos = nr * 4 + nc;

            Board neighbor = board;
            std::swap(neighbor[zero], neighbor[new_pos]);
            uint64_t neighbor_id = encodeBoard(neighbor);
            int new_g = current.g + 1;

            auto it = visited.find(neighbor_id);
            if (it == visited.end() || new_g < it->second.first) {
                visited[neighbor_id] = {new_g, current.id};
                int h = heuristic(neighbor);
                open.push({neighbor_id, new_g, h, current.id});
            }
        }
    }

    return;
}

void printBoard(const Board& board) {
    std::cout << "Initial board:\n";
    for (int i = 0; i < 16; ++i) {
        if (i % 4 == 0) std::cout << std::endl;
        std::cout << (int)board[i] << "\t";
    }
    std::cout << "\n\n";
}


int main(int argc, char* argv[]) {
    Board board;
    int (*heuristic)(const Board&) = manhattanHeuristic;

    if (argc >= 2 && std::string(argv[2]) == "--random") {
    board = generateRandomBoard();
    } else if (argc >= 2 && std::string(argv[2]) == "--randomzad") {
    board = generateRandomZad();
    } else if (argc >= 4) {
        for (int i = 0; i < 16; ++i) {
            board[i] = std::stoi(argv[i + 2]);
        }
    } else {
        std::cerr << "Usage:\n" << argv[0] << " --md|--mdlc|--id|--max [tiles] OR --random\n";
        return 1;
    }


    if (!isSolvable(board)) {
        std::cout << "The puzzle is not solvable.\n";
        return 1;
    }

    if (std::string(argv[1]) == "--mdlc") heuristic = linearConflictHeuristic;
    else if (std::string(argv[1]) == "--id") heuristic = inversionDistanceHeuristic;
    else if (std::string(argv[1]) == "--max") heuristic = maxMDIDHeuristic;
    else heuristic = manhattanHeuristic;

    Board goal;
    std::iota(goal.begin(), goal.end() - 1, 1);
    goal[15] = 0;
    printBoard(board);

    solve(board, goal, heuristic);
    return 0;
}
