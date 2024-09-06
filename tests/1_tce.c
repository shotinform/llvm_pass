#include <stdio.h>

int foo(int a, int b, int n){
    if (n < 0)
        return 0;

    return foo(a, b, n-1);
}



int main(){
    return 0;
}