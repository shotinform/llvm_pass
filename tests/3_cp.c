#include <stdio.h>



int main(){
    int a, b, c;

    a = 2;
    b = a;
    // kada promenimo jedan operand
    b = 3;
    c = b;

    printf("%d", c);


    return 0;
}