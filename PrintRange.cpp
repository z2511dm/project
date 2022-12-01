#include <iostream>
#include <set>
#include <vector>
#include <algorithm>

using namespace std;

template <typename It>
void PrintRange(It range_begin, It range_end)
{
    for (; range_begin != range_end; ++range_begin)
    {
        cout << *range_begin << " ";
    }
    cout << endl;
}

template <typename T, typename T2>
void FindAndPrint(T cont, T2 elem)
{
    auto result = find(cont.begin(), cont.end(), elem);
    PrintRange(cont.begin(), result);
    PrintRange(result, cont.end());
}

int main()
{
    set<int> test = { 1, 1, 1, 2, 3, 4, 5, 5 };
    cout << "Test1"s << endl;
    FindAndPrint(test, 3);
    cout << "Test2"s << endl;
    FindAndPrint(test, 0); // элемента 0 нет в контейнере
    cout << "End of tests"s << endl;
}