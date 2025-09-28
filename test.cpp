#include <bits/stdc++.h>
#include <omp.h>
using namespace std;

struct Oops : public runtime_error {
    using runtime_error::runtime_error;
};

struct State {
    vector<string> m; // map
    int y, x;         // player position
};

static const pair<char, pair<int,int>> DYDX[] = {
    {'W', {-1, 0}},
    {'A', { 0,-1}},
    {'S', { 1, 0}},
    {'D', { 0, 1}},
};

int main(){
    cout << DYDX[1].second.second;
}