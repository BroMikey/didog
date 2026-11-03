#include "stdio.h"

int main(){
    double res = 0;
    int  flag = 1;
    for (int i = 1; i <= 100; i++){
        res += (double)(1.0/i) * flag;
        flag = -flag;
    }
    printf("res = %.6f\n", res);
}
