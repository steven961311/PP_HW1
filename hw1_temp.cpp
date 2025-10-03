//pass until 22.txt
//simple dead static need to deal with fragile

#include <bits/stdc++.h>
#include <omp.h>
using namespace std;

struct Oops : public runtime_error {
    using runtime_error::runtime_error;
};

struct Reachpath {
    bitset<4> reach;
    string path;
};

enum Cell_T : int{
    EMPTY = 0,
    WALL = 1,
    GOAL = 2,
    FRAGILE = 3,
    DEAD = 4
};
enum bneighbor_T : uint8_t{
    U = 0,
    L = 1,
    D = 2,
    R = 3
};

struct Player {
    int y;
    int x;
};

struct box_T {
    int y;
    int x;
    bool finish;
    bitset<4> reach;

    bool operator==(const box_T& o) const noexcept {
        return x == o.x && y == o.y;
    }

    bool operator<(const box_T& o) const noexcept {
        if (y != o.y) return y < o.y;         // 先比 y
        return x < o.x;                       // 再比 x
    }
};

struct State {
    vector<box_T> boxes;
    inline void norm() { sort(boxes.begin(), boxes.end()); }

    inline bool has_box(int y,int x) const {
        return binary_search(boxes.begin(), boxes.end(), box_T{y,x});
    }

    inline int index_of_box(int y, int x) const {
        auto it = lower_bound(boxes.begin(), boxes.end(), box_T{y,x});
        if (it != boxes.end() && it->y==y && it->x==x) return int(it - boxes.begin());
        return -1;
    }

    bool operator==(const State& o) const noexcept {
        return boxes==o.boxes;
    }

};

struct Node {
    State st;
    string path;
};

struct BoardInfo {
    int H, W;
    vector<string> map;
    vector<vector<uint8_t>> cells;
};

static const unordered_map<char, pair<int,int>> MOVES = {
    {'W', {-1, 0}},
    {'A', { 0,-1}},
    {'S', { 1, 0}},
    {'D', { 0, 1}}
};

static BoardInfo B;
static Player player;

