#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define NUM_CHILDREN 5  // Кількість дочірніх процесів для створення
#define MAX_DELAY_SEC 6 // Максимальна затримка для дочірніх процесів у секундах

int main() {
    pid_t pids_created[NUM_CHILDREN]; // Масив для зберігання PID створених дочірніх процесів
    int i;

    srand(time(NULL));

    printf("БАТЬКІВСЬКИЙ ПРОЦЕС (PID: %d): Починаю роботу.\n", getpid());
    printf("БАТЬКІВСЬКИЙ: Створюю %d дочірніх процесів...\n\n", NUM_CHILDREN);

    for (i = 0; i < NUM_CHILDREN; i++) {
        pids_created[i] = fork();

        if (pids_created[i] < 0) {
            perror("Помилка fork");
            for (int j = 0; j < i; j++) {
                if (pids_created[j] > 0) {
                     wait(NULL);
                }
            }
            exit(EXIT_FAILURE);
        }
        else if (pids_created[i] == 0) {
            pid_t my_pid = getpid();
            srand(time(NULL) ^ getpid());

            int delay = (rand() % MAX_DELAY_SEC) + 1; 

            printf("  ДОЧІРНІЙ (PID: %d, Батько PPID: %d): Створено. Затримка на %d сек.\n", my_pid, getppid(), delay);
            sleep(delay); 
            printf("  ДОЧІРНІЙ (PID: %d): Завершую роботу. Код виходу: %d.\n", my_pid, i);
            exit(i); 
        }
        else {
            printf("БАТЬКІВСЬКИЙ: Створено дочірній процес з PID: %d (порядковий номер %d).\n", pids_created[i], i);
        }
    }

    printf("\nБАТЬКІВСЬКИЙ: Всі %d дочірні процеси створено.\n", NUM_CHILDREN);
    printf("БАТЬКІВСЬКИЙ: Очікую на завершення дочірніх процесів...\n\n");

    int status;
    pid_t terminated_pid;
    int children_reaped_count = 0;

    while (children_reaped_count < NUM_CHILDREN) {
        // wait() блокує батьківський процес, доки БУДЬ-ЯКИЙ дочірній процес не завершиться
        terminated_pid = wait(&status);

        if (terminated_pid > 0) {
            children_reaped_count++;
            printf("БАТЬКІВСЬКИЙ: wait() повернув PID %d.\n", terminated_pid);

            if (WIFEXITED(status)) { // Перевіряємо, чи процес завершився нормально
                printf("БАТЬКІВСЬКИЙ: Дочірній процес PID %d завершився нормально. Код виходу: %d.\n",
                       terminated_pid, WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status)) { // Перевіряємо, чи процес був завершений сигналом
                 printf("БАТЬКІВСЬКИЙ: Дочірній процес PID %d був завершений сигналом %d.\n",
                       terminated_pid, WTERMSIG(status));
            }
            else {
                printf("БАТЬКІВСЬКИЙ: Дочірній процес PID %d завершився з невідомим статусом.\n",
                       terminated_pid);
            }
            printf("БАТЬКІВСЬКИЙ: Залишилося очікувати %d дочірніх процесів.\n\n", NUM_CHILDREN - children_reaped_count);

        }
        else {
            if (errno == ECHILD) {
                printf("БАТЬКІВСЬКИЙ: Помилка wait: немає більше дочірніх процесів для очікування (ECHILD).\n");
                break; 
            }
            else if (errno == EINTR) {
                printf("БАТЬКІВСЬКИЙ: Виклик wait() був перерваний сигналом. Повторна спроба...\n");
                continue; 
            }
            else {
                perror("Помилка wait");
                break; 
            }
        }
    }

    if (children_reaped_count == NUM_CHILDREN) {
        printf("\nБАТЬКІВСЬКИЙ: Всі %d дочірні процеси успішно завершено та зібрано.\n", NUM_CHILDREN);
    }
    else {
        printf("\nБАТЬКІВСЬКИЙ: Не всі дочірні процеси були зібрані належним чином (зібрано %d з %d).\n", children_reaped_count, NUM_CHILDREN);
    }
    printf("БАТЬКІВСЬКИЙ ПРОЦЕС: Завершення роботи.\n");

    return 0;
}
