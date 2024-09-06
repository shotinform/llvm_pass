#include <stdio.h>



int main(){
    int a, b, c;

    a = 2;
    b = a;

    if (c > 5){
        b = 3;
    }
    else{
        printf("%d", b);
    }

    while (c == 5){
        printf("%d", b);
    }


    return 0;
}