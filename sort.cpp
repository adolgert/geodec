#include <iostream>
#include <algorithm>
#include <vector>


using namespace std;

struct comp_track
{
    bool &even;
    comp_track(bool& parity) : even(parity) {}
    bool operator()(int a, int b) {
        bool res = (a<b);
        if (res) {
            even=!even;
            cout << "lt" << even << " ";
        } else {
            cout << "gt" << even << " ";
        }
        return res;
    }
};



template<class T>
void parity(T begin, T end)
{
    bool parity;
    comp_track track_sort(parity);
    sort( begin, end, track_sort);

    cout << " sort finds " << parity << endl;
}



void lparity(int l[], size_t cnt)
{
    vector<int> jumble(l, l+cnt);
    parity(jumble.begin(), jumble.end());
}



int main(int argc, char* argv[])
{
    int no_simple[] = { 0, 1, 2, 3, 4 };
    lparity(no_simple,2);
    lparity(no_simple,3);
    int one[] = { 1, 0, 2 };
    lparity(one,2);
    lparity(one,3);
    int todd[] = { 0, 2, 1 };
    lparity(todd,3);
    int myints[] = { 7, 9, 8, 6 };
    lparity(myints,4);

    return 0;
}
