# Windows y FL Studio

## Instalación

1. Descarga el artefacto Windows x64 de una ejecución correcta de GitHub Actions y extráelo.
2. Cierra FL Studio antes de reemplazar un plugin instalado.
3. Copia **la carpeta completa** `CRIS VOX TUNER.vst3` a `C:\Program Files\Common Files\VST3`. Windows puede pedir permisos de administrador para esa carpeta. También puedes elegir una carpeta VST3 que tengas configurada en el host.
4. En FL Studio abre el gestor de plugins, comprueba las carpetas de búsqueda y ejecuta la búsqueda de plugins nuevos.
5. Carga CRIS VOX TUNER como efecto del mezclador en el canal de la voz.

Usa el artefacto **Windows**, no el bundle compilado para Linux. La prueba de escaneo y uso en FL Studio real queda pendiente en esta entrega. No se ha generado un instalador ni una firma de código.

## LIVE INPUT

Es el modo de entrada normal. La voz de la pista pasa por el afinador. Selecciona la tonalidad y escala, elige un preset y ajusta la intensidad. La compensación de latencia depende de la configuración del host. Con una latencia de aproximadamente 100 ms, trabaja con voz ya grabada y evita usar este diseño para monitorizarte mientras grabas.

MIX y ORIGINAL A/B comparan señales alineadas dentro del motor. No debes crear una copia de la voz sin compensación de latencia para mezclarla en paralelo. BYPASS interno conserva el retardo del motor.

## CLIP EDITOR

Al importar un archivo, el plugin cambia a modo fuente CLIP. En este modo **silencia su entrada del mezclador** y reproduce el clip cargado, para no duplicarlo con la pista original. No es una integración ARA: no lee automáticamente clips del Playlist.

Con HOST SYNC desactivado, PLAY inicia una escucha local desde el principio y STOP la detiene. No está ligada al transporte de FL Studio.

Con HOST SYNC activado, la reproducción sigue el transporte y la posición temporal proporcionados por el host. CLIP START indica el segundo del proyecto donde empieza el archivo. La ruta compensa la latencia declarada, pero debe verificarse en FL Studio con una señal de prueba antes de usarla para entregar un mix. Comprueba reproducción, inicio, búsqueda, loops, pre-roll y compensación de latencia. Si el host no proporciona una posición temporal válida, no se inicia reproducción sincronizada.

La ruta más predecible de esta primera versión es importar, editar y **EXPORT WAV**, y después colocar el WAV exportado en el Playlist en la misma posición que la toma original. No reproduzcas a la vez ambas tomas sin querer hacerlo.

## Comprobación de aceptación pendiente

- Escaneo limpio del VST3 x64 y apertura/cierre repetido de su interfaz.
- Sesiones a 44,1/48/96 kHz; buffers 64/128/256/512/1024.
- Automatización rápida, cambios de preset, MIX 0/50/100 y bypass.
- Recuperación del proyecto con rutas válidas y con un archivo de audio ausente.
- Exportación offline del proyecto y comparación de timing con el original.
- HOST SYNC con loops y cambios de posición.
- Varias instancias en una sesión real, con medición de CPU media y picos.
- Escucha de voces masculinas graves, sibilantes, respiraciones, vibrato, portamentos y cambios de registro.
