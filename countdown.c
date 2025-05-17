#include <stdio.h>
#include <unistd.h>  // để dùng sleep()

int main() {
    for (int i = 10; i >= 0; i--) {
        printf("%d\n", i);
        sleep(1);  // dừng 1 giây
    }
    printf("Đã hết giờ!\n");
    return 0;
}
