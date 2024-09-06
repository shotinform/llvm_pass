#include <stdio.h>

void foo(int a, int b, int n){
    
    int c = a + b;
    printf("%d", c);
    foo(a, b, n-1);
}



int main(){
    return 0;
}