# Dependencias y fuentes correspondientes

Las fuentes originales del proyecto se entregan bajo AGPL-3.0-only (`LICENSE`). Las dependencias se descargan mediante CMake desde sus repositorios oficiales en commits fijos. No se incluye una licencia comercial de JUCE.

| Dependencia | Versión/commit | Aviso |
|---|---|---|
| JUCE | 8.0.13 / `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2` | AGPLv3 o licencia comercial; `licenses/JUCE-LICENSE.md` |
| Signalsmith Stretch | 1.3.2 / `88061fd4eda56a2353c5d65c74aa3f4ccf76f9da` | MIT; `licenses/Signalsmith-Stretch.txt` |
| Signalsmith Linear | 0.6.6 / `7bb1c8f36794d6980080ce817c91f8da8fe56a66` | MIT; `licenses/Signalsmith-Linear.txt` |

Fuentes exactas:

- https://github.com/juce-framework/JUCE/tree/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2
- https://github.com/Signalsmith-Audio/signalsmith-stretch/tree/88061fd4eda56a2353c5d65c74aa3f4ccf76f9da
- https://github.com/Signalsmith-Audio/linear/tree/7bb1c8f36794d6980080ce817c91f8da8fe56a66

JUCE contiene el SDK VST3 y otros componentes con avisos propios en su árbol de fuentes. CMake conserva esos archivos al descargarlo. La configuración no requiere un SDK VST2, ASIO externo, ARA o una biblioteca de clonación vocal.

La prueba de integración utiliza `examples/Assets/singing.ogg` del árbol de JUCE descargado. Ese archivo no se copia a las fuentes ni a los binarios distribuidos por este proyecto. Los resultados de esa prueba no implican afiliación con JUCE ni con los artistas mencionados en nombres de presets.

Al distribuir binarios, acompáñalos de estas fuentes, de los scripts de compilación y de las fuentes y avisos de las dependencias correspondientes. El repositorio descargado por CMake identifica los avisos adicionales de cada módulo.
