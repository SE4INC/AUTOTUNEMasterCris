# Arquitectura y decisiones DSP

## Detector y seguimiento

PitchDetector recibe muestras una a una, sustituye entradas no finitas por cero, aplica dos filtros de un polo a 1,8 kHz y diezma por un factor entero para analizar aproximadamente a 11–12 kHz en las frecuencias de sesión principales. Ese filtrado es una protección básica, no un filtro antialias de estudio diseñado para analizar cualquier fuente polifónica.

Cada unos 10 ms ejecuta YIN con un ring de 1024 muestras y 512 términos de diferencia por retardo. El rango es 55–700 Hz. Utiliza diferencia acumulativa normalizada, el primer mínimo por debajo de 0,15 e interpolación parabólica. Un RMS filtrado inferior a 0,003 o una confianza menor a 0,85 evita clasificar la trama como voz afinable. Esos umbrales fijos no equivalen a eliminación de ruido.

PitchTracker rechaza errores aislados muy grandes mediante una mediana de cinco observaciones, pero conserva la curva fina para no convertir el vibrato en escalones. Sigue un centro con 120 ms de constante de tiempo. Marca notas sostenidas después de 180 ms sin un cambio significativo. La variación respecto al centro se registra en cents: es una medida heurística, todavía no un clasificador fiable de vibrato frente a portamento. Las transiciones se deducen de cambios del centro y se segmentan notas de al menos 50 ms.

El análisis offline selecciona el canal con más energía para estimar F0. Así evita cancelar una voz estéreo al sumar canales en oposición de fase. LIVE utiliza el primer canal como referencia de F0 y aplica la misma corrección a ambos. Dos cantantes diferentes en L/R no están soportados.

## Decisión musical

PitchCorrection busca la nota permitida más cercana en cromática, mayor, menor natural o pentatónica menor. Introduce 0,15 semitonos de histéresis antes de cambiar objetivo. Una nota editada manualmente toma prioridad sobre la escala. La corrección se limita a una octava en cada dirección y se suaviza con la constante de tiempo SPEED.

HUMANIZE interpola entre la F0 observada y su centro lento para calcular el error. Con más conservación, la corrección actúa principalmente sobre el centro y permanece parte del vibrato original. FLEX utiliza tolerancia y una rampa de entrada; DRIFT escala la intensidad sobre notas sostenidas. No se prometen separación perfecta entre vibrato, deriva y transición ni una detección automática de tonalidad.

## Transformación del audio y formantes

FormantProcessor integra Signalsmith Stretch 1.3.2, fijado por commit, con análisis/síntesis espectral y fase vinculada entre canales. No cambia la duración: entrega el mismo número de muestras que recibe. FORMANT controla cuánto se compensa la envolvente espectral; se proporciona F0 como referencia. Es compensación espectral aproximada, no reconstrucción física exacta del tracto vocal.

LIVE usa ventanas de 80 ms, intervalos de 20 ms y cómputo dividido. CLIP usa 120 ms/30 ms para más resolución en registros graves. En las pruebas iniciales, un intervalo de 10 ms aumentó el error de afinación de tonos armónicos graves; el intervalo final se eligió después de medir ese resultado.

La biblioteca informa latencias de entrada y salida por separado. El render aplica controles en el tiempo del centro de análisis, añade silencio final para recuperar el audio pendiente y descarta el preroll correspondiente. La salida queda alineada y con la longitud exacta del original. En LIVE se retarda el original la misma latencia que el procesado antes de MIX/A-B/bypass. Se retarda además la máscara de voz por la diferencia entre la latencia total y el centro del detector.

Una máscara de voz suavizada a 8–10 ms interpola hacia el audio original en regiones no vocalizadas. Eso reduce el tratamiento de respiraciones y consonantes, pero una ventana espectral sigue pudiendo afectar el entorno de los transitorios. Se debe escuchar y evaluar con grabaciones reales variadas.

## Hilos, memoria y reproducción

- El callback LIVE utiliza arrays fijos para bloques internos de hasta 128 muestras. Los rings y el motor espectral se configuran en prepareToPlay.
- Los callbacks de parámetros solo invalidan una revisión atómica. El worker sondea esa revisión y espera brevemente para agrupar movimientos de controles.
- ClipService carga, analiza y renderiza fuera del hilo de audio. Un nuevo ajuste cancela un render obsoleto; no vuelve a detectar F0 si no cambia el archivo.
- Los snapshots de audio son inmutables. El callback utiliza un puntero protegido por hazard pointer; el worker conserva temporalmente el snapshot retirado mientras se esté leyendo. La destrucción y liberación de buffers ocurre fuera del callback.
- La interfaz consulta snapshots mediante mutex fuera del hilo de audio y se actualiza a 20 Hz. No recibe llamadas de dibujo desde processBlock.
- La exportación pertenece al worker del procesador y se espera su finalización al destruirlo; no hay un hilo de exportación separado que sobreviva a la descarga del plugin.
- CLIP utiliza interpolación lineal si el archivo y el host tienen distintas frecuencias. Para la máxima fidelidad de reproducción usa archivos a la frecuencia de la sesión; la exportación conserva la frecuencia original. Un remuestreador bandlimited para esa ruta queda pendiente.

La memoria de un clip puede ser importante: un archivo estéreo de 180 s a 96 kHz ocupa aproximadamente 138 MB en float32 por copia. Original, render actual y un render nuevo pueden coexistir brevemente. El límite por archivo es 180 s, no una garantía de bajo uso de RAM.

## Persistencia

APVTS guarda los parámetros. Se agregan el índice del último preset, la ruta del audio y las notas editadas a un ValueTree serializado a XML binario de JUCE. Los renders se reconstruyen al abrir el proyecto. Las ediciones se identifican por índice y tiempo de inicio; si se sustituye el archivo por otro contenido, deben revisarse. No hay edición destructiva del original, audio embebido, ARA, undo/redo ni editor de tempo en esta versión.
