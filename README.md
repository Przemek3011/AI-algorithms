# AI Algorithms

This repository contains two C++ projects related to artificial intelligence and search/game-playing algorithms:

- **A* solver for the 15-puzzle**
- **Extended Tic-Tac-Toe AI client** based on **minimax with alpha-beta pruning**

## Contents

- [Project 1: A* 15-puzzle solver](#project-1-a-15-puzzle-solver)
- [Project 2: Extended Tic-Tac-Toe AI client](#project-2-extended-tic-tac-toe-ai-client)


---

## Project 1: A* 15-puzzle solver

File: `A_star_algorithm.cpp`

This program solves the classic **15-puzzle** using the **A\*** search algorithm with different heuristics.

### Features

- Solves a 4x4 sliding puzzle
- Uses **A\*** search
- Supports multiple heuristics:
  - **Manhattan distance**
  - **Manhattan distance + linear conflict**
  - **Inversion distance**
  - **max(Manhattan, Inversion distance)**
- Detects whether a puzzle is solvable
- Can:
  - solve a puzzle provided from command line
  - generate a random solvable puzzle
  - generate a randomized task with goal-like structure

### Implemented heuristics

- `--md` - Manhattan distance
- `--mdlc` - Manhattan distance with linear conflict
- `--id` - inversion distance
- `--max` - maximum of Manhattan distance and inversion distance

### Example usage

Compile:

```bash
g++ -std=c++17 -O2 A_star_algorithm.cpp -o a_star
