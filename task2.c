#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#define NUM_CHILDREN 3

void child_process_task(int id, int delay_seconds) {
    printf("  ДОЧІРНІЙ %d (PID: %d): Створено. Працюватиму %d сек. Мій код виходу: %d.\n",
           id, getpid(), delay_seconds, id * 10);
    sleep(delay_seconds); // Симуляція роботи
    printf("  ДОЧІРНІЙ %d (PID: %d): Роботу завершено. Вихід.\n", id, getpid());
    exit(id * 10); // Завершення з унікальним кодом виходу
}

// Допоміжна функція для друку статусу завершення
void print_child_status(pid_t child_pid, int status) {
    if (WIFEXITED(status)) {
        printf("БАТЬКІВСЬКИЙ: Дочірній процес PID %d завершився нормально з кодом %d.\n",
               child_pid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        printf("БАТЬКІВСЬКИЙ: Дочірній процес PID %d був завершений сигналом %d.\n",
               child_pid, WTERMSIG(status));
    }
    else {
        printf("БАТЬКІВСЬКИЙ: Дочірній процес PID %d завершився ненормально (інша причина).\n", child_pid);
    }
}

int main() {
    pid_t pids[NUM_CHILDREN];
    int child_ids[NUM_CHILDREN] = {1, 2, 3};
    
    int delays[NUM_CHILDREN] = {2, 6, 4}; 

    int status;
    pid_t terminated_pid;
    int reaped_children_count = 0;          // Лічильник "зібраних" дочірніх процесів

    printf("БАТЬКІВСЬКИЙ (PID: %d): Програма починається, створення %d дочірніх процесів...\n", getpid(), NUM_CHILDREN);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        int current_child_id = child_ids[i];

        // Розподіл затримок: pids[0] для Дитини 1 (2с), pids[1] для Дитини 2 (6с), pids[2] для Дитини 3 (4с)
        int current_delay = (i == 0) ? delays[0] : (i == 1 ? delays[1] : delays[2]);

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork помилка");

            // Завершити вже створені дочірні процеси перед виходом
            for (int j = 0; j < i; j++) {
                if (pids[j] > 0) kill(pids[j], SIGTERM);
            }
            while(wait(NULL) > 0); 
            exit(EXIT_FAILURE);
        }
        else if (pids[i] == 0) {
            child_process_task(current_child_id, current_delay);
        }
        else {
            printf("БАТЬКІВСЬКИЙ: Створено Дочірній %d з PID %d (очікувана затримка %d сек).\n",
                   current_child_id, pids[i], current_delay);
        }
    }
    
    // --- Фаза 1: Демонстрація wait() ---
    printf("\nБАТЬКІВСЬКИЙ [Фаза 1]: Використання wait() для очікування БУДЬ-ЯКОГО дочірнього процесу.\n");
    printf("БАТЬКІВСЬКИЙ: Очікую, що Дитина 1 (PID %d, затримка %ds) завершиться першою.\n", pids[0], delays[0]);

    terminated_pid = wait(&status); 
    if (terminated_pid > 0) {
        printf("БАТЬКІВСЬКИЙ [Фаза 1]: wait() повернув PID %d.\n", terminated_pid);
        print_child_status(terminated_pid, status);
        for(int k=0; k<NUM_CHILDREN; ++k) if(pids[k] == terminated_pid) pids[k] = 0; 
        reaped_children_count++;
    }
    else {
        perror("БАТЬКІВСЬКИЙ [Фаза 1]: wait() помилка");
    }

    // --- Фаза 2: Демонстрація waitpid(specific_pid, &status, 0) ---
    printf("\nБАТЬКІВСЬКИЙ [Фаза 2]: Використання waitpid() для очікування КОНКРЕТНО Дитини 3 (PID %d, затримка %ds).\n", pids[2], delays[2]);
    
    if (pids[2] != 0) { 
        terminated_pid = waitpid(pids[2], &status, 0); 
        if (terminated_pid > 0) {
            printf("БАТЬКІВСЬКИЙ [Фаза 2]: waitpid(PID %d) повернув PID %d.\n", pids[2], terminated_pid);
            print_child_status(terminated_pid, status);
            if (pids[2] == terminated_pid) pids[2] = 0; 
            reaped_children_count++;
        }
        else {
             if (errno == ECHILD)
                printf("БАТЬКІВСЬКИЙ [Фаза 2]: Дитина %d (PID %d) вже була зібрана або не існує.\n", child_ids[2], pids[2]);
             else
                perror("БАТЬКІВСЬКИЙ [Фаза 2]: waitpid() для конкретної дитини помилка");
        }
    }
    else {
        printf("БАТЬКІВСЬКИЙ [Фаза 2]: Дитина 3 (початковий індекс pids[2]) вже була позначена як зібрана.\n");
    }

    // --- Фаза 3: Демонстрація waitpid(-1, &status, WNOHANG) ---
    printf("\nБАТЬКІВСЬКИЙ [Фаза 3]: Використання waitpid(-1, ..., WNOHANG) для неблокуючої перевірки решти.\n");
    if (pids[1] != 0) {
        printf("БАТЬКІВСЬКИЙ: Очікую знайти Дитину 2 (початковий PID %d, затримка %ds).\n", pids[1], delays[1]);
    }

    int attempts = 0;
    while (reaped_children_count < NUM_CHILDREN && attempts < 25) {
        terminated_pid = waitpid(-1, &status, WNOHANG);

        if (terminated_pid > 0) { 
            printf("БАТЬКІВСЬКИЙ [Фаза 3]: waitpid() з WNOHANG зібрав PID %d на спробі %d.\n", terminated_pid, attempts + 1);
            print_child_status(terminated_pid, status);

            for(int k=0; k<NUM_CHILDREN; ++k) 
                if(pids[k] == terminated_pid) 
                    pids[k] = 0;
            reaped_children_count++;
        }
        else if (terminated_pid == 0) { 
            printf("БАТЬКІВСЬКИЙ [Фаза 3]: (Спроба %d) Жоден дочірній процес не завершився. Виконую іншу роботу...\n", attempts + 1);
            usleep(500000); 
        }
        else { 
            if (errno == ECHILD) { 
                printf("БАТЬКІВСЬКИЙ [Фаза 3]: (Спроба %d) Немає більше дочірніх процесів для очікування (ECHILD).\n", attempts + 1);
            }
            else {
                perror("БАТЬКІВСЬКИЙ [Фаза 3]: waitpid() з WNOHANG помилка");
            }
            break; 
        }
        attempts++;
        if (reaped_children_count == NUM_CHILDREN) break; 
    }
    
    printf("\nБАТЬКІВСЬКИЙ: Завершення спроб \"збору\" дочірніх процесів.\n");
    if (reaped_children_count == NUM_CHILDREN) {
        printf("БАТЬКІВСЬКИЙ: Всі %d дочірні процеси були успішно \"зібрані\".\n", NUM_CHILDREN);
    }
    else {
        printf("БАТЬКІВСЬКИЙ: \"Зібрано\" %d з %d дочірніх процесів.\n", reaped_children_count, NUM_CHILDREN);
        printf("БАТЬКІВСЬКИЙ: Залишкові PID (0 якщо зібрано або помилка при створенні):\n");

        printf("  Початковий PID для Дитини 1 (індекс 0, затримка %ds): %s\n", delays[0], (pids[0] == 0) ? "зібрано/помилка" : "не зібрано");
        printf("  Початковий PID для Дитини 2 (індекс 1, затримка %ds): %s\n", delays[1], (pids[1] == 0) ? "зібрано/помилка" : "не зібрано");
        printf("  Початковий PID для Дитини 3 (індекс 2, затримка %ds): %s\n", delays[2], (pids[2] == 0) ? "зібрано/помилка" : "не зібрано");
    }

    printf("БАТЬКІВСЬКИЙ: Завершення програми.\n");
    return 0;
}
