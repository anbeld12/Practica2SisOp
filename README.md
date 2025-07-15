# Practica2SisOp

**Práctica 2: Búsqueda de Películas en Base de Datos**

Este proyecto, desarrollado para la asignatura de *Sistemas Operativos 2025-1*, implementa un sistema de búsqueda de películas utilizando dos procesos interconectados: un servidor y un cliente. Actualmente, el **proceso servidor está desplegado en una máquina virtual (instancianueva) alojada en Google Cloud Platform**, permitiendo así la interacción remota mediante sockets TCP/IP.

## Integrantes:
- Diego Alejandro Arevalo Arias
- Angel David Beltran Garcia

## Descripción:

El objetivo principal es permitir la búsqueda eficiente de títulos de películas en una base de datos. Para ello, se emplean dos procesos principales:

- **Proceso Servidor:**
   - Desplegado en una VM en GCP (Zona: us-central1-c, IP externa: `35.184.211.253`).
   - Gestiona una HashTable indexada por el título de la película. Cada entrada almacena nodos con el índice, año, título y un puntero al siguiente nodo (para colisiones).
   - Recibe del cliente el título y año, busca coincidencias en la tabla hash, y retorna el campo completo al cliente si encuentra una coincidencia.

- **Proceso Cliente:**
   - Se ejecuta localmente.
   - Solicita al usuario el título y año de la película.
   - Se conecta remotamente al servidor a través de sockets TCP/IP utilizando la IP externa de la VM.
   - Recibe y muestra el resultado de la búsqueda.

## Base de Datos:

La base de datos contiene información de películas desde 1890 hasta la actualidad, descargada desde:

[https://www.kaggle.com/datasets/kunwarakash/imdbdatasets?select=title_basics.tsv](https://www.kaggle.com/datasets/kunwarakash/imdbdatasets?select=title_basics.tsv)

## Formato de los Datos: 

El archivo `title_basics.tsv` contiene las siguientes columnas:
- `tconst`: Identificador alfanumérico único del título.
- `titleType`: Tipo/formato del título (ej. movie, short, tvseries, tvepisode, video).
- `primaryTitle`: El título más popular o el utilizado en materiales promocionales.
- `originalTitle`: Título original, en el idioma original.
- `isAdult`: Indicador de contenido para adultos (0: no-adulto; 1: adulto).
- `startYear`: Año de lanzamiento del título. En series de TV, es el año de inicio.
- `endYear`: Año de fin de la serie de TV. \N para otros tipos de títulos.
- `runtimeMinutes`: Duración principal del título en minutos.
- `genres`: Hasta tres géneros asociados al título.

## Modo de uso:

1. Ejecutar el **servidor** `p2-dataProgram` en la máquina virtual de Google Cloud.
2. Ejecutar el **cliente** `p1-dataProgram` localmente, asegurándose de que el código esté configurado con la IP `35.184.211.253` y el puerto `3350`.

## Funciones Clave:

- `insertar_en_disco(...)`: Inserta nodo en disco.
- `construir_desde_tsv(...)`: Construye tabla desde el archivo TSV.
- `buscar_en_disco(...)`: Busca nodos según título y año.

## Justificación de Criterios de Búsqueda

Consideramos que el título y el año de lanzamiento son los criterios más óptimos para la consulta, ya que nos permiten acercarnos a un resultado único y preciso. Aunque pueden existir varias películas con el mismo nombre, su año de lanzamiento suele ser diferente, lo que facilita la identificación de la entrada deseada en la base de datos.

