| Campo       | Detalle                                                       |
| ----------- | ------------------------------------------------------------- |
| Curso       | Sistemas Operativos (UdeA)                                    |
| Laboratorio | Práctica No. 2 — API de Procesos                              |
| Entregable  | Repositorio GitHub con `README.md` + carpeta de código fuente |
| Ejecutable  | `wish`                                                        |
Nombre completo

Cristian Echeverry
1) Descripción de la solución

Implementamos wish como un loop que:

lee una línea (con getline()),
parsea comandos (con strsep()),
ejecuta built-ins en el proceso del shell y externos con fork() + execv(),
espera terminación con waitpid() y repite.

Modos

Interactivo: imprime wish> (con espacio al final) y lee de stdin.
Batch: ./wish batch.txt lee comandos del archivo y no imprime prompt.
2) Funcionalidades implementadas
2.1 Built-in commands

El enunciado pide implementar exit, cd y path como comandos integrados.

exit
Uso: exit
Si recibe argumentos: error.
cd (alias aceptado: chd por typo del enunciado)
Uso: cd <directorio>
Si recibe 0 o más de 1 argumento: error.
path (alias aceptado: route por typo del enunciado)
Uso: path /bin /usr/bin ...
Sobrescribe la ruta anterior. Si se deja vacío, el shell no ejecuta externos (solo built-ins).
Path inicial: /bin (por enunciado).
2.2 Ejecución de comandos externos
Se resuelve el ejecutable buscando cmd dentro de cada directorio en path usando access(dir/cmd, X_OK).
Luego se ejecuta con fork() + execv(); el padre espera con waitpid().
2.3 Redirección (>)

Soporta comando args > archivo con estas reglas:

Redirige stdout y stderr al mismo archivo.
Si existe, se sobrescribe (truncate).
Son error: varios >, o más de un archivo a la derecha.
2.4 Comandos paralelos (&)

Soporta cmd1 & cmd2 args & cmd3 ...:

Lanza todos los procesos y luego espera a que terminen antes de volver al prompt / siguiente línea.
2.5 Manejo de errores

Para cualquier error, se imprime exactamente:

An error has occurred

a stderr.

Errores fatales (terminan el shell con exit(1)):

Invocar con más de un archivo batch.
Batch file inválido (no se puede abrir).
3) Documentación del código (funciones)

Archivo principal: src/wish.c

print_error()
Imprime el único mensaje de error del proyecto en stderr.
trim()
Elimina espacios/tabs al inicio y final de una cadena (in-place).
tokenize()
Tokeniza un comando por espacios/tabs y arma argv[] para execv().
split_parallel()
Parte una línea por & para crear una lista de comandos “paralelos”.
parse_redirection()
Detecta > y valida el formato; retorna comando limpio y nombre de archivo destino.
path_set(), find_executable()
Gestionan el search path y resuelven el ejecutable usando access(..., X_OK).
run_builtin()
Implementa exit, cd/chd, path/route.
execute_line()
Implementa la lógica completa: paralelismo + redirección + built-ins + externos.
4) Cómo compilar y ejecutar
4.1 Compilar
make
4.2 Ejecutar

Interactivo:

./wish

Batch:

./wish tests/batch_basic.txt
5) Pruebas realizadas
5.1 Básicas (externos)
ls
ls -la /tmp
5.2 Built-ins
path /bin /usr/bin
cd /tmp y luego pwd
exit
5.3 Redirección
ls -la /tmp > output.txt
ls /noexiste > output_err.txt (debe capturar también stderr)
5.4 Paralelismo
sleep 1 & sleep 1 & sleep 1
ls & pwd & whoami
6) Problemas presentados y soluciones
Parseo de espacios múltiples / tabs
Se resolvió normalizando tokens con strsep() y descartando tokens vacíos.
Redirección de stdout y stderr al mismo archivo
Se implementó con open(..., O_TRUNC) + dup2(fd, STDOUT_FILENO) y dup2(fd, STDERR_FILENO).
Esperar múltiples procesos en paralelo
Se guardan pids y al final se hace waitpid() por cada uno.
7) Video de sustentación (10 minutos)
Video: (link de YouTube)
8) Manifiesto de transparencia (IA generativa)
Se usó IA para:
Proponer una arquitectura base (funciones, parsing, estructura de carpetas).
Sugerir casos de prueba y checklist de errores.
Redactar el README (documentación y formato).
No se usó IA para:
Ejecutar el código en la máquina local.
Sustituir el proceso de depuración y pruebas finales.
9) Estructura del repositorio
.
├── src/
│   └── wish.c
├── tests/
│   ├── batch_basic.txt
│   ├── batch_parallel.txt
│   └── batch_redir.txt
├── Makefile
└── README.md