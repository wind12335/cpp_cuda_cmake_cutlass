// 最普通的 C 代码：4x4 矩阵乘，用来走完 LLVM 编译全流程
void matmul(int n, int a[4][4], int b[4][4], int c[4][4]) {
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
      for (int k = 0; k < n; k++)
        c[i][j] += a[i][k] * b[k][j];
}

#include <stdio.h>
int main() {
  int a[4][4] = {{1,2,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
  int b[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
  int c[4][4] = {0};
  matmul(4, a, b, c);
  printf("c[0][1] = %d\n", c[0][1]);  // 应该 = 2
  return 0;
}
