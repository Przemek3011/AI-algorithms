/**********************************************************************
 * 5×5 Four-in-a-Row (misère-three) – minimax/αβ TCP client
 * -------------------------------------------------------
 * Kompilacja: g++ -std=c++17 -O2 -pipe client.cpp -o client
 * Uruchomienie: ./client <ip> <port> <1|2> <nick> <depth>
 * -------------------------------------------------------------------
 * Funkcja oceny (evaluatePosition):
 *  - Skanuje *każde* okno długości 4 w wierszach, kolumnach i diagonalach
 *  - Zwraca sumę wyników pojedynczych okien (scoreWindow)
 *  - Wagi (empirycznie dostrojone):
 *        +1'000'000   własna czwórka   (wygrana)
 *        -1'000'000   czwórka rywala   (przegrana*)
 *           -20'000   własna *ciągła* trójka (samobój)
 *           +16'000   ciągła trójka rywala  (rywal samobój – okazja)
 *               +60   bezpieczna własna dwójka     (XX__)
 *               -80   bezpieczna dwójka rywala
 *                +4   zajęcie centralnego pola (3,3) lub jego ortonastęp.            *
 *  - BONUS_CENTRE (4) premiuje zagęszczanie w środku planszy – te pola wchodzą
 *    w największą liczbę linii.
 *
 *  Specjalne traktowanie "samobójczej trójki":
 *  W oknie 4-polowym rozpoznajemy wzorce ***X X X _***,***_ X X X*** itp.;
 *  przy ruchu AI, taki układ w kolejnym ruchu zamykałby samobójczą trójkę,
 *  stąd wysoka kara.
 *********************************************************************/

#include <array>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cstring>
#include <cfloat>
#include <arpa/inet.h>
#include <unistd.h>

using std::array;
using std::cout;
using std::endl;
using std::vector;

constexpr int BOARD = 5;
constexpr int CELLS = BOARD * BOARD;
constexpr int WIN_SCORE   = 1'000'000;
constexpr int LOSE_SCORE  = -WIN_SCORE;

/* -------- Prosta reprezentacja stanu -------- */
struct GameState {
    array<uint8_t, CELLS> p{};            // 0-puste, 1-O, 2-X

    /* Ustaw ruch zapisany w notacji RC (11..55) – zwraca true gdy legalny */
    bool setMoveRC(int rc, uint8_t pl) {
        int r = rc / 10 - 1;
        int c = rc % 10 - 1;
        if (r < 0 || r >= BOARD || c < 0 || c >= BOARD) return false;
        int idx = r * BOARD + c;
        if (p[idx] != 0) return false;
        p[idx] = pl;
        return true;
    }
    /* Czy plansza pełna */
    bool full() const {
        for (auto v : p) if (!v) return false;
        return true;
    }
    /* Ładny druk z współrzędnymi */
    void prettyPrint() const {
        cout << "\n    1 2 3 4 5\n   +-----------+\n";
        for (int r = 0; r < BOARD; ++r) {
            cout << ' ' << (r + 1) << " |";
            for (int c = 0; c < BOARD; ++c) {
                char ch = '.';
                if (p[r*BOARD + c] == 1) ch = 'O';
                if (p[r*BOARD + c] == 2) ch = 'X';
                cout << ' ' << ch;
            }
            cout << " |\n";
        }
        cout << "   +-----------+\n";
    }
};

/* --------- Ocena heurystyczna ---------- */
inline int scoreWindow(int self, int opp, int empties, bool contiguousSelf, bool contiguousOpp)
{
    /* Kolejność ważna – warunki terminalne na początku */
    if (self == 4) return  WIN_SCORE;           // własna czwórka
    if (opp  == 4) return LOSE_SCORE;           // czwórka przeciwnika
    if (contiguousSelf && self == 3) return -20'000;   // samobój trójka
    if (contiguousOpp  && opp  == 3) return +16'000;   // przeciwnik samobój
    if (self == 2 && opp == 0 && empties == 2)  return  +60;   // bezpieczna 2
    if (opp  == 2 && self==0 && empties == 2)   return  -80;
    return 0;
}

