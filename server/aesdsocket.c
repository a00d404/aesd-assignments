#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"

int server_fd = -1;

// Флаг для контролируемого выхода из бесконечного цикла
volatile sig_atomic_t keep_running = 1;

// Обработчик сигналов SIGINT и SIGTERM
static void signal_handler(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        keep_running = 0;
        
        // Будим accept, если он заблокирован (закрытием сокета)
        if (server_fd != -1) {
            close(server_fd);
            server_fd = -1;
        }
    }
}

int main(int argc, char *argv[]) {
    // Открываем логирование в syslog (идентификатор "aesdsocket")
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Настраиваем перехват сигналов
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;
    sigaction(SIGINT, &new_action, NULL);
    sigaction(SIGTERM, &new_action, NULL);

    int client_fd = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 1. Создаем потоковый сокет (TCP)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        closelog();
        return -1;
    }

    // Позволяет повторно использовать порт сразу после перезапуска сервера
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Настраиваем адрес структуры для привязки к порту 9000
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Слушаем на всех интерфейсах
    server_addr.sin_port = htons(PORT);

    // 2. Привязываем сокет к порту (bind)
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        closelog();
        return -1;
    }

    // 3. Проверяем аргумент -d для запуска в режиме демона
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(server_fd);
            closelog();
            return -1;
        }
        // Родительский процесс завершает работу, оставляя потомка в фоне
        if (pid > 0) {
            close(server_fd);
            closelog();
            return 0; 
        }

        // Создаем новый сеанс для процесса-потомка
        if (setsid() < 0) {
            perror("setsid failed");
            close(server_fd);
            closelog();
            return -1;
        }

        // Меняем рабочий каталог на корневой
        if (chdir("/") < 0) {
            perror("chdir failed");
            close(server_fd);
            closelog();
            return -1;
        }

        // Перенаправляем стандартные потоки ввода/вывода в /dev/null
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null >= 0) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }
    }
    // 3. Переводим сокет в режим прослушивания (listen)
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        close(server_fd);
        closelog();
        return -1;
    }

    // 4. Главный цикл сервера
    while (keep_running) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            // Если accept прерван сигналом, выходим из цикла
            if (!keep_running) break;
            perror("accept failed");
            continue;
        }

        // Преобразуем IP-адрес клиента в строку для syslog
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Выделяем начальный буфер для пакета данных
        size_t buffer_size = 1024;
        char *packet_buffer = malloc(buffer_size);
        if (packet_buffer == NULL) {
            perror("malloc failed");
            close(client_fd);
            continue;
        }

        size_t total_bytes_received = 0;
        ssize_t bytes_read = 0;
        int packet_complete = 0;

        // Читаем данные из сокета, пока не встретим '\n'
        while (!packet_complete && (bytes_read = recv(client_fd, packet_buffer + total_bytes_received, 1, 0)) > 0) {
            total_bytes_received += bytes_read;

            // Если буфер заполнился, расширяем его
            if (total_bytes_received >= buffer_size) {
                buffer_size += 1024;
                char *new_ptr = realloc(packet_buffer, buffer_size);
                if (new_ptr == NULL) {
                    perror("realloc failed");
                    free(packet_buffer);
                    packet_buffer = NULL;
                    break;
                }
                packet_buffer = new_ptr;
            }

            // Проверяем, является ли последний прочитанный байт символом новой строки
            if (packet_buffer[total_bytes_received - 1] == '\n') {
                packet_complete = 1;
            }
        }

        if (packet_complete && packet_buffer != NULL) {
            // Открываем файл для добавления данных (создаем, если нет)
            int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd >= 0) {
                write(fd, packet_buffer, total_bytes_received);
                close(fd);
            }

            // Теперь читаем весь файл и отправляем обратно клиенту
            fd = open(DATA_FILE, O_RDONLY);
            if (fd >= 0) {
                char send_buffer[1024];
                ssize_t bytes_to_send;
                while ((bytes_to_send = read(fd, send_buffer, sizeof(send_buffer))) > 0) {
                    send(client_fd, send_buffer, bytes_to_send, 0);
                }
                close(fd);
            }
        }

        // Освобождаем память буфера текущего пакета
        if (packet_buffer != NULL) {
            free(packet_buffer);
        }

        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    // Корректная зачистка при выходе
    if (server_fd != -1) {
        close(server_fd);
    }
    // Удаляем файл с данными, если он существует
    unlink(DATA_FILE);

    syslog(LOG_INFO, "Server shutdown complete");
    closelog();
    return 0;
}
