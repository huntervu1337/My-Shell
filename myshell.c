/*
 * MyShell - Shell đơn giản, chạy trên Linux
 * Tác giả: Nguyễn Đình Vũ
 * Môn học: Hệ điều hành
 * Mô tả: Shell đơn giản hỗ trợ thực thi lệnh hệ thống, lệnh nội bộ (cd, exit), chạy chương trình.
 * Biên dịch: gcc -o myshell myshell.c
 * Sử dụng: ./myshell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

typedef struct {
    pid_t pid;
    char name[256];
    int running; // 1: running, 0: stopped
} bg_process_t;

#define MAX_BG 128
bg_process_t bg_list[MAX_BG];
int bg_count = 0;

// Thêm tiến trình background vào danh sách
void add_bg_process(pid_t pid, char *name) {
    if (bg_count < MAX_BG) {
        bg_list[bg_count].pid = pid;
        strncpy(bg_list[bg_count].name, name, sizeof(bg_list[bg_count].name)-1);
        bg_list[bg_count].name[sizeof(bg_list[bg_count].name)-1] = '\0';
        bg_list[bg_count].running = 1;
        bg_count++;
    }
}

// Cập nhật trạng thái tiến trình (dừng/tiếp tục)
void update_bg_status(pid_t pid, int running) {
    for (int i = 0; i < bg_count; ++i) {
        if (bg_list[i].pid == pid) {
            bg_list[i].running = running;
            break;
        }
    }
}

// Xóa tiến trình khỏi danh sách
void remove_bg_process(pid_t pid) {
    for (int i = 0; i < bg_count; ++i) {
        if (bg_list[i].pid == pid) {
            for (int j = i; j < bg_count - 1; ++j) {
                bg_list[j] = bg_list[j+1];
            }
            bg_count--;
            break;
        }
    }
}

void parse_input(char *input, char **argv, int *background) {
    int argc = 0;
    char *token = strtok(input, " \t\n");
    *background = 0;

    while (token != NULL && argc < MAX_ARGS - 1) {
        if (strcmp(token, "&") == 0) {
            *background = 1; // Phát hiện dấu &
        } else {
            argv[argc++] = token;
        }
        token = strtok(NULL, " \t\n");
    }

    argv[argc] = NULL;
}

void execute_command(char **argv, int background) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // Tiến trình con: Khôi phục xử lý mặc định cho SIGINT
        signal(SIGINT, SIG_DFL);
        if (execvp(argv[0], argv) == -1) {
            perror("myshell");
        }
        exit(EXIT_FAILURE); // Chỉ chạy nếu exec thất bại
    } else {
        if (!background) {
            signal(SIGINT, SIG_IGN);
            int status;
            waitpid(pid, &status, 0);
            signal(SIGINT, SIG_DFL);
        } else {
            printf("[Background] PID: %d\n", pid);
            add_bg_process(pid, argv[0]);
        }
    }
}

// Xử lý tiến trình zombie background
void reap_bg_processes() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_bg_process(pid);
    }
}

int handle_builtin(char **argv) {
    // Lệnh cd: chuyển thư mục
    if (strcmp(argv[0], "cd") == 0) {
        const char *path = argv[1];
        if (path == NULL) {
            path = getenv("HOME");  // Mặc định về HOME nếu không có đối số
        }

        if (chdir(path) != 0) {
            perror("cd");
        }
        return 1;  // Đã xử lý
    }
    else if (strcmp(argv[0], "exit") == 0) {
        exit(0);
    }
    // Lệnh list: liệt kê tiến trình background
    else if (strcmp(argv[0], "list") == 0) {
        printf("PID\tName\t\tStatus\n");
        for (int i = 0; i < bg_count; ++i) {
            printf("%d\t%s\t\t%s\n", bg_list[i].pid, bg_list[i].name, bg_list[i].running ? "Running" : "Stopped");
        }
        return 1;
    }
    // Lệnh kill <pid>: kết thúc tiến trình background
    else if (strcmp(argv[0], "kill") == 0) {
        if (argv[1] == NULL) {
            printf("Usage: kill <pid>\n");
        } else {
            pid_t pid = atoi(argv[1]);
            if (kill(pid, SIGKILL) == 0) {
                printf("Killed process %d\n", pid);
                remove_bg_process(pid);
            } else {
                perror("kill");
            }
        }
        return 1;
    }
    // Lệnh stop <pid>: dừng tiến trình background
    else if (strcmp(argv[0], "stop") == 0) {
        if (argv[1] == NULL) {
            printf("Usage: stop <pid>\n");
        } else {
            pid_t pid = atoi(argv[1]);
            if (kill(pid, SIGSTOP) == 0) {
                printf("Stopped process %d\n", pid);
                update_bg_status(pid, 0);
            } else {
                perror("stop");
            }
        }
        return 1;
    }
    // Lệnh resume <pid>: tiếp tục tiến trình background
    else if (strcmp(argv[0], "resume") == 0) {
        if (argv[1] == NULL) {
            printf("Usage: resume <pid>\n");
        } else {
            pid_t pid = atoi(argv[1]);
            if (kill(pid, SIGCONT) == 0) {
                printf("Resumed process %d\n", pid);
                update_bg_status(pid, 1);
            } else {
                perror("resume");
            }
        }
        return 1;
    }
    // Lệnh path: in ra biến môi trường PATH
    else if (strcmp(argv[0], "path") == 0) {
        char *path = getenv("PATH");
        if (path) {
            printf("PATH=%s\n", path);
        } else {
            printf("PATH is not set.\n");
        }
        return 1;
    }
    // Lệnh addpath: thêm thư mục vào PATH
    else if (strcmp(argv[0], "addpath") == 0) {
        if (argv[1] == NULL) {
            printf("Usage: addpath <directory>\n");
        } else {
            char *old_path = getenv("PATH");
            char new_path[MAX_INPUT * 2];
            if (old_path) {
                snprintf(new_path, sizeof(new_path), "%s:%s", old_path, argv[1]);
            } else {
                snprintf(new_path, sizeof(new_path), "%s", argv[1]);
            }
            setenv("PATH", new_path, 1);
            printf("PATH updated: %s\n", new_path);
        }
        return 1;
    }

    return 0;  // Không phải lệnh nội bộ
}

int main() {
    char input[MAX_INPUT];
    char *argv[MAX_ARGS];
    int background;

    while (1) {
        reap_bg_processes(); // Thu dọn tiến trình zombie
        printf("myShell> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;  // EOF (Ctrl+D)
        }

        // Bỏ qua dòng trống
        if (strcmp(input, "\n") == 0) continue;

        parse_input(input, argv, &background);  // Phân tách lệnh và kiểm tra background

        if (argv[0] != NULL) {
            if (!handle_builtin(argv)) {
                execute_command(argv, background);  // Thực thi lệnh hệ thống
            }
        }
    }

    return 0;
}
