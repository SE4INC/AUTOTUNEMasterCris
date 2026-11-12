# Alcance de esta entrega

Es una implementación funcional de desarrollo con DSP, editor y pruebas. No es una certificación de calidad equivalente a herramientas comerciales de afinación ni una versión profesional ya terminada.

## Funciona en el código y en las pruebas locales

- Detección y corrección real de tono, no solo visualización.
- Presets conectados a parámetros del motor.
- LIVE con mezcla alineada y procesamiento estéreo vinculado.
- Importación, análisis, notas editables, render y exportación.
- Estado con parámetros, rutas y objetivos manuales.
- Compensación espectral de formantes.
- Interfaz JUCE real con capturas verificadas en dos tamaños.

## Límites actuales

- Una voz monofónica por instancia. No hay separación de fuentes ni procesamiento polifónico.
- Rango de detección 55–700 Hz; los registros fuera del rango permanecen sin corrección fiable.
- Los umbrales de voz/ruido son fijos. Ruido tonal, reverberación fuerte o voz distorsionada pueden confundir F0.
- La segmentación es heurística: vibrato amplio, glissandos y saltos rápidos pueden producir notas divididas o agrupadas. Se puede cambiar el objetivo de una nota, pero todavía no dividir o unir segmentos manualmente.
- La conservación de vibrato se basa en seguir un centro lento. No hay un clasificador de vibrato validado por separado.
- DRIFT controla intensidad en notas sostenidas; todavía no ofrece una curva de deriva editable independiente.
- La compensación de formantes y la protección de consonantes son aproximadas. No garantizan timbre idéntico para cualquier desplazamiento.
- El editor permite mover pitch, no duración ni posición de notas. No hay undo/redo, selección múltiple, ARA, grabación de entrada, edición de tempo ni audio embebido.
- Los clips se limitan a 180 segundos. Sus buffers pueden ocupar cientos de MB durante un render.
- Si importas una frecuencia diferente de la sesión, la escucha CLIP usa interpolación lineal. La exportación conserva la frecuencia de origen.
- La escucha local PLAY requiere que el host siga llamando al procesador con el transporte detenido. Si el host suspende el plugin, usa HOST SYNC durante reproducción o la aplicación standalone.
- No hay cadena de mezcla vocal adicional, detector de tonalidad ni presets oficiales de artistas.
- Los presets están en código y en el menú del host; no hay gestor de presets de usuario ni archivos `.vstpreset` exportables.
- Las ediciones manuales siguen respetando CORRECTION y SPEED. Con CORRECTION en cero, tampoco se aplica el desplazamiento manual.
- Cambiar repetidamente parámetros de afinación en CLIP requiere nuevos renders. Mientras se calculan se escucha la versión anterior; por eso la automatización rápida debe usarse en LIVE.
- La latencia y las ventanas del motor no producen el mismo tipo de enganche instantáneo que todos los afinadores comerciales. Un preset de 1 ms no elimina esas ventanas.
- El medidor puede marcar clipping. No se incluye limitador y la exportación PCM puede saturar si OUTPUT es excesivo.

## Para aceptar una versión de producción

Ejecutar el workflow Windows, revisar el resultado, cargar el VST3 en FL Studio y completar `FL_STUDIO.md`. Añadir tomas masculinas graves reales con autorización y evaluar errores de octava, consonantes, vibrato y artefactos mediante escucha comparativa. La muestra cantada de JUCE usada en las pruebas es muy corta y no demuestra rendimiento general en ese registro.
