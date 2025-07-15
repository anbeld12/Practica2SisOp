// p2-socketServer.c - servidor usando sockets TCP/IP con manejo de señales y seguimiento
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

#define PORT 3350
#define TABLE_SIZE 500
#define MAX_TITLE_LEN 256
#define MAX_RESULT_LEN 4096

typedef struct {
    off_t offset_original;
    char originalTitle[MAX_TITLE_LEN];
    int year;
    off_t siguiente;
} NodoDisco;

int server_fd = -1;

void handle_signal(int signal) {
    printf("\nSeñal %d recibida. Cerrando servidor.\n", signal);
    if (server_fd != -1) {
        close(server_fd);
        printf("Socket servidor cerrado. Puerto %d liberado.\n", PORT);
    }
    exit(0);
}

static inline unsigned hash_string(const char *str) {
    unsigned hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % TABLE_SIZE;
}

void buscar_en_disco(const char *archivo_hash, const char *archivo_tsv, const char *title, int year, char *resultado) {
    FILE *f_hash = fopen(archivo_hash, "rb");
    FILE *f_tsv = fopen(archivo_tsv, "r");
    if (!f_hash || !f_tsv) { snprintf(resultado, MAX_RESULT_LEN, "Error abriendo archivos\n"); if (f_hash) fclose(f_hash); if (f_tsv) fclose(f_tsv); return; }
    unsigned h = hash_string(title);
    off_t pos_cabeza;
    if (fseeko(f_hash, h * sizeof(off_t), SEEK_SET) != 0 || fread(&pos_cabeza, sizeof(pos_cabeza), 1, f_hash) != 1) { snprintf(resultado, MAX_RESULT_LEN, "Error leyendo bucket\n"); fclose(f_hash); fclose(f_tsv); return; }
    NodoDisco nodo; char *line = NULL; size_t cap = 0; bool encontrado = false;
    for (off_t pos = pos_cabeza; pos != 0;) {
        if (fseeko(f_hash, pos, SEEK_SET) != 0 || fread(&nodo, sizeof(nodo), 1, f_hash) != 1) { break; }
        if (strcmp(nodo.originalTitle, title) == 0 && nodo.year == year) {
            if (fseeko(f_tsv, nodo.offset_original, SEEK_SET) == 0 && getline(&line, &cap, f_tsv) != -1) {
                snprintf(resultado, MAX_RESULT_LEN, "%s", line); encontrado = true; break;
            }
        }
        pos = nodo.siguiente;
    }
    if (!encontrado) { snprintf(resultado, MAX_RESULT_LEN, "NA\n"); }
    if (line) free(line); fclose(f_hash); fclose(f_tsv);
}

int main() {

    const char *archivo_hash = "/home/celudiego05/hash_table_title.dat";
    const char *archivo_tsv = "/home/celudiego05/title.basics.tsv";

    int client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[MAX_TITLE_LEN + 16];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("--- INICIANDO SERVIDOR ---\n");

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket creado.\n");

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Socket enlazado al puerto %d.\n", PORT);

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Servidor en escucha.\n");
    printf("Servidor listo. Esperando conexiones.\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // --- INICIO MANEJO CLIENTE ---
        printf("\nConexión cliente desde %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Nuevo bucle para manejar múltiples solicitudes del mismo cliente
        while (1) {
            int len = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (len <= 0) {
                printf("Cliente cerró conexión\n");
                break; 
            }

            buffer[len] = '\0';
            
            // Verificar si el cliente envió la señal de salida (-1)
            if (strcmp(buffer, "-1\n") == 0 || strcmp(buffer, "-1") == 0) {
                printf("Recibida señal -1 cerrando conexión");
                break; 
            }

            char title[MAX_TITLE_LEN];
            int year;

            if (sscanf(buffer, "%[^\n]\n%d", title, &year) < 2) {
                fprintf(stderr, "Formato de mensaje inválido recibido del cliente. Se esperaba 'Titulo\\nAño'.\n");
                send(client_fd, "Error: Formato inválido.\n", strlen("Error: Formato inválido.\n"), 0);
                continue; 
            }
            
            printf("Recibido: Título='%s', Año=%d\n", title, year);

            char resultado[MAX_RESULT_LEN];
            printf("Buscando en base de datos...\n");
            buscar_en_disco(archivo_hash, archivo_tsv, title, year, resultado);
            printf("Busqueda finalizada.\n");
            
            printf("Enviando respuesta...\n");
            send(client_fd, resultado, strlen(resultado), 0);
        } // Fin del bucle interno (manejo de cliente)

        close(client_fd); // Cierra la conexión UNA VEZ que el cliente ha terminado o ha habido un error
        printf("Conexión con cliente cerrada.\n");
        printf("Servidor listo. Esperando nuevas conexiones.\n");
    } // Fin del bucle externo (aceptar nuevas conexiones)

    close(server_fd);
    return 0;
}