// 以「反向拉箱」預計算：從所有目標出發，找出箱子能被「拉」到的所有格子。
// 任何非牆且沒被走到的格子 => 簡單死鎖格。
static void compute_simple_dead() {
    vector<vector<uint8_t>> reach(B.H, vector<uint8_t>(B.W, 0));
    deque<pair<int,int>> q;

    // 目標格作為種子
    for (int y = 0; y < B.H; ++y)
        for (int x = 0; x < B.W; ++x)
            if (B.cells[y][x] == GOAL) { reach[y][x] = 1; q.emplace_back(y, x); }

    auto inb = [&](int y, int x){
        return (0 <= y && y < B.H && 0 <= x && x < B.W);
    };

    const int D[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
    while (!q.empty()) {
        auto [vy, vx] = q.front(); q.pop_front();

        // 從 (vy,vx) 這個「箱子當前位置」，考慮它是從哪個格子被拉來的：
        // 若箱子在 (vy,vx)，要「拉」向方向 (dy,dx)，則它的前一格是 (uy,ux) = (vy - dy, vx - dx)，
        // 需要玩家站在 (wy,wx) = (vy + dy, vx + dx)。空盤上只要這兩格不是牆就可。
        for (auto [dy,dx] : D) {
            int uy = vy - dy, ux = vx - dx; // 箱子「來自」的位置
            int wy = uy - dy, wx = ux - dx; // 玩家站位（拉時站在箱子相對的另一側）

            if (!inb(uy,ux) || !inb(wy,wx)) continue;
            if (B.cells[uy][ux] == WALL || B.cells[wy][wx] == WALL) continue; // 任一是牆就拉不動
            if (!reach[uy][ux]) {
                reach[uy][ux] = 1;
                q.emplace_back(uy, ux);
            }
        }
    }

    // 沒被標到、且不是牆的，就是 simple dead square
    vector<vector<uint8_t>> dead(B.H, vector<uint8_t>(B.W, 0));
    for (int y = 0; y < B.H; ++y)
        for (int x = 0; x < B.W; ++x)
            if (B.cells[y][x] != WALL && !reach[y][x]) B.cells[y][x] = DEAD;

    return;
}

static void build_board_info(const vector<string>& m) {
    B.H = (int)m.size();
    B.W = (int)m[0].size();
    B.map = m;
    B.cells.assign(B.H, vector<uint8_t>(B.W, 0));

    for (int y=0;y<B.H;++y)
        for (int x=0;x<B.W;++x) {
            char c = m[y][x];
            if(c == '#') B.cells[y][x] = WALL;
            else if(c == '.' || c == 'O' || c == 'X') B.cells[y][x] = GOAL;
            else if(c == '@' || c == '!') B.cells[y][x] == FRAGILE;
        }

    return;
}

static bool frozen_box(const State st, int y, int x) {
    auto is_blocker = [&](int yy,int xx){
        if (yy<0||yy>=B.H||xx<0||xx>=B.W) return true;
        for(auto& i: st.boxes) {
            if(i.y == yy && i.x == xx)
                return true;
        }
        return B.cells[yy][xx] == WALL;
    };
    if (B.cells[y][x] == GOAL) return false;

    bool up = is_blocker(y-1,x), dn = is_blocker(y+1,x);
    bool lf = is_blocker(y,x-1), rt = is_blocker(y,x+1);

    if (up && dn && (lf || rt)) return true;
    if (lf && rt && (up || dn)) return true;
    return false;
}

static bool deadlock_2x2(State& st) {
    for (int y=0; y+1<B.H; ++y) {
        for (int x=0; x+1<B.W; ++x) {
            if (st.has_box(y,x)&&st.has_box(y,x+1)&&st.has_box(y+1,x)&&st.has_box(y+1,x+1)) {
                if (B.cells[y][x] != GOAL && B.cells[y][x+1] != GOAL && B.cells[y+1][x] != GOAL && B.cells[y+1][x+1] != GOAL)
                    return true;
            }
        }
    }    
    return false;
}

static inline string rstrip(const string& s) {
    size_t end = s.size();
    while (end > 0 && (s[end-1] == '\n' || s[end-1] == '\r')) --end;
    return s.substr(0, end);
}

string get_there(State& st, int y, int x, int ty, int tx) {
    vector<vector<string>> vis(B.H, vector<string>(B.W, ""));
    queue<pair<int, int>> q;
    q.push({y, x});

    auto is_block = [&](int y, int x) {
        if (y < 0 || y >= B.H || x < 0 || x >= B.W) return true;
        if (B.cells[y][x] == WALL) return true;       // 牆
        if (st.has_box(y,x)) return true;             // 箱子
        return false;
    };

    while (!q.empty()) {
        auto [cury, curx] = q.front(); q.pop();
        //cout << cury << " " << curx << vis[cury][curx];
        if (cury==ty && curx==tx) {
            break;
        }
        
        for (auto& d : MOVES) {
            
            int ny = cury + d.second.first, nx = curx + d.second.second;
            if (!is_block(ny,nx) && vis[ny][nx] == "" &&(ny != y || nx != x)) {
                //cout << "oh" << endl;
                vis[ny][nx] = vis[cury][curx] + d.first;
                q.push({ny,nx});
            }
        }
    }
    return vis[ty][tx];
}

bitset<4> reachable(State& st, int y, int x, int py, int px) {
    vector<vector<bool>> vis(B.H, vector<bool>(B.W, false));
    queue<pair<int,int>> q;
    q.push({py, px});
    vis[py][px] = true;

    auto is_block = [&](int iy, int ix) {
        if (iy < 0 || iy >= B.H || ix < 0 || ix >= B.W) return true;
        if (B.cells[iy][ix] == WALL) return true;       // 牆
        for(auto& b: st.boxes) {
            if(b.y == iy && b.x == ix)
                return true;
        }
        //if (st.has_box(iy,ix)) return true;             // 箱子
        return false;
    };

    bitset<4> reach = 0b0000;
    int first = true;
    while (!q.empty()) {
        auto [cury, curx] = q.front(); q.pop();
        /*
        if (cury==y-1 && curx==x) { //up
            out.reach.set(L);
        }
        if (cury==y+1 && curx==x) { //down
            out.reach.set(R);
        }
        if (cury==y && curx==x-1) { //left
            out.reach.set(U);
        }
        if (cury==y && curx==x+1) { //right
            out.reach.set(D);
        }*/
        
        for (auto& d : MOVES) {
            int ny = cury + d.second.first, nx = curx + d.second.second;
                
            if (!is_block(ny,nx) && !vis[ny][nx]) {
                vis[ny][nx] =true;
                q.push({ny,nx});
            }
        }
        
    }
    if (vis[y-1][x]) { //up
        reach.set(L);
    }
    if (vis[y+1][x]) { //down
        reach.set(R);
    }
    if (vis[y][x-1]) { //left
        reach.set(U);
    }
    if (vis[y][x+1]) { //right
        reach.set(D);
    }
/*
    for(int i = 0; i < B.H; i++) {
        for(int j = 0 ; j < B.W; j++) {
            if(vis[i][j] == "" ) {
                cout << i << " " << j << " ";
            }
            else {
                cout << vis[i][j] << " ";
            }
        } cout << endl;
    } 

    
    for(int i = 0; i<4;i++) {
        cout << out.path << endl;
    }
   */
   cout <<"pyx: "<< py << " " << px<<endl;
   cout << "yx " << y << " " << x<<endl;
    for(int i = 0; i < B.H; i++) {
        for(int j = 0 ; j < B.W; j++) {
            bool print_y = false;
            for(auto& b: st.boxes) {
                if(i == b.y && j == b.x) {
                    cout << "B" << " ";
                    print_y = true;
                }
            }if(print_y) continue;
            if(i == py && j == px)
                cout << "p" << " ";
            else if(vis[i][j])
                cout << "T" << " ";
            else 
                cout << (int)B.cells[i][j] << " ";
        }
        cout << endl;
    }

    return reach;
}


static State loadstate(const string& filename) {
    ifstream fin(filename);
    if (!fin) throw Oops("failed to open input file");

    vector<string> m;
    vector<int> widths;
    unordered_map<char, long long> stats;
    State st;
    player.y = -1, player.x = -1;

    string line;
    int y = 0;
    while (getline(fin, line)) {
        line = rstrip(line);
        // 記錄玩家位置（支援 'o','O','!' 其一）
        for (int x = 0; x < (int)line.size(); ++x) {
            char c = line[x];
            if (c == 'o' || c == 'O' || c == '!') {
                player.y = y; player.x = x;
            }
            if(c == 'x')
                st.boxes.push_back(box_T{y, x, false, 0b0000});
            if(c == 'X')
                st.boxes.push_back(box_T{y, x, true, 0b0000});
            stats[c]++;
        }
        widths.push_back((int)line.size());
        m.push_back(line);
        ++y;
    }
    st.norm();

    if (m.empty()) throw Oops("input file is empty");

    // 檢查非法字元
    const string allowed = "xXoO. #@!";
    unordered_set<char> allowed_set(allowed.begin(), allowed.end());
    unordered_set<char> present;
    for (auto& kv : stats) present.insert(kv.first);

    vector<char> invalid;
    for (char c : present) if (!allowed_set.count(c)) invalid.push_back(c);
    if (!invalid.empty()) {
        sort(invalid.begin(), invalid.end());
        string s = "{";
        for (size_t i=0;i<invalid.size();++i) {
            if (i) s += ", ";
            s.push_back(invalid[i]);
        }
        s += "}";
        throw Oops("input file contains invalid characters: " + s);
    }

    // 箱子與目標、玩家數量檢查
    long long boxes   = stats['x'] + stats['X'];
    long long targets = stats['.'] + stats['X'] + stats['O'];
    if (boxes != targets) {
        throw Oops("got " + to_string(boxes) + " boxes and " + to_string(targets) + " targets in input");
    }
    long long nplayers = stats['o'] + stats['O'] + stats['!'];
    if (nplayers != 1) {
        throw Oops("got " + to_string(nplayers) + " players in input");
    }

    // 寬度一致
    if (!widths.empty()) {
        int w0 = widths[0];
        for (int w : widths) if (w != w0) {
            // 列出所有寬度
            string s = "[";
            for (size_t i=0;i<widths.size();++i) {
                if (i) s += ", ";
                s += to_string(widths[i]);
            }
            s += "]";
            throw Oops("input rows having different widths: " + s);
        }
    }

    build_board_info(m);
    compute_simple_dead();

    for(auto& s: st.boxes) {
        s.reach = reachable(st, s.y, s.x, player.y, player.x);
        //cout << s.reachpath.reach  << endl;
    }

    if (player.y < 0 || player.x < 0) throw Oops("player position not found");

    return st;
}

static inline bool in_bounds(const vector<string>& m, int y, int x) {
    return y >= 0 && y < (int)m.size() && x >= 0 && x < (int)m[0].size();
}

//TODO
static vector<Node> try_move(Node& node) {
    vector<Node> new_nodes;
    int cury = player.y, curx = player.x;

    for(auto& mv: node.path) {
        cury += MOVES.at(mv).first;
        curx += MOVES.at(mv).second;
    }
    //cout<<"nodeyx: " << cury << " " << curx << endl;

    State st = node.st;
    for(auto& b: st.boxes) {
        int x = b.x, y = b.y;
        int idx = st.index_of_box(y, x);
        cout << "idx " << idx << " " << st.boxes[idx].y << " " << st.boxes[idx].x <<  endl;
        int direct = 0;
        for(auto& mv: MOVES) {
            cout << mv.first << endl;
            bool ok = false;
            State temp = st;
            int from_y = y-mv.second.first;
            int from_x = x-mv.second.second;
            //cout << st.boxes[idx].reachpath.reach << " " <<direct << endl;
            //string pre_path = reachable(st, from_y, from_x);
            //cout << temp.boxes[idx].reachpath.path[direct] << " " << mv.first << " " << temp.boxes[idx].reachpath.reach[direct] << endl;
            if(temp.boxes[idx].reach[direct]) {
                int to_y = y+mv.second.first; 
                int to_x = x+mv.second.second;
                bool has_a_box = false;
                for(auto& b: temp.boxes) {
                    if(b.y == to_y && b.x == to_x) {
                        has_a_box = true;
                    }
                }
                if(B.cells[to_y][to_x] == WALL || B.cells[to_y][to_x] == FRAGILE || B.cells[to_y][to_x] == DEAD || has_a_box) {
                    direct++;
                    continue;
                }
                else if(B.cells[to_y][to_x] == GOAL) {
                    temp.boxes[idx].finish = true;
                } else {
                    temp.boxes[idx].finish = false; 
                }
                temp.boxes[idx].y = to_y;
                temp.boxes[idx].x = to_x;
                if(deadlock_2x2(temp) || frozen_box(temp, to_y, to_x)) {
                    direct++;
                    continue;
                }
                string there_path = get_there(st, cury, curx, from_y, from_x);
                cout <<"there_path "<< there_path << endl;
                cout << "cur " << cury <<" "<< curx << endl;
                cout << "from " << from_y <<" "<< from_x << endl;
                for(auto& b: temp.boxes) {
                    b.reach = reachable(temp, b.y, b.x, y, x);

                }
                for(auto& b: temp.boxes) {
                    int idx = st.index_of_box(b.y, b.x);
                    cout << b.reach << " " << b.y << " " << b.x << endl;
                }cout << endl;
                temp.norm();
                new_nodes.push_back(Node{temp, node.path + there_path + mv.first});
            }
            direct++;
        }
    }

    /*
    for(auto& node: new_nodes) {
        cout<< "path: " <<node.path << endl;
        for(auto& box: node.st.boxes) {
            cout << box.y << " " << box.x;
        }cout << endl;
    }*/
    return new_nodes;
    /*
    const auto& m = st.m;
    int H = (int)m.size();
    int W = (int)m[0].size();
    int y = st.y, x = st.x;

    int yy = y + dy, xx = x + dx;
    int yyy = yy + dy, xxx = xx + dx;

    // 邊界保護（原 Python 假設被牆包住，這裡多一層防呆）
    if (!in_bounds(m, yy, xx)) return nullopt;

    vector<string> n = m; // 複製地圖以產生新狀態

    auto set_player_on = [&](char c) {
        if (c == ' ')       n[yy][xx] = 'o';
        else if (c == '.')  n[yy][xx] = 'O';
        else if (c == '@')  n[yy][xx] = '!';
        else return false;
        return true;
    };

    char front = m[yy][xx];

    if (front == ' ' || front == '.' || front == '@') {
        if (!set_player_on(front)) return nullopt;
    } else if (front == 'x' || front == 'X') {
        // 推箱
        if (!in_bounds(m, yyy, xxx)) return nullopt;
        char front2 = m[yyy][xxx];
        if (front2 != ' ' && front2 != '.') return nullopt;

        // 1) 靜態死點（若不是目標且在死點）
        if (!B.goal[yyy][xxx] && B.dead_static[yyy][xxx]) return nullopt;

        // 玩家踏入箱子原位
        if (front == 'x') n[yy][xx] = 'o';
        else              n[yy][xx] = 'O';

        // 箱子推到下一格
        if (front2 == ' ') n[yyy][xxx] = 'x';
        else               n[yyy][xxx] = 'X';

        // 2) 動態死鎖（掃描新盤面）
        if (deadlock_2x2(n, B)) return nullopt;

        if (frozen_box(n, yyy, xxx, B)) return nullopt;
    } else {
        // 牆或其他不可進入
        return nullopt;
    }

    // 清理玩家原地（還原底圖）
    char orig = m[y][x];
    if (orig == 'o')      n[y][x] = ' ';
    else if (orig == '!') n[y][x] = '@';
    else                  n[y][x] = '.'; // 包含 'O'

    return State{move(n), yy, xx};
    */
}

static bool is_solved(const State& st) {
    for (const auto& b : st.boxes) {
        cout<< b.y << " " << b.x << " is_solve? "<< b.finish << endl << endl;
    }
    for (const auto& b : st.boxes) {
        if(!b.finish) return false;
    }
    return true;
}

static string key_of(const State& st) {
    // 將整張圖 + 分隔符 + y,x 序列化
    // 注意：這裡沒有必要包含分隔符在每行結尾，單純接起來也可；為可讀性加 '\n'
    string k = "";
    for (auto& b : st.boxes) {
        k += to_string(b.y) + "," + to_string(b.x) + "|" + b.reach.to_string() + ";";
    }
    //cout << k << endl;
    return k;
}
//TODO
/*
static string key_boxes(const State& st) {
    vector<pair<int,int>> boxes;
    for (int y = 0; y < (int)st.m.size(); ++y)
        for (int x = 0; x < (int)st.m[0].size(); ++x) {
            char c = st.m[y][x];
            if (c == 'x' || c == 'X') boxes.emplace_back(y,x);
        }
    sort(boxes.begin(), boxes.end());
    string k; k.reserve(boxes.size()*8);
    for (auto [y,x] : boxes) {
        k += to_string(y); k += ','; k += to_string(x); k += ';';
    }
    return k;
}*/

static string solve_bfs(const string& filename) {
    State init = loadstate(filename);
    unordered_set<string> visited;              // 只做去重
    vector<Node> frontier; frontier.reserve(1024);
    frontier.push_back(Node{init, ""});         // ✅ 放入初始節點

    visited.insert(key_of(init));

    while (!frontier.empty()) {
        vector<Node> next;
        next.reserve(frontier.size() * 2);

        std::atomic<bool> found(false);
        string answer;

        #pragma omp parallel
        {
            vector<Node> local_next;
            local_next.reserve(256);
            string local_answer;
            bool local_found = false;
            //目前32最快
            #pragma omp for schedule(guided, 16384)
            for (int i = 0; i < (int)frontier.size(); ++i) {
                if (found.load(std::memory_order_relaxed)) continue;

                Node& cur = frontier[i];
                cout <<omp_get_thread_num() << " "<< key_of(cur.st) << endl;
                // ✅ 命中終局：只記錄答案，不在平行區 exit
                if (!local_found && is_solved(cur.st)) {
                    local_answer = cur.path;
                    local_found = true;
                    continue;
                }

                cout  <<cur.path<< endl;
                vector<Node> new_nodes = try_move(cur);
                for(auto& node: new_nodes) {
                    string k = key_of(node.st);
                    bool inserted = false;
                    #pragma omp critical(visited_cs)
                    {
                        if (visited.insert(k).second) inserted = true;
                        cout <<omp_get_thread_num() << " " << inserted<<" " <<k << " " <<node.path<< endl;
                    }

                    if (inserted) {
                        local_next.push_back(node);
                    }
                }

            } // omp for

            // ✅ 若本 thread 找到解，寫回全域旗標與答案
            if (local_found) {
                #pragma omp critical(ans_cs)
                {
                    if (!found.load()) {
                        found.store(true, std::memory_order_relaxed);
                        answer = std::move(local_answer);
                    }
                }
            }

            // ✅ 合併 next（簡單起見用 critical）
            #pragma omp critical(next_cs)
            {
                next.insert(next.end(),
                            std::make_move_iterator(local_next.begin()),
                            std::make_move_iterator(local_next.end()));
            }
        } // end parallel

        if (found.load()) {
            return answer; // ✅ 在層級邊界統一返回（保持 BFS 最短性）
        }

        if (next.empty()) break; // ✅ 無新節點 → 無解
        frontier.swap(next);
    }

    throw Oops("no solution");
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 2) {
        cerr << "usage: " << argv[0] << " <input_file>\n";
        return 1;
    }
    try {
        string ans = solve_bfs(argv[1]);
        cout << ans << "\n";
    } catch (const Oops& e) {
        cerr << e.what() << "\n";
        return 1;
    } catch (const exception& e) {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}