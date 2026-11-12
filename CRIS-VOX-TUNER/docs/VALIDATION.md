# Validación local — 11 de noviembre de 2026

## Entorno y alcance

Linux x86_64, Debian 13, GCC 14.2, CMake 4.4.4, build Release, JUCE 8.0.13 y las revisiones de Signalsmith fijadas en CMake. El contenedor es compartido: las mediciones no son un benchmark reproducible del ordenador del usuario ni del medidor de CPU de FL Studio.

## DSP

- Detección de tonos armónicos de 60, 82,4, 110, 196 y 440 Hz a 44,1/48/96 kHz: error dentro de 8 cents en los casos comprobados.
- Ruido blanco y silencio: comprobación de rechazo mediante umbrales de confianza/RMS.
- Corrección de una señal armónica de 112,246 Hz hacia 110 Hz: F0 estimada en la salida 110,237 Hz a 44,1 kHz y 110,298 Hz a 48/96 kHz.
- Pruebas adicionales a 61,735/82,407/195,998 Hz, con desafinación de +35 y −35 cents: errores residuales aproximados entre 2,9 y 8,1 cents en esos seis casos.
- Objetivo manual: la transposición de una nota se comprueba midiendo de nuevo el audio renderizado.
- Longitud y finitud de la salida; transparencia exacta de CORRECTION=0 en el render offline; cancelación de análisis.
- Conservación de un vibrato sintético de 5 Hz y ±25 cents con parámetros naturales.
- La prueba de impulso verifica que la ruta seca/bypass sale con la latencia declarada por el motor.
- Detección de asignaciones ordinarias y alineadas de C++ en el primer bloque preparado y durante procesamiento sostenido: ninguna en los bloques LIVE comprobados.

## CPU orientativa del motor LIVE

Una instancia estéreo, bloques de 128, 300 bloques medidos, cambio de MIX entre 50% y 100%, preset de corrección fuerte y compensación de formantes. Una pasada local produjo:

| Frecuencia | Audio procesado | Tiempo de cálculo | Fracción de un núcleo aproximada |
|---|---:|---:|---:|
| 44,1 kHz | 0,871 s | 14,4 ms | 1,7% |
| 48 kHz | 0,800 s | 12,4 ms | 1,6% |
| 96 kHz | 0,400 s | 10,9 ms | 2,7% |

Son tiempos medios de esa pasada, sin GUI ni coste del host. No se han certificado peores picos, sesiones grandes ni el rendimiento en Windows. No se promete “cero lag”. El informe bruto permite ver variaciones entre pasadas.

## Integración y muestra cantada real

La prueba carga `examples/Assets/singing.ogg` del JUCE fijado: 1,36025 segundos, dos notas segmentadas. Es una muestra real muy corta en un registro que no representa una validación de voces masculinas graves.

Con STRONG CORRECTION, el error mediano respecto a la nota cromática más cercana, evaluado por el mismo detector en tramas sostenidas, pasó aproximadamente de **43,02 a 5,40 cents**. Ese indicador es una medida interna: no contiene anotación F0 independiente, evaluación perceptual ni una garantía de timbre natural.

También se comprueban carga, edición manual, render, recuperación de parámetros/ruta/ediciones, exportación WAV con longitud exacta, reproducción de CLIP y capturas de la interfaz JUCE a 1180×820 y 960×740.

## Validación externa VST3

pluginval 1.0.4 para Linux, strictness level 5, con pantalla X11 virtual accesible. Resultado: **SUCCESS** en la ejecución registrada. Incluye escaneo, apertura fría/caliente, programas, editor, editor durante procesamiento, estados, parámetros automatizables, buses y procesamiento/automatización a 44,1/48/96 kHz con bloques 64/128/256/512/1024.

El validador oficial de Steinberg no se ejecutó: pluginval informa que no se configuró su ruta. Esta prueba no sustituye validación Windows ni FL Studio.

## Pendiente antes de producción

1. Ejecutar GitHub Actions en Windows/MSVC y revisar los artefactos.
2. Cargar ese VST3 en FL Studio y completar `FL_STUDIO.md`.
3. Añadir grabaciones masculinas graves variadas, con permiso de uso, y evaluar errores de octava, tracking y consonantes.
4. Hacer escucha comparativa de formantes, vibrato y artefactos, además de las métricas internas.
5. Medir picos de CPU, varias instancias, exportación del host y sincronización CLIP bajo loops y saltos del transporte.

Los binarios locales son exclusivamente Linux x86_64. No se ha compilado, probado ni firmado un VST3 Windows en este entorno. El workflow incluido generará esos artefactos si completa sus pasos correctamente.
