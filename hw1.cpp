#include <bits/stdc++.h>
using namespace std;

struct Oops : public runtime_error {
    using runtime_error::runtime_error;
};

struct State {
    vector<string> m; // map
    int y, x;         // player position
};

static const unordered_map<char, pair<int,int>> DYDX = {
    {'W', {-1, 0}},
    {'A', { 0,-1}},
    {'S', { 1, 0}},
    {'D', { 0, 1}},
};

static inline string rstrip(const string& s) {
    size_t end = s.size();
    while (end > 0 && (s[end-1] == '\n' || s[end-1] == '\r')) --end;
    return s.substr(0, end);
}

static State loadstate(const string& filename) {
    ifstream fin(filename);
    if (!fin) throw Oops("failed to open input file");

    vector<string> m;
    vector<int> widths;
    unordered_map<char, long long> stats;
    int oy = -1, ox = -1;

    string line;
    int y = 0;
    while (getline(fin, line)) {
        line = rstrip(line);
        // 記錄玩家位置（支援 'o','O','!' 其一）
        for (int x = 0; x < (int)line.size(); ++x) {
            char c = line[x];
            if (c == 'o' || c == 'O' || c == '!') {
                oy = y; ox = x;
            }
            stats[c]++;
        }
        widths.push_back((int)line.size());
        m.push_back(line);
        ++y;
    }

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

    if (oy < 0 || ox < 0) throw Oops("player position not found");

    return State{m, oy, ox};
}

static inline bool in_bounds(const vector<string>& m, int y, int x) {
    return y >= 0 && y < (int)m.size() && x >= 0 && x < (int)m[0].size();
}

static optional<State> try_move(const State& st, int dy, int dx) {
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

        // 玩家踏入箱子原位
        if (front == 'x') n[yy][xx] = 'o';
        else              n[yy][xx] = 'O';

        // 箱子推到下一格
        if (front2 == ' ') n[yyy][xxx] = 'x';
        else               n[yyy][xxx] = 'X';
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
}

static bool is_solved(const vector<string>& m) {
    for (const auto& r : m)
        for (char c : r)
            if (c == 'x') return false;
    return true;
}

// 產生可用於 visited 的鍵（地圖 + 玩家座標）
static string key_of(const State& st) {
    // 將整張圖 + 分隔符 + y,x 序列化
    // 注意：這裡沒有必要包含分隔符在每行結尾，單純接起來也可；為可讀性加 '\n'
    string k;
    k.reserve(st.m.size() * (st.m[0].size() + 1) + 16);
    for (const auto& row : st.m) {
        k += row; k += '\n';
    }
    k += '|';
    k += to_string(st.y);
    k += ',';
    k += to_string(st.x);
    return k;
}

static string solve_bfs(const string& filename) {
    State init = loadstate(filename);
    unordered_map<string, string> visited; // state_key -> path
    deque<State> q;

    string k0 = key_of(init);
    visited.emplace(k0, "");
    q.push_back(init);

    while (!q.empty()) {
        State cur = move(q.front()); q.pop_front();
        if (is_solved(cur.m)) {
            return visited[key_of(cur)];
        }
        const string cur_key = key_of(cur);
        const string cur_path = visited[cur_key];

        for (auto&& kv : DYDX) {
            char move_char = kv.first;
            int dy = kv.second.first, dx = kv.second.second;

            auto nxt = try_move(cur, dy, dx);
            if (!nxt) continue;

            string nk = key_of(*nxt);
            if (visited.find(nk) == visited.end()) {
                visited.emplace(nk, cur_path + move_char);
                q.push_back(move(*nxt));
            }
        }
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