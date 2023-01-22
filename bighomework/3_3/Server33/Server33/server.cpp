#include<iostream>
using namespace std;
int main()
{
    double total = 1100000.0;
    int total_month = 30*12;
    double ll = 4.41 / 100.0;
    double a = 5520;
    double sum = 0;
    int month=0;

    while (total > 0)
    {
        cout << month++ << endl;
        total = total + total * ll / 12 - a;
        sum += a;
        //month++;
    }
    cout << sum << endl;
    cout << month << endl;

}