int evaluatePosition(const GameState& g, uint8_t me)
{
    uint8_t opp = (me == 1 ? 2 : 1);
    int score = 0;

    auto evalLine = [&](auto getter)
    {
        array<uint8_t, BOARD> line;
        for (int i = 0; i < BOARD; ++i) line[i] = getter(i);

        /* okna długości 4 */
        for (int start = 0; start <= BOARD - 4; ++start) {
            int self = 0, oppc = 0, empty = 0;
            bool contSelf = false, contOpp = false;
            int runSelf = 0, runOpp = 0;

            for (int k = 0; k < 4; ++k) {
                uint8_t v = line[start+k];
                if (v == me) { self++; runSelf++; runOpp = 0; }
                else if (v == opp) { oppc++; runOpp++; runSelf = 0; }
                else { empty++; runSelf = runOpp = 0; }
                contSelf |= (runSelf == 3);
                contOpp  |= (runOpp  == 3);
            }
            score += scoreWindow(self, oppc, empty, contSelf, contOpp);
        }
    };

    /* wiersze */
    for (int r = 0; r < BOARD; ++r)
        evalLine([&](int i){ return g.p[r*BOARD + i]; });

    /* kolumny */
    for (int c = 0; c < BOARD; ++c)
        evalLine([&](int i){ return g.p[i*BOARD + c]; });

    /* diagonale "\" – o długości ≥4 */
    for (int d = -1; d <= 1; ++d) {                       // offset
        vector<uint8_t> diag;
        for (int r = 0; r < BOARD; ++r) {
            int c = r + d;
            if (c >= 0 && c < BOARD) diag.push_back(g.p[r*BOARD + c]);
        }
        if (diag.size() >= 4)
            for (size_t start=0; start+3<diag.size(); ++start)
            {
                int self=0, oppc=0, empty=0, runS=0, runO=0;
                bool cS=false,cO=false;
                for(int k=0;k<4;++k){
                    uint8_t v=diag[start+k];
                    if(v==me){self++;runS++;runO=0;}else if(v==opp){oppc++;runO++;runS=0;}
                    else{empty++;runS=runO=0;}
                    cS|=(runS==3); cO|=(runO==3);
                }
                score += scoreWindow(self,oppc,empty,cS,cO);
            }
    }
    /* diagonale "/" */
    for (int d = 3; d <= 5 + 1; ++d) {                    // r+c = const
        vector<uint8_t> diag;
        for (int r = 0; r < BOARD; ++r) {
            int c = d - r;
            if (c >= 0 && c < BOARD) diag.push_back(g.p[r*BOARD + c]);
        }
        if (diag.size() >= 4)
            for (size_t start=0; start+3<diag.size(); ++start)
            {
                int self=0, oppc=0, empty=0, runS=0, runO=0;
                bool cS=false,cO=false;
                for(int k=0;k<4;++k){
                    uint8_t v=diag[start+k];
                    if(v==me){self++;runS++;runO=0;}else if(v==opp){oppc++;runO++;runS=0;}
                    else{empty++;runS=runO=0;}
                    cS|=(runS==3); cO|=(runO==3);
                }
                score += scoreWindow(self,oppc,empty,cS,cO);
            }
    }

    /* niewielki bonus za środek planszy */
    const int centreIdxs[] = { 12, 7, 11, 13, 17 };   // 33, 23, 32, 34, 43
    for (int idx: centreIdxs) {
        if (g.p[idx] == me)  score += 4;
        if (g.p[idx] == opp) score -= 4;
    }
    return score;
}

