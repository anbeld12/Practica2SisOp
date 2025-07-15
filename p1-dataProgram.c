// p1-socketClient.c - cliente TCP/IP que consulta por título y año
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_TITLE_LEN 256
#define MAX_RESULT_LEN 4096
#define PORT 3350
#define SERVER_IP "35.184.211.253" // IP del servidor hardcodeada

void printf_formateado(char *resultado) {
    printf("\n%-10s %-10s %-25s %-25s %-6s %-6s %-6s %-6s %-20s\n",
           "tconst", "type", "primaryTitle", "originalTitle", "adult", "year", "end", "min", "genres");
    printf("---------------------------------------------------------------------------------------------------------------------\n");

    char *token = strtok(resultado, "\t\n");
    int field = 0;

    while (token != NULL) {
        switch (field % 9) {
            case 0: printf("%-10s ", token); break;
            case 1: printf("%-10s ", token); break;
            case 2: printf("%-25s ", token); break;
            case 3: printf("%-25s ", token); break;
            case 4: printf("%-6s ", token); break;
            case 5: printf("%-6s ", token); break;
            case 6: printf("%-6s ", token); break;
            case 7: printf("%-6s ", token); break;
            case 8: printf("%-20s\n", token); break;
        }
        token = strtok(NULL, "\t\n");
        field++;
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char title[MAX_TITLE_LEN];
    int year;
    char mensaje[MAX_TITLE_LEN + 10];
    char resultado[MAX_RESULT_LEN];
    int len;
    char input[20];
    int opcion_continuar = 0;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("connect");
        close(sock);
        return EXIT_FAILURE;
    }

    do {
        printf("\n--- Nueva Búsqueda ---\n");
        printf("Ingrese el título original a buscar: ");
        fgets(title, MAX_TITLE_LEN, stdin);
        title[strcspn(title, "\n")] = '\0';

        printf("Ingrese el año a buscar: ");
        if (fgets(input, sizeof(input), stdin) == NULL || sscanf(input, "%d", &year) != 1) {
            fprintf(stderr, "Entrada de año inválida. Intente de nuevo.\n");
            opcion_continuar = 1;
            continue;
        }

        snprintf(mensaje, sizeof(mensaje), "%s\n%d", title, year);
        send(sock, mensaje, strlen(mensaje), 0);

        len = recv(sock, resultado, sizeof(resultado) - 1, 0);
        if (len <= 0) {
            fprintf(stderr, "No hay respuesta del servidor\n");
            break;
        }

        resultado[len] = '\0';

        if (strcmp(resultado, "NA\n") == 0) {
            printf("\nNo se encontraron resultados.\n");
        } else {
            printf_formateado(resultado);
        }

        printf("\n¿Desea realizar otra búsqueda? (Presione -1 para salir, cualquier otra tecla/número para continuar): ");
        if (fgets(input, sizeof(input), stdin) != NULL) {
            opcion_continuar = atoi(input);
        } else {
            opcion_continuar = 1;
        }
    } while (opcion_continuar != -1);

    printf("Enviando señal de salida (-1) al servidor...\n");
    send(sock, "-1\n", strlen("-1\n"), 0);

    close(sock);
    printf("Saliendo del cliente.\n");
    return EXIT_SUCCESS;
}
