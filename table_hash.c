#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h> // Necesario para off_t, aunque no en todos los sistemas
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

// Definiciones de tamaño para la tabla hash y la longitud máxima del título
#define TABLE_SIZE 500
#define MAX_TITLE_LEN 256

// Estructura para los nodos de la HashTable en disco
typedef struct {
    off_t offset_original;              // Posición en el archivo TSV (para futuras búsquedas)
    char originalTitle[MAX_TITLE_LEN];  // Título original
    int year;                           // Año
    off_t siguiente;                    // Offset al siguiente nodo en la lista
} NodoDisco;

// --- Funciones de Utilidad ---

// Función de hash (djb2)
static inline unsigned hash_string(const char *str) {
    unsigned hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c

    return hash % TABLE_SIZE;
}

// Crea un archivo de tabla hash vacío en disco
void crear_tabla_hash_vacia(const char *nombre_archivo_hash) {
    FILE *f = fopen(nombre_archivo_hash, "wb");
    if (!f) {
        fprintf(stderr, "Error creando '%s': %s\n", nombre_archivo_hash, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Inicializa la tabla con ceros (representando listas vacías al inicio)
    off_t cero = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (fwrite(&cero, sizeof(cero), 1, f) != 1) {
            fprintf(stderr, "Error inicializando tabla hash: %s\n", strerror(errno));
            fclose(f);
            exit(EXIT_FAILURE);
        }
    }
    fclose(f);
    printf("Archivo de tabla hash vacío '%s' creado.\n", nombre_archivo_hash);
}

// Inserta un nodo en la tabla hash en disco
bool insertar_en_disco(const char *nombre_archivo_hash, const char *title, int year, off_t offset_tsv) {
    // Abre el archivo de tabla hash para lectura y escritura binaria
    FILE *f = fopen(nombre_archivo_hash, "r+b");
    if (!f) {
        fprintf(stderr, "Error abriendo '%s' para insertar: %s\n", nombre_archivo_hash, strerror(errno));
        return false;
    }

    // Calcula el índice del bucket usando la función de hash
    unsigned h = hash_string(title);
    off_t cabeza;

    // Lee el offset de la "cabeza" de la lista enlazada para este bucket
    if (fseeko(f, h * sizeof(off_t), SEEK_SET) != 0 ||
        fread(&cabeza, sizeof(cabeza), 1, f) != 1) {
        fprintf(stderr, "Error leyendo cabeza de lista en bucket %u: %s\n", h, strerror(errno));
        fclose(f);
        return false;
    }

    // Mueve el puntero al final del archivo para añadir el nuevo nodo
    if (fseeko(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error buscando fin de archivo en '%s': %s\n", nombre_archivo_hash, strerror(errno));
        fclose(f);
        return false;
    }

    off_t nuevo_offset = ftello(f); // Este será el offset donde se escribirá el nuevo nodo

    // Prepara el nuevo nodo
    NodoDisco nodo;
    strncpy(nodo.originalTitle, title, MAX_TITLE_LEN - 1);
    nodo.originalTitle[MAX_TITLE_LEN - 1] = '\0';
    nodo.year = year;
    nodo.offset_original = offset_tsv;
    nodo.siguiente = cabeza;  // El nuevo nodo apunta a la antigua cabeza de la lista

    // Escribe el nodo en disco
    if (fwrite(&nodo, sizeof(nodo), 1, f) != 1) {
        fprintf(stderr, "Error escribiendo nodo en '%s': %s\n", nombre_archivo_hash, strerror(errno));
        fclose(f);
        return false;
    }

    // Actualiza el puntero de la cabeza de la lista en el bucket para que apunte al nuevo nodo
    if (fseeko(f, h * sizeof(off_t), SEEK_SET) != 0 ||
        fwrite(&nuevo_offset, sizeof(nuevo_offset), 1, f) != 1) {
        fprintf(stderr, "Error actualizando cabeza de lista en bucket %u: %s\n", h, strerror(errno));
        fclose(f);
        return false;
    }

    fclose(f); // Cierra el archivo
    return true;
}

// --- Función Principal ---

int main(void) {
    // Define la ruta al archivo TSV de entrada y al archivo de tabla hash de salida
    const char *ruta_tsv_entrada = "title.basics.tsv"; // Asegúrate de que este archivo esté en el mismo directorio
    const char *ruta_hash_salida = "hash_table_title.dat"; // Nombre del archivo de tabla hash a generar

    printf("Iniciando la generación de la tabla hash.\n");
    printf("Archivo TSV de entrada: '%s'\n", ruta_tsv_entrada);
    printf("Archivo de tabla hash de salida: '%s'\n", ruta_hash_salida);

    // 1. Crea un archivo de tabla hash vacío con el tamaño inicial
    crear_tabla_hash_vacia(ruta_hash_salida);

    // 2. Abre el archivo TSV de entrada para lectura
    FILE *f_tsv = fopen(ruta_tsv_entrada, "r");
    if (!f_tsv) {
        fprintf(stderr, "Error: No se pudo abrir el archivo TSV '%s': %s\n", ruta_tsv_entrada, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    long long processed_lines = 0;
    long long inserted_entries = 0;

    // 3. Descarta la línea de encabezado del TSV
    if ((len = getline(&line, &cap, f_tsv)) == -1) {
        fprintf(stderr, "Error: El archivo TSV está vacío o hubo un problema al leer el encabezado.\n");
        free(line);
        fclose(f_tsv);
        exit(EXIT_FAILURE);
    }
    processed_lines++;

    printf("Procesando archivo TSV...\n");

    // 4. Procesa cada línea del TSV para extraer datos e insertar en la tabla hash
    while ((len = getline(&line, &cap, f_tsv)) != -1) {
        processed_lines++;
        off_t pos = ftello(f_tsv) - len;  // Posición de inicio de la línea actual

        // Extrae los campos relevantes: título original (campo 3) y año (campo 5)
        char *saveptr = NULL;
        char *token;
        int field_idx = 0;
        int year = 0;
        char originalTitle[MAX_TITLE_LEN] = {0};

        char *temp_line = strdup(line); // Duplicar la línea para strtok_r
        if (temp_line == NULL) {
            fprintf(stderr, "Error de memoria al duplicar línea.\n");
            continue; // Saltar a la siguiente línea si hay un error de memoria
        }

        token = strtok_r(temp_line, "\t\n", &saveptr);
        while (token) {
            switch (field_idx) {
                case 3: // Campo para originalTitle
                    strncpy(originalTitle, token, MAX_TITLE_LEN - 1);
                    originalTitle[MAX_TITLE_LEN - 1] = '\0';
                    break;
                case 5: // Campo para startYear
                    if (strcmp(token, "\\N") != 0) {
                        year = atoi(token);
                    }
                    break;
            }
            token = strtok_r(NULL, "\t\n", &saveptr);
            field_idx++;
        }
        free(temp_line); // Liberar la copia de la línea

        // Inserta en la tabla hash si los datos son válidos
        // Se valida que el título no esté vacío y el año esté en un rango razonable
        if (originalTitle[0] != '\0' && year >= 1800 && year <= 2100) { // Rango de años más realista
            if (insertar_en_disco(ruta_hash_salida, originalTitle, year, pos)) {
                inserted_entries++;
            } else {
                fprintf(stderr, "Advertencia: Falló la inserción para '%s' (%d).\n", originalTitle, year);
            }
        }
        // Puedes añadir un printf aquí para ver el progreso, por ejemplo, cada 10000 líneas
        if (processed_lines % 100000 == 0) {
            printf("Líneas procesadas: %lld\n", processed_lines);
        }
    }

    // 5. Libera recursos y cierra archivos
    free(line);
    fclose(f_tsv);

    printf("\nProceso de generación de tabla hash completado.\n");
    printf("Total de líneas TSV procesadas: %lld\n", processed_lines);
    printf("Total de entradas insertadas en la tabla hash: %lld\n", inserted_entries);
    printf("El archivo de tabla hash '%s' ha sido generado con éxito.\n", ruta_hash_salida);

    return EXIT_SUCCESS;
}