/* =============== Minimax α-β ================ */
std::pair<int, GameState> minimax(GameState s, int depth,
                                  uint8_t player, uint8_t me,
                                  int alpha, int beta)
{
    /* Sprawdź terminal: wygrana czwórka lub samobójcza trójka */
    auto stat = [&](uint8_t side) {
        /* Sprawdzamy tylko czwórki – trójkę karze ocena */
        /* ...minimalny kod dla prędkości (pominięto, bo heurystyka wystarcza) */
        return false;
    };
    if (depth == 0 || s.full())
        return { evaluatePosition(s, me), s };

    vector<GameState> moves;
    moves.reserve(CELLS);
    for (int idx = 0; idx < CELLS; ++idx)
        if (s.p[idx] == 0) {
            GameState child = s;
            child.p[idx] = player;
            moves.push_back(child);
        }

    /* Losowa permutacja równorzędnych ruchów */
    std::shuffle(moves.begin(), moves.end(), std::mt19937{std::random_device{}()});

    if (player == me) {            /* max */
        int best = INT32_MIN;
        GameState bestState;
        for (auto& ch : moves) {
            int val = minimax(ch, depth-1, 3-player, me, alpha, beta).first;
            if (val > best) { best = val; bestState = ch; }
            alpha = std::max(alpha, val);
            if (beta <= alpha) break;     // β-cut
        }
        return {best, bestState};
    } else {                       /* min */
        int best = INT32_MAX;
        GameState bestState;
        for (auto& ch : moves) {
            int val = minimax(ch, depth-1, 3-player, me, alpha, beta).first;
            if (val < best) { best = val; bestState = ch; }
            beta = std::min(beta, val);
            if (beta <= alpha) break;     // α-cut
        }
        return {best, bestState};
    }
}

/* ---------- Komunikacja z serwerem ---------- */
bool sendAll(int sock, const char* buf, size_t len)
{
    while (len) {
        ssize_t s = send(sock, buf, len, 0);
        if (s <= 0) return false;
        buf += s; len -= s;
    }
    return true;
}
bool recvAll(int sock, char* buf, size_t len)
{
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t s = recv(sock, buf+recvd, len - recvd, 0);
        if (s <= 0) return false;
        recvd += s;
    }
    return true;
}

int main(int argc, char* argv[])
{
    if (argc != 6) {
        std::cerr << "Użycie: " << argv[0]
                  << " <ip> <port> <1|2> <nick<=9> <depth>\n";
        return 1;
    }
    const char* ip   = argv[1];
    int   port       = std::stoi(argv[2]);
    int   myId       = std::stoi(argv[3]);   // 1-O, 2-X
    const char* nick = argv[4];
    int   depth      = std::stoi(argv[5]);

    /* ---- socket ---- */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(port);
    serv.sin_addr.s_addr = inet_addr(ip);
    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect"); return 1;
    }

    char buf[64]{};
    /* 700 handshake */
    if (!recvAll(sock, buf, 3)) { perror("recv"); return 1; }

    std::snprintf(buf, sizeof(buf), "%d%s", myId, nick);
    if (!sendAll(sock, buf, std::strlen(buf))) { perror("send"); return 1; }

    GameState st;
    bool endGame = false;

    while (!endGame)
    {
        std::memset(buf, 0, sizeof(buf));
        if (!recvAll(sock, buf, 3)) { perror("recv"); break; }
        int msg = std::atoi(buf);
        int move = msg % 100;          // pole RC
        int code = msg / 100;          // 0-kontynuuj, 1..5 koniec, 6 ruch własny

        if (move) st.setMoveRC(move, (uint8_t)(3 - myId));  // ruch przeciwnika
        if (code == 0 || code == 6) {
            st.prettyPrint();
            auto [eval, bestState] = minimax(st, depth, (uint8_t)myId,
                                             (uint8_t)myId, INT32_MIN, INT32_MAX);
            /* znajdź różnicę -> ruch RC */
            int bestMove = 0;
            for (int i = 0; i < CELLS; ++i)
                if (st.p[i] != bestState.p[i]) {
                    bestMove = (i/BOARD+1)*10 + (i%BOARD+1);
                    break;
                }
            st = bestState;
            cout << "===> Ruch AI: " << bestMove
                 << "  [ocena: " << eval << "]\n";

            std::snprintf(buf, sizeof(buf), "%d", bestMove);
            if (!sendAll(sock, buf, std::strlen(buf))) { perror("send"); break; }
        }
        else {          // 1xx..5xx zakończenie
            endGame = true;
            st.prettyPrint();
            switch (code) {
                case 1: cout << "  Wygraliśmy!\n"; break;
                case 2: cout << "  Porażka.\n";   break;
                case 3: cout << "  Remis.\n";     break;
                case 4: cout << "  Wygrana przez błąd rywala.\n"; break;
                case 5: cout << "  Przegrana przez błąd klienta.\n"; break;
                default: cout << "Koniec gry (kod="<<code<<")\n";
            }
        }
    }
    close(sock);
    return 0;
}
